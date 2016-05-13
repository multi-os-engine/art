/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <unistd.h>
#include "art/oatdump/oatmap.pb.h"
#include "emit_oatmap.h"

class OatMapBuilderImpl {
 public:
  OatMapBuilderImpl(uint32_t adler32_checksum, uint64_t base_code_addr) {
    proto.set_adler32_checksum(adler32_checksum);
    proto.set_base_code_addr(base_code_addr);
  }

  void AddDexFile(const std::string &sha1sig) {
    auto dexfile = proto.add_dexfiles();
    dexfile->set_sha1signature(sha1sig);
    current_dexfile = dexfile;
  }

  void AddClass(uint32_t class_def_index) {
    assert(current_dexfile);
    auto dexclass = current_dexfile->add_classes();
    dexclass->set_classindex(class_def_index);
    current_dexclass = dexclass;
  }

  void AddMethod(uint32_t method_index, uint32_t code_start_offset,
                 uint32_t code_size) {
    assert(current_dexclass);
    auto dexmethod = current_dexclass->add_methods();
    dexmethod->set_mindex(method_index);
    dexmethod->set_mstart(code_start_offset);
    dexmethod->set_msize(code_size);
  }

  bool EmitToFile(int file_descriptor) {
    int size = proto.ByteSize();
    std::string data;
    data.resize(size);
    ::google::protobuf::uint8 *dtarget =
        reinterpret_cast<::google::protobuf::uint8 *>(string_as_array(&data));
    proto.SerializeWithCachedSizesToArray(dtarget);
    size_t fsiz = size;
    ssize_t esiz = size;
    if (write(file_descriptor, dtarget, fsiz) != esiz) {
      return false;
    }
    return true;
  }

 private:
  inline char *string_as_array(std::string *str) {
    return str->empty() ? NULL : &*str->begin();
  }

  oatmap::OatFile proto;
  oatmap::DexFile *current_dexfile;
  oatmap::DexClass *current_dexclass;
};

//======================================================================

OatMapBuilder::OatMapBuilder(uint32_t adler32_checksum, uint64_t base_code_addr)
    : impl(new OatMapBuilderImpl(adler32_checksum, base_code_addr)) {}

OatMapBuilder::~OatMapBuilder() {}

void OatMapBuilder::AddDexFile(const std::string &sha1sig) {
  impl->AddDexFile(sha1sig);
}

void OatMapBuilder::AddClass(uint32_t class_def_index) {
  impl->AddClass(class_def_index);
}

void OatMapBuilder::AddMethod(uint32_t method_index, uint32_t code_start_offset,
                              uint32_t code_size) {
  impl->AddMethod(method_index, code_start_offset, code_size);
}

bool OatMapBuilder::EmitToFile(int file_descriptor) {
  return impl->EmitToFile(file_descriptor);
}
