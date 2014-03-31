/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_COMPILER_UTILS_ASSEMBLER_TEST_H_
#define ART_COMPILER_UTILS_ASSEMBLER_TEST_H_

#include "assembler.h"

#include "gtest/gtest.h"

#include <cstdio>
#include <fstream>
#include <iostream>

namespace art {

template<typename T>
class AssemblerTest : public testing::Test {
 public:
  T& GetAssembler() {
    return *assembler_;
  }

  typedef const char* (*TestFn)(T* assembler);

  void Driver(TestFn f) {
    const char* assembly_text = f(assembler_.get());

    EXPECT_NE(assembly_text, nullptr) << "Empty assembly";

    NativeAssemblerResult res;
    Compile(assembly_text, &res);


    EXPECT_TRUE(res.ok) << res.error_msg;

    size_t cs = assembler_->CodeSize();
    UniquePtr<std::vector<uint8_t> > data(new std::vector<uint8_t>(cs));
    MemoryRegion code(&(*data)[0], data->size());
    assembler_->FinalizeInstructions(code);

    // Compare the two buffers.
    EXPECT_EQ(*data, *res.code) << "Outputs not identical";

    Clean(&res);
  }

 protected:
  void SetUp() OVERRIDE {
    assembler_.reset(new T());
  }

  // Return the host assembler command for this test.
  virtual const char* GetAssemblerCommand() = 0;

  // Return the host objdump command for this test.
  virtual const char* GetObjdumpCommand() = 0;

 private:
  // Structure to store intermediates and results.
  struct NativeAssemblerResult {
    bool ok;
    std::string error_msg;
    std::string base_name;
    UniquePtr<std::vector<uint8_t>> code;
    uintptr_t length;
  };

  // Compile the assembly file from_file to a binary file to_file. Returns true on success.
  bool Assemble(const char* from_file, const char* to_file, std::string* error_msg) {
    std::vector<std::string> args;

    args.push_back(GetAssemblerCommand());
    args.push_back("-o");
    args.push_back(to_file);
    args.push_back(from_file);

    return Exec(args, error_msg);
  }

  // Runs objdump -h on the binary file and extracts the first line with .text.
  // Returns "" on failure.
  std::string Objdump(std::string file) {
    std::string error_msg;
    std::vector<std::string> args;

    args.push_back(GetObjdumpCommand());
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

  // Compile the given assembly code and extract the binary, if possible. Put result into res.
  bool Compile(const char* assembly_code, NativeAssemblerResult* res) {
    using std::ofstream;
    using std::string;
    using std::istringstream;
    using std::vector;
    using std::istream_iterator;
    using std::ifstream;

    res->ok = false;
    res->code.reset(nullptr);

    res->base_name = tmpnam(nullptr);

    // TODO: Lots of error checking.

    ofstream s_out(res->base_name + ".S");
    s_out << assembly_code;
    s_out.close();

    if (!Assemble((res->base_name + ".S").c_str(), (res->base_name + ".o").c_str(),
                  &res->error_msg)) {
      res->error_msg = "Could not compile.";
      return false;
    }

    string odump = Objdump(res->base_name + ".o");
    if (odump.length() == 0) {
      res->error_msg = "Objdump failed.";
      return false;
    }

    istringstream iss(odump);
    istream_iterator<string> start(iss);
    istream_iterator<string> end;
    vector<string> tokens(start, end);

    if (tokens.size() < 6) {
      res->error_msg = "Objdump output not recognized: too few tokens.";
      return false;
    }

    if (tokens[1] != ".text") {
      res->error_msg = "Objdump output not recognized: .text not second token.";
      return false;
    }

    string lengthToken = "0x" + tokens[2];
    istringstream(lengthToken) >> std::hex >> res->length;

    string offsetToken = "0x" + tokens[5];
    uintptr_t offset;
    istringstream(offsetToken) >> std::hex >> offset;

    ifstream obj(res->base_name + ".o");
    obj.seekg(offset);
    res->code.reset(new std::vector<uint8_t>(res->length));
    obj.read(reinterpret_cast<char*>(&(*res->code)[0]), res->length);
    obj.close();

    res->ok = true;
    return true;
  }

  void Clean(const NativeAssemblerResult* res) {
    std::remove((res->base_name + ".S").c_str());
    std::remove((res->base_name + ".o").c_str());
    std::remove((res->base_name + ".o.dump").c_str());
  }

  UniquePtr<T> assembler_;
};

}  // namespace art

#endif  // ART_COMPILER_UTILS_ASSEMBLER_TEST_H_
