/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "assembler_x86_64.h"

#include "gtest/gtest.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <functional>

namespace art {

struct NativeAssemblerResult {
  bool ok;
  std::string error_msg;
  std::string base_name;
  UniquePtr<std::vector<uint8_t>> code;
  uintptr_t length;
};

static std::string get_assembler_command() {
  // TODO: Generalize this.
  return "/usr/bin/as";
}

// Compile the assembly file from_file to a binary file to_file. Returns true on success.
static bool compile(std::string from_file, std::string to_file) {
  std::string error_msg;
  std::vector<std::string> args;

  args.push_back(get_assembler_command());
  args.push_back("-o");
  args.push_back(to_file);
  args.push_back(from_file);

  return Exec(args, &error_msg);
}

static std::string get_objdump_command() {
  // TODO: Generalize this.
  return "/usr/bin/objdump -h";
}

// Runs objdump -h on the binary file and extracts the first line with .text. Returns "" on failure.
static std::string objdump(std::string file) {
  std::string error_msg;
  std::vector<std::string> args;

  args.push_back(get_objdump_command());
  args.push_back(file);
  args.push_back(">");
  args.push_back(file+".dump");
  std::string cmd = Join(args, ' ');

  args.clear();
  args.push_back("/bin/sh");
  args.push_back("-c");
  args.push_back(cmd);

  if (!Exec(args, &error_msg)) {
    EXPECT_TRUE(false) << error_msg;
  }

  std::ifstream dump(file+".dump");

  std::string line;
  bool found = false;
  while (std::getline(dump, line)) {
    if (line.find(".text") != line.npos) {
      found = true;
      break;
    }
  }

  dump.close();

  if (found) {
    return line;
  } else {
    return "";
  }
}

static bool compile(NativeAssemblerResult& res, std::string& assembly) {
  using std::ofstream;
  using std::string;
  using std::istringstream;
  using std::vector;
  using std::istream_iterator;
  using std::ifstream;

  res.ok = false;
  res.code.reset(nullptr);

  res.base_name = tmpnam(nullptr);

  // TODO: Lots of error checking.

  ofstream s_out(res.base_name + ".S");
  s_out << assembly;
  s_out.close();

  if (!compile(res.base_name + ".S", res.base_name + ".o")) {
    res.error_msg = "Could not compile.";
    return false;
  }

  string odump = objdump(res.base_name + ".o");
  if (odump.length() == 0) {
    res.error_msg = "Objdump failed.";
    return false;
  }

  istringstream iss(odump);
  istream_iterator<string> start(iss);
  istream_iterator<string> end;
  vector<string> tokens(start, end);

  if (tokens.size() < 6) {
    res.error_msg = "Objdump output not recognized: too few tokens.";
    return false;
  }

  if (tokens[1] != ".text") {
    res.error_msg = "Objdump output not recognized: .text not second token.";
    return false;
  }

  string lengthToken = "0x" + tokens[2];
  istringstream(lengthToken) >> std::hex >> res.length;

  string offsetToken = "0x" + tokens[5];
  uintptr_t offset;
  istringstream(offsetToken) >> std::hex >> offset;

  ifstream obj(res.base_name + ".o");
  obj.seekg(offset);
  res.code.reset(new std::vector<uint8_t>(res.length));
  obj.read(reinterpret_cast<char*>(&(*res.code)[0]), res.length);
  obj.close();

  res.ok = true;
  return true;
}

// Deletes all temporary files that could have been created when compiling.
static void clean(NativeAssemblerResult& res) {
  std::remove((res.base_name+".S").c_str());
  std::remove((res.base_name+".o").c_str());
  std::remove((res.base_name+".o.dump").c_str());
}

TEST(AssemblerX86_64, CreateBuffer) {
  AssemblerBuffer buffer;
  AssemblerBuffer::EnsureCapacity ensured(&buffer);
  buffer.Emit<uint8_t>(0x42);
  ASSERT_EQ(static_cast<size_t>(1), buffer.Size());
  buffer.Emit<int32_t>(42);
  ASSERT_EQ(static_cast<size_t>(5), buffer.Size());
}

static void driver(std::function<std::string (x86_64::X86_64Assembler&)> f) {
  x86_64::X86_64Assembler assembler;
  std::string assembly_text = f(assembler);

  EXPECT_GT(assembly_text.length(), 0U) << "Empty assembly";

  NativeAssemblerResult res;
  compile(res, assembly_text);

  clean(res);

  EXPECT_TRUE(res.ok) << res.error_msg;

  size_t cs = assembler.CodeSize();
  UniquePtr<std::vector<uint8_t> > entry_stub(new std::vector<uint8_t>(cs));
  MemoryRegion code(&(*entry_stub)[0], entry_stub->size());
  assembler.FinalizeInstructions(code);

  // Compare the two buffers.
  EXPECT_EQ(*entry_stub, *res.code) << "Outputs not identical";
}

static std::string pushq_test(x86_64::X86_64Assembler& assembler) {
  assembler.pushq(x86_64::RAX);
  assembler.pushq(x86_64::RBX);
  assembler.pushq(x86_64::RCX);

  return "pushq %rax\npushq %rbx\npushq %rcx\n";
}

TEST(AssemblerX86_64, SimpleTest) {
  driver(std::function<std::string (x86_64::X86_64Assembler&)>(pushq_test));
}

}  // namespace art
