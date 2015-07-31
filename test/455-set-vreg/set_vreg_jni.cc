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

#include "arch/context.h"
#include "art_method-inl.h"
#include "jni.h"
#include "scoped_thread_state_change.h"
#include "stack.h"
#include "thread.h"

namespace art {

namespace {

class TestVisitor : public StackVisitor {
 public:
  TestVisitor(Thread* thread, Context* context, mirror::Object* this_value)
      SHARED_REQUIRES(Locks::mutator_lock_)
      : StackVisitor(thread, context, StackVisitor::StackWalkKind::kIncludeInlinedFrames),
        this_value_(this_value) {}

  bool VisitFrame() SHARED_REQUIRES(Locks::mutator_lock_) {
    ArtMethod* m = GetMethod();
    std::string m_name(m->GetName());

    if (m_name.compare("testIntVReg") == 0) {
      uint32_t value = 0;
      CHECK(GetVReg(m, 1, kReferenceVReg, &value));
      CHECK_EQ(reinterpret_cast<mirror::Object*>(value), this_value_);

      CheckSetVReg(m, 2, 5, kIntVReg);
      CheckSetVReg(m, 3, 4, kIntVReg);
      CheckSetVReg(m, 4, 3, kIntVReg);
      CheckSetVReg(m, 5, 2, kIntVReg);
      CheckSetVReg(m, 6, 1, kIntVReg);
    } else if (m_name.compare("testLongVReg") == 0) {
      uint32_t value = 0;
      CHECK(GetVReg(m, 3, kReferenceVReg, &value));
      CHECK_EQ(reinterpret_cast<mirror::Object*>(value), this_value_);

      CheckSetVRegPair(m, 4, std::numeric_limits<int64_t>::max(), kLongLoVReg, kLongHiVReg);
      CheckSetVRegPair(m, 6, 4, kLongLoVReg, kLongHiVReg);
      CheckSetVRegPair(m, 8, 3, kLongLoVReg, kLongHiVReg);
      CheckSetVRegPair(m, 10, 2, kLongLoVReg, kLongHiVReg);
      CheckSetVRegPair(m, 12, 1, kLongLoVReg, kLongHiVReg);
    } else if (m_name.compare("testFloatVReg") == 0) {
      uint32_t value = 0;
      CHECK(GetVReg(m, 1, kReferenceVReg, &value));
      CHECK_EQ(reinterpret_cast<mirror::Object*>(value), this_value_);

      CheckSetVReg(m, 2, bit_cast<uint32_t, float>(5.0f), kFloatVReg);
      CheckSetVReg(m, 3, bit_cast<uint32_t, float>(4.0f), kFloatVReg);
      CheckSetVReg(m, 4, bit_cast<uint32_t, float>(3.0f), kFloatVReg);
      CheckSetVReg(m, 5, bit_cast<uint32_t, float>(2.0f), kFloatVReg);
      CheckSetVReg(m, 6, bit_cast<uint32_t, float>(1.0f), kFloatVReg);
    } else if (m_name.compare("testDoubleVReg") == 0) {
      uint32_t value = 0;
      CHECK(GetVReg(m, 3, kReferenceVReg, &value));
      CHECK_EQ(reinterpret_cast<mirror::Object*>(value), this_value_);

      CheckSetVRegPair(m, 4, bit_cast<uint64_t, double>(5.0), kDoubleLoVReg, kDoubleHiVReg);
      CheckSetVRegPair(m, 6, bit_cast<uint64_t, double>(4.0), kDoubleLoVReg, kDoubleHiVReg);
      CheckSetVRegPair(m, 8, bit_cast<uint64_t, double>(3.0), kDoubleLoVReg, kDoubleHiVReg);
      CheckSetVRegPair(m, 10, bit_cast<uint64_t, double>(2.0), kDoubleLoVReg, kDoubleHiVReg);
      CheckSetVRegPair(m, 12, bit_cast<uint64_t, double>(1.0), kDoubleLoVReg, kDoubleHiVReg);
    } else if (m_name.compare("testReferenceVReg") == 0) {
      uint32_t value = 0;
      CHECK(GetVReg(m, 1, kReferenceVReg, &value));
      CHECK_EQ(reinterpret_cast<mirror::Object*>(value), this_value_);

      CheckSetVReg(m, 2, value, kReferenceVReg);
      CheckSetVReg(m, 3, value, kReferenceVReg);
      CheckSetVReg(m, 4, value, kReferenceVReg);
      CheckSetVReg(m, 5, value, kReferenceVReg);
      CheckSetVReg(m, 6, value, kReferenceVReg);
    }

    return true;
  }

  void CheckSetVReg(ArtMethod* m, uint16_t vreg, uint32_t new_value, VRegKind kind)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    CHECK(SetVReg(m, vreg, new_value, kind));
    uint32_t actual_value;
    GetVReg(m, vreg, kind, &actual_value);
    CHECK_EQ(new_value, actual_value);
  }

  void CheckSetVRegPair(ArtMethod* m, uint16_t vreg, uint64_t new_value,
                        VRegKind kind_lo, VRegKind kind_hi)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    CHECK(SetVRegPair(m, vreg, new_value, kind_lo, kind_hi));
    uint64_t actual_value;
    GetVRegPair(m, vreg, kind_lo, kind_hi, &actual_value);
    CHECK_EQ(new_value, actual_value);
  }

  mirror::Object* this_value_;
};

extern "C" JNIEXPORT void JNICALL Java_Main_doNativeCallSetVReg(JNIEnv*, jobject value) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<Context> context(Context::Create());
  TestVisitor visitor(soa.Self(), context.get(), soa.Decode<mirror::Object*>(value));
  visitor.WalkStack();
}

}  // namespace

}  // namespace art
