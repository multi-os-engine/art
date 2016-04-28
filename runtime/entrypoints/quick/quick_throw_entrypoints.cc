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

#include "callee_save_frame.h"
#include "common_throws.h"
#include "dex_file.h"
#include "dex_instruction.h"
#include "dex_instruction_utils.h"
#include "mirror/object-inl.h"
#include "thread.h"
#include "utils.h"
#include "well_known_classes.h"

namespace art {

// Deliver an exception that's pending on thread helping set up a callee save frame on the way.
extern "C" NO_RETURN void artDeliverPendingExceptionFromCode(Thread* self)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  self->QuickDeliverException();
}

// Called by generated call to throw an exception.
extern "C" NO_RETURN void artDeliverExceptionFromCode(mirror::Throwable* exception, Thread* self)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  /*
   * exception may be null, in which case this routine should
   * throw NPE.  NOTE: this is a convenience for generated code,
   * which previously did the null check inline and constructed
   * and threw a NPE if null.  This routine responsible for setting
   * exception_ in thread and delivering the exception.
   */
  ScopedQuickEntrypointChecks sqec(self);
  if (exception == nullptr) {
    self->ThrowNewException("Ljava/lang/NullPointerException;", "throw with null exception");
  } else {
    self->SetException(exception);
  }
  self->QuickDeliverException();
}

// Called by generated call to throw a NPE exception.
extern "C" NO_RETURN void artThrowNullPointerExceptionFromCode(Thread* self)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  self->NoteSignalBeingHandled();
  ThrowNullPointerExceptionFromDexPC();
  self->NoteSignalHandlerDone();
  self->QuickDeliverException();
}

// Called by generated call to throw an arithmetic divide by zero exception.
extern "C" NO_RETURN void artThrowDivZeroFromCode(Thread* self)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  ThrowArithmeticExceptionDivideByZero();
  self->QuickDeliverException();
}

// Called by generated call to throw an array index out of bounds exception.
extern "C" NO_RETURN void artThrowArrayBoundsFromCode(int index, int length, Thread* self)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  // TODO: Use a new entrypoint for SIOOB instead of hacking this one.
  // Compiled code uses the AIIOB entrypoint also for the String.charAt() that throws SIIOB.
  uint32_t dex_pc;
  ArtMethod* current_method = self->GetCurrentMethod(&dex_pc);
  // Note: current_method is null for stub_test.
  const Instruction* instruction = nullptr;
  Instruction::Code opcode = Instruction::AGET;
  if (LIKELY(current_method != nullptr)) {
    const DexFile::CodeItem* code_item = current_method->GetCodeItem();
    DCHECK_LT(dex_pc, code_item->insns_size_in_code_units_);
    instruction = Instruction::At(code_item->insns_ + dex_pc);
    opcode = instruction->Opcode();
  }
  if (IsInstructionAGetOrAPut(opcode) || opcode == Instruction::FILL_ARRAY_DATA) {
    ThrowArrayIndexOutOfBoundsException(index, length);
    self->QuickDeliverException();
  } else {
    DCHECK(IsInstructionInvoke(opcode) || IsInstructionQuickInvoke(opcode)) << opcode;
    DCHECK_EQ(PrettyMethod(instruction->VRegB(), *current_method->GetDexFile()),
              "char java.lang.String.charAt(int)");
    // TODO: Add extra frame!
    self->ThrowNewExceptionF("Ljava/lang/StringIndexOutOfBoundsException;",
                             "length=%i; index=%i", length, index);
    self->QuickDeliverException();
  }
}

extern "C" NO_RETURN void artThrowStackOverflowFromCode(Thread* self)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  self->NoteSignalBeingHandled();
  ThrowStackOverflowError(self);
  self->NoteSignalHandlerDone();
  self->QuickDeliverException();
}

extern "C" NO_RETURN void artThrowNoSuchMethodFromCode(int32_t method_idx, Thread* self)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  ThrowNoSuchMethodError(method_idx);
  self->QuickDeliverException();
}

extern "C" NO_RETURN void artThrowClassCastException(mirror::Class* dest_type,
                                                     mirror::Class* src_type,
                                                     Thread* self)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  DCHECK(!dest_type->IsAssignableFrom(src_type));
  ThrowClassCastException(dest_type, src_type);
  self->QuickDeliverException();
}

extern "C" NO_RETURN void artThrowArrayStoreException(mirror::Object* array, mirror::Object* value,
                                                      Thread* self)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  ThrowArrayStoreException(value->GetClass(), array->GetClass());
  self->QuickDeliverException();
}

}  // namespace art
