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

#include "interpreter/interpreter_common.h"
#include "base/hex_dump.h"

namespace art {
namespace arm {

#define DEBUG_LOGS 0

//
// Entry points from assembly language to the interpreter mangled functions
//

extern "C" {
void CheckSuspendFromAsm(Thread* thread) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  CheckSuspend(thread);
}

String* ResolveStringFromAsm(Thread* self, MethodHelper* mh, uint32_t string_idx) {
#if DEBUG_LOGS
  LOG(INFO) << "ResolveStringFromAsm(" << string_idx << ")";
#endif
  return interpreter::ResolveString(self, *mh, string_idx);
}

bool DoIntDivideFromAsm(ShadowFrame* shadow_frame, size_t result_reg,
                              uint32_t dividend, uint32_t divisor) {
  return interpreter::DoIntDivide(*shadow_frame, result_reg, dividend, divisor);
}

bool DoIntRemainderFromAsm(ShadowFrame* shadow_frame, size_t result_reg,
                                  uint32_t dividend, uint32_t divisor) {
  return interpreter::DoIntRemainder(*shadow_frame, result_reg, dividend, divisor);
}

bool DoLongDivideFromAsm(ShadowFrame* shadow_frame, size_t result_reg,
                                uint32_t dividend_lo, uint32_t dividend_hi,
                                uint32_t divisor_lo, uint32_t divisor_hi) {
  int64_t dividend = static_cast<int64_t>(dividend_hi) << 32LL | dividend_lo;
  int64_t divisor = static_cast<int64_t>(divisor_hi) << 32LL | divisor_lo;
  return interpreter::DoLongDivide(*shadow_frame, result_reg, dividend, divisor);
}

bool DoLongRemainderFromAsm(ShadowFrame* shadow_frame, size_t result_reg,
                            uint32_t dividend_lo, uint32_t dividend_hi,
                            uint32_t divisor_lo, uint32_t divisor_hi) {
  int64_t dividend = static_cast<int64_t>(dividend_hi) << 32LL | dividend_lo;
  int64_t divisor = static_cast<int64_t>(divisor_hi) << 32LL | divisor_lo;
  return interpreter::DoLongRemainder(*shadow_frame, result_reg, dividend, divisor);
}

void DoMonitorEnterFromAsm(Thread* self, Object* ref) NO_THREAD_SAFETY_ANALYSIS {
  interpreter::DoMonitorEnter(self, ref);
}

void DoMonitorExitFromAsm(Thread* self, Object* ref) NO_THREAD_SAFETY_ANALYSIS {
  interpreter::DoMonitorExit(self, ref);
}


ArtMethod* FindVirtualMethodFromAsm(uint32_t method_idx, mirror::Object* this_object,
                       mirror::ArtMethod* referrer, Thread* self) {
#if DEBUG_LOGS
  LOG(INFO) << "FindMethodFromAsm(" << method_idx << ")";
#endif
  ArtMethod* result = FindMethodFromCode<kVirtual, false>(method_idx, &this_object,
                                                                          &referrer,
                                                                          self);
  return result;
}

ArtMethod* FindDirectMethodFromAsm(uint32_t method_idx, mirror::Object* this_object,
                       mirror::ArtMethod* referrer, Thread* self) {
#if DEBUG_LOGS
  LOG(INFO) << "FindMethodFromAsm(" << method_idx << ")";
#endif
  ArtMethod* result = FindMethodFromCode<kDirect, false>(method_idx, &this_object,
                                                         &referrer,
                                                         self);
  return result;
}

ArtMethod* FindSuperMethodFromAsm(uint32_t method_idx, mirror::Object* this_object,
                       mirror::ArtMethod* referrer, Thread* self) {
#if DEBUG_LOGS
  LOG(INFO) << "FindMethodFromAsm(" << method_idx << ")";
#endif
  ArtMethod* result = FindMethodFromCode<kSuper, false>(method_idx, &this_object,
                                                        &referrer,
                                                        self);
  return result;
}

ArtMethod* FindInterfaceMethodFromAsm(uint32_t method_idx, mirror::Object* this_object,
                       mirror::ArtMethod* referrer, Thread* self) {
#if DEBUG_LOGS
  LOG(INFO) << "FindMethodFromAsm(" << method_idx << ")";
#endif
  ArtMethod* result = FindMethodFromCode<kInterface, false>(method_idx, &this_object,
                                                            &referrer,
                                                            self);
  return result;
}

ArtMethod* FindStaticMethodFromAsm(uint32_t method_idx, mirror::Object* this_object,
                       mirror::ArtMethod* referrer, Thread* self) {
#if DEBUG_LOGS
  LOG(INFO) << "FindMethodFromAsm(" << method_idx << ")";
#endif
  ArtMethod* result = FindMethodFromCode<kStatic, false>(method_idx, nullptr,
                                                         &referrer,
                                                         self);
  return result;
}


// TODO: this could be inlined into assembly for speed
ArtMethod* QuickMethodFromAsm(uint32_t vtable_idx, mirror::Object* receiver) {
  ArtMethod* method = receiver->GetClass()->GetVTable()->GetWithoutChecks(vtable_idx);
  return method;
}


bool DoCallFromAsm(ArtMethod* method, Object* receiver, Thread* self, ShadowFrame* shadow_frame,
            uint16_t* instaddr, JValue* result) {
#if DEBUG_LOGS
  LOG(INFO) << "DoCallFromAsm";
#endif
  //  Build an Instruction* from the address of the dex instruction.
  const Instruction* inst = reinterpret_cast<Instruction*>(instaddr);
  uint16_t inst_data = inst->Fetch16(0);
  return interpreter::DoCall<false, false>(method, self, *shadow_frame, inst,
      inst_data, result);
}

bool DoCallRangeFromAsm(ArtMethod* method, Object* receiver, Thread* self, ShadowFrame* shadow_frame,
            uint16_t* instaddr, JValue* result) {
  //  Build an Instruction* from the address of the dex instruction.
  const Instruction* inst = reinterpret_cast<Instruction*>(instaddr);
  uint16_t inst_data = inst->Fetch16(0);
  return interpreter::DoCall<true, false>(method, self, *shadow_frame, inst,
        inst_data, result);
}

mirror::Object* AllocObjectFromAsm(uint32_t type_idx, mirror::ArtMethod* method, Thread* self)
    NO_THREAD_SAFETY_ANALYSIS {
#if DEBUG_LOGS
  LOG(INFO) << "AllocObjectFromAsm(" << type_idx << ")";
#endif
  mirror::Object* result = AllocObjectFromCode<false, true>(type_idx, method, self,
                                          Runtime::Current()->GetHeap()->GetCurrentAllocator());
#if DEBUG_LOGS
  LOG(INFO) << "object allocated: " << result;
#endif
  return result;
}

mirror::Array* AllocArrayFromAsm(uint32_t type_idx, mirror::ArtMethod* method, Thread* self,
                                  int32_t component_count)
    NO_THREAD_SAFETY_ANALYSIS {
#if DEBUG_LOGS
  LOG(INFO) << "allocating array with type " << type_idx << " and count " << component_count;
#endif
  mirror::Array* array =  AllocArrayFromCode<false, true>(type_idx, method, component_count, self,
                                          Runtime::Current()->GetHeap()->GetCurrentAllocator());
#if DEBUG_LOGS
  LOG(INFO) << "returning array at " << array;
#endif
  return array;
}

void PrintFromAsm(const char *s) {
  LOG(INFO) << s;
  // std::cout << s << "\n";
}


ArtField* FindFieldFromAsm_InstanceObjectRead(uint32_t field_idx, mirror::ArtMethod* method, Thread* self,
                           Primitive::Type field_type) {
  ArtField* result = nullptr;
  result = FindFieldFromCode<InstanceObjectRead, false>(field_idx, method, self,
                                                           Primitive::ComponentSize(field_type));
  return result;
}

ArtField* FindFieldFromAsm_InstanceObjectWrite(uint32_t field_idx, mirror::ArtMethod* method, Thread* self,
                           Primitive::Type field_type) {
  ArtField* result = nullptr;
  result = FindFieldFromCode<InstanceObjectWrite, false>(field_idx, method, self,
                                                           Primitive::ComponentSize(field_type));
  return result;
}

ArtField* FindFieldFromAsm_InstancePrimitiveRead(uint32_t field_idx, mirror::ArtMethod* method, Thread* self,
                           Primitive::Type field_type) {
  ArtField* result = nullptr;
  result = FindFieldFromCode<InstancePrimitiveRead, false>(field_idx, method, self,
                                                           Primitive::ComponentSize(field_type));
  return result;
}

ArtField* FindFieldFromAsm_InstancePrimitiveWrite(uint32_t field_idx, mirror::ArtMethod* method, Thread* self,
                           Primitive::Type field_type) {
  ArtField* result = nullptr;
  result = FindFieldFromCode<InstancePrimitiveWrite, false>(field_idx, method, self,
                                                           Primitive::ComponentSize(field_type));
  return result;
}

ArtField* FindFieldFromAsm_StaticObjectRead(uint32_t field_idx, mirror::ArtMethod* method, Thread* self,
                           Primitive::Type field_type) {
  ArtField* result = nullptr;
  result = FindFieldFromCode<StaticObjectRead, false>(field_idx, method, self,
                                                           Primitive::ComponentSize(field_type));
  return result;
}

ArtField* FindFieldFromAsm_StaticObjectWrite(uint32_t field_idx, mirror::ArtMethod* method, Thread* self,
                           Primitive::Type field_type) {
  ArtField* result = nullptr;
  result = FindFieldFromCode<StaticObjectWrite, false>(field_idx, method, self,
                                                           Primitive::ComponentSize(field_type));
  return result;
}

ArtField* FindFieldFromAsm_StaticPrimitiveRead(uint32_t field_idx, mirror::ArtMethod* method, Thread* self,
                           Primitive::Type field_type) {
  ArtField* result = nullptr;
  result = FindFieldFromCode<StaticPrimitiveRead, false>(field_idx, method, self,
                                                           Primitive::ComponentSize(field_type));
  return result;
}

ArtField* FindFieldFromAsm_StaticPrimitiveWrite(uint32_t field_idx, mirror::ArtMethod* method, Thread* self,
                           Primitive::Type field_type) {
  ArtField* result = nullptr;
  result = FindFieldFromCode<StaticPrimitiveWrite, false>(field_idx, method, self,
                                                           Primitive::ComponentSize(field_type));
  return result;
}


void RegDumpFromAsm(uint32_t* regs) {
  const char *regnames[] = {
      "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
      "r8", "r9", "r10", "r11", "r12", "sp", "lr", "pc"
  };
  for (int i = 0; i < 16; i++) {
    LOG(INFO) << regnames[i] << ": 0x" << std::hex << regs[i] << " " << std::dec << regs[i];
  }
}

void HexDumpFromAsm(uint32_t* addr, uint32_t len) {
    LOG(INFO) << HexDump(addr, len, true, "");
}

void WriteBarrierFieldFromAsm(mirror::Object* dest, int field_offset, mirror::Object* newvalue) {
  if (field_offset < 0 && field_offset >= 0x10000) {
    LOG(FATAL) << "field offset is out of range: " << field_offset;
  }
  Runtime::Current()->GetHeap()->WriteBarrierField(dest, MemberOffset(field_offset), newvalue);
}

// Given a dex pc, look up an exception handler for it.
uint32_t FindNextInstructionFollowingExceptionFromAsm(Thread* self,
                                                      ShadowFrame* shadow_frame,
                                                      uint32_t dex_pc,
                                                      mirror::Object* this_object) {
#if DEBUG_LOGS
  LOG(INFO) << "looking for exception handler for dexpc " << dex_pc;
#endif
  uint32_t found_dex_pc = interpreter::FindNextInstructionFollowingException(self, *shadow_frame,
                                               dex_pc);
#if DEBUG_LOGS
  LOG(INFO) << "found_dex_pc: " << found_dex_pc;
#endif
  return found_dex_pc;
}

void SetExceptionFromAsm(Thread* self, ShadowFrame* shadow_frame, mirror::Object *exception) {
  self->SetException(shadow_frame->GetCurrentLocationForThrow(), exception->AsThrowable());
}

// Called function is:
// static inline mirror::Class* ResolveVerifyAndClinit(uint32_t type_idx,
//    const mirror::ArtMethod* referrer,
//    Thread* self, bool can_run_clinit,
//    bool verify_access)


mirror::Class* ResolveVerifyAndClinitFromAsm(uint32_t type_idx, mirror::ArtMethod* referrer,
                                   Thread* self) {
  return ResolveVerifyAndClinit(type_idx, referrer, self, false, false);
}

void ThrowClassCastExceptionFromAsm(mirror::Class* cls, mirror::Object* obj) {
  ThrowClassCastException(cls, obj->GetClass());
}

bool InstanceOfFromAsm(mirror::Class* cls, mirror::Object* obj) {
  return obj->InstanceOf(cls);
}

void ThrowDivideByZeroExceptionFromAsm() {
  ThrowArithmeticExceptionDivideByZero();
}

void ThrowAbstractMethodErrorFromAsm(mirror::ArtMethod* method) {
  ThrowAbstractMethodError(method);
}

void ThrowArrayIndexOutOfBoundsExceptionFromAsm(int index, int length) {
  ThrowArrayIndexOutOfBoundsException(index, length);
}

void ThrowStackOverflowFromAsm(Thread* self) {
  ThrowStackOverflowError(self);
}

void ThrowNullPointerExceptionFromAsm(ShadowFrame* shadow_frame) {
  ThrowNullPointerExceptionFromDexPC(shadow_frame->GetCurrentLocationForThrow());
}

void ThrowNullPointerExceptionForFieldAccessFromAsm(ShadowFrame* shadow_frame,
                                             mirror::ArtField* field, bool is_read) {
  ThrowNullPointerExceptionForFieldAccess(shadow_frame->GetCurrentLocationForThrow(), field,
                                          is_read);
}

void CheckArrayAssignFromAsm(mirror::Object* a, mirror::Object* val) {
  ObjectArray<Object>* array = a->AsObjectArray<Object>();
  array->CheckAssignable(val);
}

// calls:
//  template <bool is_range, bool do_access_check>
//  bool DoFilledNewArray(const Instruction* inst, const ShadowFrame& shadow_frame,
//                        Thread* self, JValue* result) {
bool DoFilledNewArrayFromAsm(void* instaddr, const ShadowFrame* shadow_frame,
                             Thread* self, JValue* result) {
  const Instruction* inst = reinterpret_cast<Instruction*>(instaddr);
  return interpreter::DoFilledNewArray<false, false, false>(inst, *shadow_frame, self, result);
}

// calls:
//  template <bool is_range, bool do_access_check>
//  bool DoFilledNewArray(const Instruction* inst, const ShadowFrame& shadow_frame,
//                        Thread* self, JValue* result) {
bool DoFilledNewArrayRangeFromAsm(void* instaddr, const ShadowFrame* shadow_frame,
                             Thread* self, JValue* result) {
  const Instruction* inst = reinterpret_cast<Instruction*>(instaddr);
  return interpreter::DoFilledNewArray<true, false, false>(inst, *shadow_frame, self, result);
}

// Conversions.

void LongToFloatFromAsm(int64_t* value, float* result) {
  *result = *value;
}

void LongToDoubleFromAsm(int64_t* value, double* result) {
  *result = *value;
}
// TODO: not used, remove this

void FloatToLongFromAsm(float* value, int64_t* result) {
  *result = *value;
}

void FloatToDoubleFromAsm(float* value, double* result) {
  *result = *value;
}

// TODO: not used, remove this
void DoubleToLongFromAsm(double* value, int64_t* result) {
  *result = *value;
}

void DoubleToFloatFromAsm(double* value, float *result) {
  *result = *value;
}

void PushShadowFrameFromAsm(ShadowFrame* frame, Thread* self) {
  self->PushShadowFrame(frame);
}

void PopShadowFrameFromAsm(Thread* self) {
  self->PopShadowFrame();
}
}   // extern "C"


}   // namespace arm
}   // namespace art
