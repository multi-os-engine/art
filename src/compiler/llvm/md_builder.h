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

#ifndef ART_SRC_COMPILER_LLVM_MD_BUILDER_H_
#define ART_SRC_COMPILER_LLVM_MD_BUILDER_H_

#include "llvm/IR/MDBuilder.h"
#include "primitive.h"
#include "safe_map.h"

namespace llvm {
  class LLVMContext;
  class MDNode;
}  // namespace llvm

namespace art {
namespace compiler {
namespace Llvm {

typedef llvm::MDBuilder LlvmMDBuilder;

// Abstract parent of all ART meta-data builders. An abstraction is provided so that the compile
// time and runtime performance of having meta-data information can be evaluated.
class ArtMDBuilder : public LlvmMDBuilder {
 public:
  ArtMDBuilder(llvm::LLVMContext& context) : LlvmMDBuilder(context) {}
  virtual ~ArtMDBuilder() {}

  // Returns a branch weight indicating we do expect the branch to be taken to the "true"
  // successor.
  virtual llvm::MDNode* BranchLikely() = 0;

  // Returns a branch weight indicating we don't expect the branch to be taken to the "true"
  // successor.
  virtual llvm::MDNode* BranchUnlikely() = 0;

  // Returns a TBAA node for a alloca-ed variable with a stack frame. The parameter indicates
  // whether the variable can escape the frame.
  virtual llvm::MDNode* GetTbaaForRandomAllocaVariable(bool can_escape) = 0;

  // Get a TBAA node for the dex_pc_ field within the shadow frame.
  virtual llvm::MDNode* GetTbaaForShadowFrameDexPc() = 0;

  // Get a TBAA node for the method_ field within the shadow frame.
  virtual llvm::MDNode* GetTbaaForShadowFrameMethod() = 0;

  // Get a TBAA node for the number_of_vregs_ field within the shadow frame.
  virtual llvm::MDNode* GetTbaaForShadowFrameNumberOfVRegs() = 0;

  // Get a TBAA node for the link_ field within the shadow frame.
  virtual llvm::MDNode* GetTbaaForShadowFrameLink() = 0;

  // Get a TBAA node for a vreg field within the shadow frame.
  virtual llvm::MDNode* GetTbaaForShadowFrameVReg() = 0;

  // Get a TBAA node for a Thread access in the C heap.
  virtual llvm::MDNode* GetTbaaForThread() = 0;

  // Get a TBAA node for a card table access.
  virtual llvm::MDNode* GetTbaaForCardTable() = 0;

  // Get a TBAA node associated with array lengths.
  virtual llvm::MDNode* GetTbaaForArrayLength() = 0;

  // Get a TBAA node for the array access.
  virtual llvm::MDNode* GetTbaaForArrayAccess(Primitive::Type elem_type,
                                              const char* elem_type_descriptor, bool is_const) = 0;

  // Get a TBAA node for the given instance field.
  virtual llvm::MDNode* GetTbaaForInstanceField(Primitive::Type type, const char* type_descriptor,
                                                const char* class_name, const char* field_name,
                                                bool is_const) = 0;

  // Get a TBAA node for the given static field.
  virtual llvm::MDNode* GetTbaaForStaticField(Primitive::Type type, const char* type_descriptor,
                                              const char* class_name, const char* field_name,
                                              bool is_const) = 0;
};

// A meta-data builder that attempts to disambiguate memory accesses.
class ExactArtMDBuilder : public ArtMDBuilder {
 public:
  ExactArtMDBuilder(llvm::LLVMContext& context)
     : ArtMDBuilder(context),
       branch_likely_(createBranchWeights(64, 4)),  // Weights match what clang generates.
       branch_unlikely_(createBranchWeights(4, 64)),  // Weights match what clang generates.
       tbaa_root_(createTBAARoot("memory")),
       stack_(createTBAANode("stack", tbaa_root_)),
       c_heap_(createTBAANode("C heap", tbaa_root_)),
       java_heap_(createTBAANode("Java heap", tbaa_root_)),
       java_array_length_heap_(createTBAANode("Java array length heap", tbaa_root_, true))
       {}

  // Returns a branch weight indicating we do expect the branch to be taken to the "true"
  // successor.
  virtual llvm::MDNode* BranchLikely() {
    return branch_likely_;
  }

  // Returns a branch weight indicating we don't expect the branch to be taken to the "true"
  // successor.
  virtual llvm::MDNode* BranchUnlikely() {
    return branch_unlikely_;
  }

  // Returns a TBAA node for a alloca-ed variable with a stack frame. The parameter indicates
  // whether the variable can escape the frame.
  virtual llvm::MDNode* GetTbaaForRandomAllocaVariable(bool can_escape) {
    if (can_escape) {
      return stack_;
    }
    return NULL;  // No TBAA node necessary.
  }

  // Get a TBAA node for the dex_pc_ field within the shadow frame.
  virtual llvm::MDNode* GetTbaaForShadowFrameDexPc() {
    // TODO: divide heaps associated with shadow frame and its fields?
    return GetTbaaForRandomAllocaVariable(true);
  }

  // Get a TBAA node for a vreg field within the shadow frame.
  virtual llvm::MDNode* GetTbaaForShadowFrameVReg() {
    // TODO: divide heaps associated with shadow frame and its fields?
    return GetTbaaForRandomAllocaVariable(true);
  }

  // Get a TBAA node for the method_ field within the shadow frame.
  virtual llvm::MDNode* GetTbaaForShadowFrameMethod() {
    // TODO: const? divide heaps associated with shadow frame and its fields?
    return GetTbaaForRandomAllocaVariable(true);
  }

  // Get a TBAA node for the number_of_vregs_ field within the shadow frame.
  virtual llvm::MDNode* GetTbaaForShadowFrameNumberOfVRegs() {
    // TODO: const? divide heaps associated with shadow frame and its fields?
    return GetTbaaForRandomAllocaVariable(true);
  }

  // Get a TBAA node for the link_ field within the shadow frame.
  virtual llvm::MDNode* GetTbaaForShadowFrameLink() {
    // TODO: const? divide heaps associated with shadow frame and its fields?
    return GetTbaaForRandomAllocaVariable(true);
  }

  // Get a TBAA node for a Thread access in the C heap.
  virtual llvm::MDNode* GetTbaaForThread() {
    return c_heap_;
  }

  // Get a TBAA node for a card table access.
  virtual llvm::MDNode* GetTbaaForCardTable() {
    return c_heap_;
  }

  // Get a TBAA node associated with array lengths.
  virtual llvm::MDNode* GetTbaaForArrayLength() {
    return java_array_length_heap_;
  }

  // Get a TBAA node for the array access.
  virtual llvm::MDNode* GetTbaaForArrayAccess(Primitive::Type elem_type,
                                              const char* elem_type_descriptor,
                                              bool is_const) {
    return GetTbaaForField(java_heap_, &java_heap_array_, &java_heap_array_const_,
                           "Java array heap", elem_type, elem_type_descriptor,
                           NULL, NULL, is_const);
  }

  // Get a TBAA node for the given field.
  virtual llvm::MDNode* GetTbaaForInstanceField(Primitive::Type type, const char* type_descriptor,
                                                const char* class_name, const char* field_name,
                                                bool is_const) {
    return GetTbaaForField(java_heap_, &java_heap_instance_, &java_heap_instance_const_,
                           "Java instance heap", type, type_descriptor,
                           class_name, field_name, is_const);
  }

  // Get a TBAA node for the given field.
  virtual llvm::MDNode* GetTbaaForStaticField(Primitive::Type type, const char* type_descriptor,
                                              const char* class_name, const char* field_name,
                                              bool is_const) {
    return GetTbaaForField(java_heap_, &java_heap_static_, &java_heap_static_const_,
                           "Java static heap", type, type_descriptor,
                           class_name, field_name, is_const);
  }

 private:
  llvm::MDNode* GetTbaaForField(llvm::MDNode* tbaa_root,
                                SafeMap<Primitive::Type, llvm::MDNode*>* non_const_map,
                                SafeMap<Primitive::Type, llvm::MDNode*>* const_map,
                                const char* heap_name,
                                Primitive::Type type, const char* type_descriptor,
                                const char* class_name, const char* field_name,
                                bool is_const);

  llvm::MDNode* const branch_likely_;  // Branch that's predicted taken.
  llvm::MDNode* const branch_unlikely_;  // Branch that's predicted not-taken.

  llvm::MDNode* const tbaa_root_;  // "heap" the common parent of all our heap types.
  llvm::MDNode* const stack_;      // The parent of all memory accesses into the stack.
  llvm::MDNode* const c_heap_;     // The parent of all memory accesses into the C heap.
  // The parent of all memory accesses into the Java heap, descendants are arrays, instance fields
  // and class fields.
  llvm::MDNode* const java_heap_;
  // Memory access meta-data representing accesses to array lengths.
  llvm::MDNode* const java_array_length_heap_;
  // Memory access meta-data representing array accesses within the Java heap.
  SafeMap<Primitive::Type, llvm::MDNode*> java_heap_array_;
  // Memory access meta-data representing instance field accesses within the Java heap.
  SafeMap<Primitive::Type, llvm::MDNode*> java_heap_instance_;
  // Memory access meta-data representing instance field accesses within the Java heap.
  SafeMap<Primitive::Type, llvm::MDNode*> java_heap_static_;
  // Memory access meta-data representing immutable array accesses within the Java heap. Const
  // array aliasing is a descendant of array aliasing, meaning stores to arrays that are const
  // will alias with loads from those arrays. Const arrays are regarded as "pointers to constant
  // memory".
  SafeMap<Primitive::Type, llvm::MDNode*> java_heap_array_const_;
  // Memory access meta-data representing final instance field accesses within the Java heap. Const
  // instance field aliasing is a descendant of instance field aliasing, meaning stores to instance
  // fields that are const will alias with loads from those fields. The final instance fields are
  // regarded as "pointers to constant memory".
  SafeMap<Primitive::Type, llvm::MDNode*> java_heap_instance_const_;
  // Memory access meta-data representing final static field accesses within the Java heap. Const
  // static field aliasing is a descendant of static field aliasing, meaning stores to static
  // fields that are const will alias with loads from those fields. The final static fields are
  // regarded as "pointers to constant memory".
  SafeMap<Primitive::Type, llvm::MDNode*> java_heap_static_const_;
};

}  // namespace Llvm
}  // namespace compiler
}  // namespace art

#endif // ART_SRC_COMPILER_LLVM_MD_BUILDER_H_
