/*
 * Copyright (C) 2012 The Android Open Source Project
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


#include "md_builder.h"

#include "base/stringprintf.h"

namespace art {
namespace compiler {
namespace Llvm {

llvm::MDNode* ExactArtMDBuilder::GetTbaaForField(llvm::MDNode* tbaa_root,
                                                 SafeMap<Primitive::Type, llvm::MDNode*>* non_const_map,
                                                 SafeMap<Primitive::Type, llvm::MDNode*>* const_map,
                                                 const char* heap_name,
                                                 Primitive::Type type, const char* type_descriptor,
                                                 const char* class_name, const char* field_name,
                                                 bool is_const) {
  // TODO: we can be smarter here and decide that differently named fields in different classes
  //       don't alias. We can also pass through better type information for reference fields.
  ::llvm::MDNode* result = is_const ? const_map->Get(type) : non_const_map->Get(type);
  if (result == NULL) {
    ::llvm::MDNode* parent = tbaa_root;
    if (is_const) {
      parent = GetTbaaForField(tbaa_root, non_const_map, const_map, heap_name,
                               type, type_descriptor, class_name, field_name, false);
    }
    std::string name(StringPrintf("%s %s %s",
                                  is_const ? "const" : "non-const",
                                  heap_name,
                                  Primitive::Descriptor(type)));
    result = createTBAANode(name, parent, is_const);
    if (is_const) {
      const_map->Put(type, result);
    } else {
      non_const_map->Put(type, result);
    }
  }
  return result;
}

}  // namespace Llvm
}  // namespace compiler
}  // namespace art

