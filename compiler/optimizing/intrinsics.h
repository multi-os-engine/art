/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_INTRINSICS_H_
#define ART_COMPILER_OPTIMIZING_INTRINSICS_H_

#include "nodes.h"
#include "optimization.h"

namespace art {

class CompilerDriver;
class DexFile;

// Check HInvoke nodes whether they represent intrinsifiable functions, and set the intrinsic_
// field accordingly.
class IntrinsicsRecognizer : public HOptimization {
 public:
  IntrinsicsRecognizer(HGraph* graph, const DexFile* dex_file, CompilerDriver* driver)
      : HOptimization(graph, true, "intrinsics_recognition"),
        dex_file_(dex_file), driver_(driver) {}

  void Run() OVERRIDE;

 private:
  const DexFile* dex_file_;
  CompilerDriver* driver_;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicsRecognizer);
};

class IntrinsicVisitor : public ValueObject {
 public:
  virtual ~IntrinsicVisitor() {}

  bool Dispatch(HInvoke* invoke) {
    if (invoke->GetIntrinsic() == Intrinsics::kNone) {
      return false;
    }

    if (invoke->IsInvokeStaticOrDirect()) {
      return Dispatch(invoke->AsInvokeStaticOrDirect());
    } else if (invoke->IsInvokeVirtual()) {
      return Dispatch(invoke->AsInvokeVirtual());
    }
    return false;
  }

  bool Dispatch(HInvokeStaticOrDirect* invoke) {
    switch (invoke->GetIntrinsic()) {
#define OPTIMIZING_INTRINSICS(Name) \
      case Intrinsics::k ## Name: \
        return Visit ## Name(invoke);
#include "intrinsics_list.h"
STATIC_INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef STATIC_INTRINSICS_LIST
#undef VIRTUAL_INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS

      default:
        break;
    }

    return false;
  }

  bool Dispatch(HInvokeVirtual* invoke) {
    switch (invoke->GetIntrinsic()) {
#define OPTIMIZING_INTRINSICS(Name) \
      case Intrinsics::k ## Name: \
        return Visit ## Name(invoke);
#include "intrinsics_list.h"
VIRTUAL_INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef STATIC_INTRINSICS_LIST
#undef VIRTUAL_INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS

      default:
        break;
    }

    return false;
  }

#define OPTIMIZING_INTRINSICS(Name) \
  virtual bool Visit ## Name(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) { \
    return false; \
  }
#include "intrinsics_list.h"
STATIC_INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef STATIC_INTRINSICS_LIST
#undef VIRTUAL_INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS

#define OPTIMIZING_INTRINSICS(Name) \
  virtual bool Visit ## Name(HInvokeVirtual* invoke ATTRIBUTE_UNUSED) { \
    return false; \
  }
#include "intrinsics_list.h"
VIRTUAL_INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef STATIC_INTRINSICS_LIST
#undef VIRTUAL_INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS

  // TODO: Unsafe, Reference.

 protected:
  IntrinsicVisitor() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(IntrinsicVisitor);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INTRINSICS_H_
