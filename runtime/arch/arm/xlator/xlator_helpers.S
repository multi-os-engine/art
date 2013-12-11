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

// types for ResolveMethod
// .set kStatic,0     // <<static>>
// .set kDirect,1     // <<direct>>
// .set kVirtual, 2    // <<virtual>>
// .set kSuper, 3      // <<super>>
// .set kInterface, 4  // <<interface>>


// Field types.
.set kPrimNot, 0
.set kPrimBoolean, 1
.set kPrimByte, 2
.set kPrimChar, 3
.set kPrimShort, 4
.set kPrimInt, 5
.set kPrimLong, 6
.set kPrimFloat, 7
.set kPrimDouble, 8
.set kPrimVoid, 9

 // We have an array of function addresses for external interpreter functions

.set art_memcpy, 0
.set art_CheckSuspend, 1
.set art_ResolveString, 2
.set art_HandlePendingException, 3
.set art_ResolveVerifyAndClinit, 4
.set art_MonitorEnter, 5
.set art_MonitorExit, 6
.set art_CheckArrayAssign, 7
.set art_InstanceOf, 8
.set art_ThrowDivideByZeroException, 9
.set art_DoIntDivide, 10
.set art_DoIntRemainder, 11
.set art_ThrowNullPointerException, 12
.set art_AllocObjectFromCode, 13
.set art_AllocArrayFromCode, 14
.set art_DoFilledNewArray, 15
.set art_DoFilledNewArrayRange, 16
.set art_FillArrayData, 17
.set art_Throw, 18
.set art_fmodf, 19
.set art_fmod, 20
.set art_ThrowArrayIndexOutOfBoundsException, 21
.set art_DoLongDivide, 22
.set art_DoLongRemainder, 23
.set art_ThrowStackOverflow, 24
.set art_ThrowNullPointerExceptionForFieldAccess, 25
.set art_ResolveVirtualMethod, 26
.set art_DoCall, 27
.set art_DoCallRange, 28
.set art_Print, 29
.set art_Breakpoint, 30
.set art_RegDump, 31
.set art_HexDump, 32
.set art_WriteBarrierField, 33
.set art_ResolveMethodQuick, 34
.set art_SetException, 35
.set art_ThrowClassCastException, 36
.set art_LongToFloat, 37
.set art_LongToDouble, 38
.set art_FloatToLong, 39
.set art_FloatToDouble, 40
.set art_DoubleToLong, 41
.set art_DoubleToFloat, 42
.set art_ThrowAbstractMethodError, 43
.set art_FloatToInt, 44
.set art_DoubleToInt, 45
.set art_CompareFloatLess, 46
.set art_CompareFloatGreater, 47
.set art_CompareDoubleLess, 48
.set art_CompareDoubleGreater, 49
.set art_ResolveDirectMethod, 50
.set art_ResolveSuperMethod, 51
.set art_ResolveInterfaceMethod, 52
.set art_ResolveStaticMethod, 53
.set art_ResolveField_InstanceObjectRead, 54
.set art_ResolveField_InstanceObjectWrite, 55
.set art_ResolveField_InstancePrimitiveRead, 56
.set art_ResolveField_InstancePrimitiveWrite, 57
.set art_ResolveField_StaticObjectRead, 58
.set art_ResolveField_StaticObjectWrite, 59
.set art_ResolveField_StaticPrimitiveRead, 60
.set art_ResolveField_StaticPrimitiveWrite, 61
.set art_PushShadowFrame, 62
.set art_PopShadowFrame, 63
.section ".text"
.arm

.global art_memcpyHelper
.global art_CheckSuspendHelper
.global art_ResolveStringHelper
.global art_HandlePendingExceptionHelper
.global art_ResolveVerifyAndClinitHelper
.global art_MonitorEnterHelper
.global art_MonitorExitHelper
.global art_CheckArrayAssignHelper
.global art_InstanceOfHelper
.global art_ThrowDivideByZeroExceptionHelper
.global art_DoIntDivideHelper
.global art_DoIntRemainderHelper
.global art_ThrowNullPointerExceptionHelper
.global art_AllocObjectFromCodeHelper
.global art_AllocArrayFromCodeHelper
.global art_DoFilledNewArrayHelper
.global art_DoFilledNewArrayRangeHelper
.global art_FillArrayDataHelper
.global art_ThrowHelper
.global art_fmodfHelper
.global art_fmodHelper
.global art_ThrowArrayIndexOutOfBoundsExceptionHelper
.global art_DoLongDivideHelper
.global art_DoLongRemainderHelper
.global art_ThrowStackOverflowHelper
.global art_ThrowNullPointerExceptionForFieldAccessHelper
.global art_ResolveVirtualMethodHelper
.global art_DoCallHelper
.global art_DoCallRangeHelper
.global art_PrintHelper
.global art_BreakpointHelper
.global art_RegDumpHelper
.global art_HexDumpHelper
.global art_WriteBarrierFieldHelper
.global art_ResolveMethodQuickHelper
.global art_SetExceptionHelper
.global art_ThrowClassCastExceptionHelper
.global art_LongToFloatHelper
.global art_LongToDoubleHelper
.global art_FloatToLongHelper
.global art_FloatToDoubleHelper
.global art_DoubleToLongHelper
.global art_DoubleToFloatHelper
.global art_ThrowAbstractMethodErrorHelper
.global art_FloatToIntHelper
.global art_DoubleToIntHelper
.global art_CompareFloatLessHelper
.global art_CompareFloatGreaterHelper
.global art_CompareDoubleLessHelper
.global art_CompareDoubleGreaterHelper
.global art_ResolveDirectMethodHelper
.global art_ResolveSuperMethodHelper
.global art_ResolveInterfaceMethodHelper
.global art_ResolveStaticMethodHelper
.global art_ResolveFieldHelper_InstanceObjectRead
.global art_ResolveFieldHelper_InstanceObjectWrite
.global art_ResolveFieldHelper_InstancePrimitiveRead
.global art_ResolveFieldHelper_InstancePrimitiveWrite
.global art_ResolveFieldHelper_StaticObjectRead
.global art_ResolveFieldHelper_StaticObjectWrite
.global art_ResolveFieldHelper_StaticPrimitiveRead
.global art_ResolveFieldHelper_StaticPrimitiveWrite
.global art_PushShadowFrameHelper
.global art_PopShadowFrameHelper

// Store the dex pc in the shadow frame
// entry:
// [sp,#0]: translated method
// r7: vregs in shadow frame
// r10: ARM pc + 4

// exit:
// r0: dexpc (also stored in shadow frame)
art_GetDexPC:
    push {r1,lr}
    ldr r0,[sp,#8]      // Get TranslatedMethod address
    sub r1, r10, #4     // Program PC (-4 to get to call instruction)
    blx LookupDexPC
    cmn r0, #1          // -1 means we couldn't find it
    strne r0,[r7,#(shadow_frame_dexpc_offset - shadow_frame_vregs_offset)]    // store dex pc in frame
    pop {r1,pc}

// NOTE: stack aligment!
// The ARM EABI says that the stack must be 8-byte aligned for all public
// interfaces.  Anything that we call from here could be considered a public
// interface.  This means we must push an even number of registers onto the stack
// before calling amy function from here.
art_memcpyHelper:
  push {r0-r4, lr}
  bl memcpy
  pop {r0-r4, pc}

// entry:
// r9: thread pointer
// preserves all temp registers
art_CheckSuspendHelper:
  push {r0-r4,lr}     // r4 is also saved to align stack for EABI
  ldrh r0,[r9,#0]
  cmp r0,#0
  bne 1f
  pop {r0-r4, pc}
1:
  mov r0,r9
  blx CheckSuspendFromAsm
  pop {r0-r4,pc}


// entry:
// r0: string index
// r9: thread pointer
// [sp,#12]: method helper address
art_ResolveStringHelper:
  ldr r1,[sp,#12]     // method helper
  mov r2, r0          // string index
  mov r0, r9          // thread
  push {r9, lr}
  blx ResolveStringFromAsm
  cmp r0,#0
  pop {r9, r10}
  beq art_HandlePendingExceptionHelper
  mov pc, r10

// handle a pending exception held in the exception_ field
// of the thread (passed in r9).  The stack contains other
// information we need.

// entry:
// [sp,#16]: code item address
// [sp,#0]: address of TranslatedMethod (holds dexmap and size)
// r7: address of vregs in shadow frame
// r9: self
// r10: processor pc

// This is only called when we have an exception pending.  It will not return to the caller.
art_HandlePendingExceptionHelper:

  ldrh r1,[r9,#0]     // flags
  cmp r1,#0
  blne art_CheckSuspendHelper

 // First figure out the dex pc (the LookupDexPC will not save r0-r3 as it's an EABI function)
  bl art_GetDexPC
  mov r10, r0     // Save for later

  // get the this pointer
  ldr r1,[r7,#(shadow_frame_method_offset - shadow_frame_vregs_offset)]  // method
  ldr r2,[r1,#method_access_flags_offset]
  tst r2,#0x08      // kStatic
  movne r1, #0      // NULL
  bne 1f
  ldr r0,[sp,#16]      // code item
  ldrh r1,[r0,#code_item_ins_size_offset]      // number of in registers
  ldr r2,[r7,#(shadow_frame_number_of_vregs_offset - shadow_frame_vregs_offset)]  // number of vregs
  sub r1, r2, r1          // get address of 'this' register
  ldr r1,[r5, r1, lsl #2]     // get vreg reference at offset r1 (the 'this' pointer)
1:


// set up the call to FindNextInstructionFollowingExceptionFromAsm.  Signature is:
// uint32_t FindNextInstructionFollowingExceptionFromAsm(Thread* self,
//                                                      ShadowFrame* shadow_frame,
//                                                      uint32_t dex_pc,
//                                                      mirror::Object* this_object)
// r1: this pointer
// r7: vregs address in shadow frame
// r9: self

  mov r0, r9            // self
  mov r3, r1            // this
  sub r1, r7, #shadow_frame_vregs_offset    // shadow frame

  mov r2, r10            // dexpc
  blx FindNextInstructionFollowingExceptionFromAsm  // r0 will be 0xffffffff or a dex pc value
  cmn r0,#1               // was result 0xffffffff
  beq 2f

  // we have a valid dex pc (in r0)
  mov r1, r0
  // translate the dex pc to a program pc value
  ldr r0,[sp,#0]      // Get TranslatedMethod address
  blx TranslateDexPC

  // r0 will contain the address of the next program instruction
  ldr pc,[r0]          // and branch to the next instruction

2:
  METHOD_RETURN       // return from method

// entry:
// [sp,#0]: TranslatedMethod
// r0: exception
// r7: vregs in shadow frame
// r9: self
// r10: ARM pc+4

// This needs to work out the dex pc but we can't call art_GetDexPC directly - hence the inline.
art_SetExceptionHelper:
   push {r0-r2,lr}
   ldr r0,[sp,#16]      // Get TranslatedMethod address
   sub r1, r10, #4     // Program PC (-4 to get to call instruction)
   blx LookupDexPC
   cmn r0, #1          // -1 means we couldn't find it
   strne r0,[r7,#(shadow_frame_dexpc_offset - shadow_frame_vregs_offset)]    // store dex pc in frame
   ldr r2,[sp,#0]      // reload exception
   sub r1, r7, #shadow_frame_vregs_offset
   mov r0, r9
   blx SetExceptionFromAsm
   pop {r0-r2,pc}


// entry:
// r0: object pointer
// r9: thread pointer
art_MonitorEnterHelper:
  cmp r0,#0
  beq art_ThrowNullPointerExceptionHelper
  push {r0,lr}
  mov r1, r0      // object pointer
  mov r0, r9      // thread
  blx DoMonitorEnterFromAsm
  ldr r0,[r9,#thread_exception_offset]
  cmp r0,#0
  pop {r0,r10}
  bne art_HandlePendingExceptionHelper
  mov pc,r10

// entry:
// r0: object pointer
// r9: thread pointer
art_MonitorExitHelper:
  cmp r0,#0
  beq art_ThrowNullPointerExceptionHelper
  push {r0,lr}
  mov r1, r0      // object pointer
  mov r0, r9      // thread
  blx DoMonitorExitFromAsm
  ldr r0,[r9,#thread_exception_offset]
  cmp r0,#0
  pop {r0,r10}
  bne art_HandlePendingExceptionHelper
  mov pc, r10

// entry:
// r1: value
// r2: array
// NOTE: saves all registers
art_CheckArrayAssignHelper:
  push {r0-r4, lr}
  mov r0, r2
  blx CheckArrayAssignFromAsm

  // if the array is not assignable, an exception will be pending
  ldr r0,[r9,#thread_exception_offset]
  cmp r0,#0
  pop {r0-r4, r10}
  bne art_HandlePendingExceptionHelper
  mov pc, r10

// entry:
// r0: class
// r1: object
// saves r1-r3
// exit:
// r0: 0 or 1
art_InstanceOfHelper:
  push {r1-r3, lr}
  blx InstanceOfFromAsm
  pop {r1-r3, pc}

art_ThrowDivideByZeroExceptionHelper:
  mov r10, lr
  blx ThrowDivideByZeroExceptionFromAsm
  b art_HandlePendingExceptionHelper

// entry:
// r0: result vreg number
// r1: dividend
// r2: divisor
// r7: shadow frame vregs address
art_DoIntDivideHelper:
  push {r0,lr}
  mov r3, r2    // divisor
  mov r2, r1    // dividend
  mov r1, r0    // result reg
  sub r0, r7, #shadow_frame_vregs_offset   // shadow frame address
  blx DoIntDivideFromAsm
  ldr r0,[r9,#thread_exception_offset]
  cmp r0,#0
  pop {r0,r10}
  bne art_HandlePendingExceptionHelper
  mov pc, r10

// entry:
// r0: result vreg number
// r1: dividend
// r2: divisor
// r7: shadow frame vregs address
art_DoIntRemainderHelper:
  push {r0,lr}
  mov r3, r2    // divisor
  mov r2, r1    // dividend
  mov r1, r0    // result reg
  sub r0, r7, #shadow_frame_vregs_offset   // shadow frame address
  blx DoIntRemainderFromAsm
  ldr r0,[r9,#thread_exception_offset]
  cmp r0,#0
  pop {r0,r10}
  bne art_HandlePendingExceptionHelper
  mov pc, r10

art_ThrowNullPointerExceptionHelper:
  mov r10, lr
  bl art_GetDexPC
  sub r0, r7, #shadow_frame_vregs_offset   // shadow frame address
  blx ThrowNullPointerExceptionFromAsm
  b art_HandlePendingExceptionHelper


// entry:
// r0: type index
// r7: vregs
// r9: thread

// exit:
// r0: Object* allocated
// mirror::Object* AllocObjectFromAsm(uint32_t type_idx, mirror::ArtMethod* method, Thread* self)
art_AllocObjectFromCodeHelper:
  push {r2,lr}
  ldr     r1,[r7,#(shadow_frame_method_offset - shadow_frame_vregs_offset)] // method_
  mov r2, r9
  blx AllocObjectFromAsm
  pop {r2,r10}
  cmp r0,#0
  beq art_HandlePendingExceptionHelper
  mov pc, r10

// entry:
// r0: type index
// r1: component count
// r7: vregs
// r9: thread

// exit:
// r0: Object* allocated
// mirror::Array* AllocArrayFromAsm(uint32_t type_idx, mirror::ArtMethod* method, Thread* self,
//    int32_t component_count)
art_AllocArrayFromCodeHelper:
  push {r2,lr}
  mov r3, r1        // component count
  ldr     r1,[r7,#(shadow_frame_method_offset - shadow_frame_vregs_offset)] // method_
  mov r2, r9
  blx AllocArrayFromAsm
  pop {r2,r10}
  cmp r0,#0
  beq art_HandlePendingExceptionHelper
  mov pc, r10

// entry:
// [lr,#0]: dex instruction address
art_DoFilledNewArrayHelper:
  RESULT r3
  push {r1,lr}
  ldr r0,[lr,#0]
  sub r1, r7, #shadow_frame_vregs_offset      // shadow frame
  mov r2, r9          // self
  blx DoFilledNewArrayFromAsm
  ldr r0,[r9,#thread_exception_offset]
  pop {r1,r10}
  cmp r0,#0
  bne art_HandlePendingExceptionHelper
  add pc, r10, #4         // return to after dex instruction

// entry:
// [lr,#0]: dex instruction address
art_DoFilledNewArrayRangeHelper:
  RESULT r3
  push {r1,lr}
  ldr r0,[lr,#0]
  sub r1, r7, #shadow_frame_vregs_offset      // shadow frame
  mov r2, r9          // self
  blx DoFilledNewArrayRangeFromAsm
  ldr r0,[r9,#thread_exception_offset]
  pop {r1,r10}
  cmp r0,#0
  bne art_HandlePendingExceptionHelper
  add pc, r10, #4         // return to after dex instruction

art_FillArrayDataHelper:
  push {r0,lr}
  NYI
  pop {r0,pc}

art_ThrowHelper:
  push {r0,lr}
  PRINT "throw"
  NYI
  pop {r0,pc}

art_fmodfHelper:
  push {r2,lr}
  bl fmodf
  pop {r2,pc}

art_fmodHelper:
  push {r2,lr}
  bl fmod
  pop {r2,pc}

// entry:
// r0: index
// r1: length
art_ThrowArrayIndexOutOfBoundsExceptionHelper:
  mov r10, lr
  blx ThrowArrayIndexOutOfBoundsExceptionFromAsm
  b art_HandlePendingExceptionHelper

// entry:
// r0: result vreg number
// r1, r2: lo, hi for lhs
// r3, r4, lo, hi for rhs
art_DoLongDivideHelper:
  push {r0,lr}
  push {r3, r4}         // push 5th and 6th args
  mov r3, r2
  mov r2, r1
  mov r1, r0
  sub r0, r7, #shadow_frame_vregs_offset
  blx DoLongDivideFromAsm
  add sp, sp, #8
  pop {r0,r10}
  ldr r0,[r9,#thread_exception_offset]
  cmp r0,#0
  bne art_HandlePendingExceptionHelper
  mov pc, r10

// entry:
// r0: result vreg number
// r1, r2: lo, hi for lhs
// r3, r4, lo, hi for rhs
art_DoLongRemainderHelper:
  push {r0,lr}
  push {r3, r4}         // push 5th and 6th args
  mov r3, r2
  mov r2, r1
  mov r1, r0
  sub r0, r7, #shadow_frame_vregs_offset
  blx DoLongRemainderFromAsm
  add sp, sp, #8
  pop {r0,r10}
  ldr r0,[r9,#thread_exception_offset]
  cmp r0,#0
  bne art_HandlePendingExceptionHelper
  mov pc, r10


// entry:
// r0: field index
// r1: field type
// ArtField* FindFieldFromAsm_...(uint32_t field_idx, mirror::ArtMethod* method, Thread* self,
                          // Primitive::Type field_type)

.macro RESOLVE_FIELD type
art_ResolveFieldHelper_\type:
  push {r1,lr}
  mov r3, r1          // field type is 4th arg
  ldr r1, [r7,#(shadow_frame_method_offset - shadow_frame_vregs_offset)]  // method
  mov r2, r9        // self
  blx FindFieldFromAsm_\type
  pop {r1,r10}
  cmp r0,#0
  beq art_HandlePendingExceptionHelper
  mov pc, r10
.endm

RESOLVE_FIELD InstanceObjectRead
RESOLVE_FIELD InstanceObjectWrite
RESOLVE_FIELD InstancePrimitiveRead
RESOLVE_FIELD InstancePrimitiveWrite
RESOLVE_FIELD StaticObjectRead
RESOLVE_FIELD StaticObjectWrite
RESOLVE_FIELD StaticPrimitiveRead
RESOLVE_FIELD StaticPrimitiveWrite


// Throw a stack overflow exception
// entry:
// r9: self

art_ThrowStackOverflowHelper:
  push {r0, lr}
  mov r0, r9
  blx ThrowStackOverflowFromAsm
  pop {r0, pc}

// entry:
// r0: field pointer
// r1: 1 => isread
art_ThrowNullPointerExceptionForFieldAccessHelper:
  mov r10, lr
  mov r2, r1
  mov r1, r0
  bl art_GetDexPC
  sub r0, r7, #shadow_frame_vregs_offset   // shadow frame address
  blx ThrowNullPointerExceptionForFieldAccessFromAsm
  b art_HandlePendingExceptionHelper

// entry:
// r0: method index
// r1: receiver
// [lr,#0]: dex pc
// returns to beyond data following call

// exit:
// r0: method
// r2: corrupted
// r10: corrupted
.macro RESOLVE_METHOD type
art_Resolve\type\()MethodHelper:
  push {r1, lr}
  mov r3, r9        // thread pointer as 4th arg
  ldr r2,[lr,#0]    // load dex pc
  str r2,[r7,#(shadow_frame_dexpc_offset - shadow_frame_vregs_offset)]    // store dex pc in frame
  ldr r2, [r7,#(shadow_frame_method_offset - shadow_frame_vregs_offset)]    // method
  bl Find\type\()MethodFromAsm
  pop {r1,r10}
  cmp r0, #0
  beq art_HandlePendingExceptionHelper
  ldr r2,[r0,#method_access_flags_offset]
  tst r2,#0x0400
  bne art_ThrowAbstractMethodErrorHelper
  add pc, r10, #4
.endm

RESOLVE_METHOD Virtual
RESOLVE_METHOD Direct
RESOLVE_METHOD Super
RESOLVE_METHOD Interface



// entry:
// r0: method index
art_ResolveStaticMethodHelper:
  push {r1, lr}
  mov r1, #0
  mov r3, r9        // thread pointer as 4th arg
  ldr r2, [r7,#(shadow_frame_method_offset - shadow_frame_vregs_offset)]    // method
  bl FindStaticMethodFromAsm
  pop {r1,r10}
  cmp r0,#0
  beq art_HandlePendingExceptionHelper
  mov pc, r10



// entry:
// r0: vtable index
// r1: receiver object
art_ResolveMethodQuickHelper:
  push {r1,lr}
  bl QuickMethodFromAsm
  pop {r1,r10}
  cmp r0, #0
  beq art_HandlePendingExceptionHelper
  ldr r2,[r0,#method_access_flags_offset]
  tst r2,#0x0400
  bne art_ThrowAbstractMethodErrorHelper
  mov pc, r10


// Call helpers.
// entry:
// r0: resolved method
// r1: receiver
// r9: thread

// at [lr,#0] and [lr,#4] are two information words:
// [lr,#0]: pointer to dex instruction
// [lr,#4]: dex pc value

// bool DoCallFromAsm(ArtMethod* method, Object* receiver, Thread* self, ShadowFrame* shadow_frame,
//            uint16_t* instaddr, JValue* result)
art_DoCallHelper:
  RESULT r3
  push {r0,lr}
  // 2 args on stack
  ldr r2,[lr,#0]         // get address of dex instruction
  push  {r2, r3}         // push instaddr and result
  ldr r2,[lr,#4]         // dex pc
  str r2,[r7,#(shadow_frame_dexpc_offset - shadow_frame_vregs_offset)]    // store dex pc in frame
  sub r3, r7, #shadow_frame_vregs_offset    // shadow frame
  mov r2, r9            // thread
  blx DoCallFromAsm
  add sp, sp, #8
  ldr r1,[r9,#thread_exception_offset]
  pop {r0,r10}
  cmp r1,#0
  bne art_HandlePendingExceptionHelper
  add pc, r10, #8        // return to address after data words


// entry:
// r0: resolved method
// r1: receiver
// r9: thread
// at [lr,#0] and [lr,#4] are two information words:
// [lr,#0]: pointer to dex instruction
// [lr,#4]: dex pc value

// bool DoCallFromAsm(ArtMethod* method, Object* receiver, Thread* self, ShadowFrame* shadow_frame,
//            uint16_t* instaddr, JValue* result)
art_DoCallRangeHelper:
  RESULT r3
  push {r0,lr}
  str r12,[r7,#(shadow_frame_dexpc_offset - shadow_frame_vregs_offset)]    // store dex pc in frame
  // 2 args on stack
  ldr r2,[lr,#0]         // get address of dex instruction
  push  {r2, r3}         // push instaddr and result
  ldr r2,[lr,#4]         // dex pc
  str r2,[r7,#(shadow_frame_dexpc_offset - shadow_frame_vregs_offset)]    // store dex pc in frame
  sub r3, r7, #shadow_frame_vregs_offset    // shadow frame
  mov r2, r9            // thread
  blx DoCallRangeFromAsm
  add sp, sp, #8
  ldr r1,[r9,#thread_exception_offset]
  pop {r0,r10}
  cmp r1,#0
  bne art_HandlePendingExceptionHelper
  add pc, r10, #8        // return to address after data words


// entry:
// lr: address of string to print
// returns to 4 byte aligned address after terminating 0 in string
art_PrintHelper:
  push {r0,r1,r2}
  mov r1, lr
  mov r2, r1
1:
  ldrb r0, [r1], #1
  cmp r0, #0
  bne 1b
  // r1 is now the byte after the last 0
  add r1, r1, #3
  bic r1, r1, #3    // align to 4 bytes
  push {r1}
  mov r0, r2
  blx PrintFromAsm
  pop {lr}
  pop {r0,r1,r2}
  mov pc, lr

art_BreakpointHelper:
  mov pc,lr

art_RegDumpHelper:
  push {r0-r12,lr}      // 14 registers pushed
  sub sp, sp, #16*4     // space for regs
  stmia sp,{r0-lr}
  add r0, sp, #16*4 + 14*4    // get old value of sp (at point of call)
  str r0,[sp,#13*4]     // store sp
  str pc,[sp,#15*4]     // store pc

  mov r0, sp            // address of reg dump memory
  blx RegDumpFromAsm

  add sp, sp, #16*4     // restore sp
  pop {r0-r12,pc}

art_HexDumpHelper:
    push {r0-r12,lr}
    blx HexDumpFromAsm
    pop {r0-r12,pc}

// entry:
// r0: object
// r1: offset
// r2: value
art_WriteBarrierFieldHelper:
    cmp r2, #0        // No nulls.
    moveq pc, lr
    push {r0,lr}
    blx WriteBarrierFieldFromAsm
    pop {r0,pc}

// entry:
// r0: type index
// r1: method
// r9: self

// exit:
// r0: resolved class
art_ResolveVerifyAndClinitHelper:
  push {r1,lr}
  mov r2, r9
  blx ResolveVerifyAndClinitFromAsm
  ldr r1,[r9,#thread_exception_offset]
  pop {r1,r10}
  cmp r0,#0
  beq art_HandlePendingExceptionHelper
  mov pc, r10

// entry:
// r0: dest class
// r1: object
// lr: pc of caller
art_ThrowClassCastExceptionHelper:
  mov r10, lr
  blx ThrowClassCastExceptionFromAsm
  bl art_GetDexPC
  b art_HandlePendingExceptionHelper

// entry:
// r0: method
// lr: pc of caller
art_ThrowAbstractMethodErrorHelper:
  mov r10, lr
  blx ThrowAbstractMethodErrorFromAsm
  bl art_GetDexPC
  b art_HandlePendingExceptionHelper


// entry:
// r0: address of vreg pair containing value
// r1: address of vreg for result
art_LongToFloatHelper:
  push {r0,lr}
  blx LongToFloatFromAsm
  pop {r0,pc}

// entry:
// r0: address of vreg pair containing value
// r1: address of vreg for result
art_LongToDoubleHelper:
  push {r0,lr}
  blx LongToDoubleFromAsm
  pop {r0,pc}

// entry:
// r0: float value
// r0, r1: long value
art_FloatToLongHelper:
    stmfd   sp!, {r4, lr}
    bl art_f2l
    ldmfd   sp!, {r4, pc}


// entry:
// r0: float

// exit:
// r0: int
art_FloatToIntHelper:
    stmfd   sp!, {r4, lr}
    bl art_f2i
    ldmfd   sp!, {r4, pc}

    mov     r1, #0x4f000000             @ (float)maxint
    mov     r4, r0
    bl      __aeabi_fcmpge              @ is arg >= maxint?
    cmp     r0, #0                      @ nonzero == yes
    mvnne   r0, #0x80000000             @ return maxint (7fffffff)
    ldmnefd sp!, {r4, pc}

    mov     r0, r4                      @ recover arg
    mov     r1, #0xcf000000             @ (float)minint
    bl      __aeabi_fcmple              @ is arg <= minint?
    cmp     r0, #0                      @ nonzero == yes
    movne   r0, #0x80000000             @ return minint (80000000)
    ldmnefd sp!, {r4, pc}

    mov     r0, r4                      @ recover arg
    mov     r1, r4
    bl      __aeabi_fcmpeq              @ is arg == self?
    cmp     r0, #0                      @ zero == no
    ldmeqfd sp!, {r4, pc}               @ return zero for NaN

    mov     r0, r4                      @ recover arg
    bl      __aeabi_f2iz                @ convert float to int
    ldmfd   sp!, {r4, pc}

// entry:
// r0: address of vreg containing float
// r1: address of vreg pair for result
art_FloatToDoubleHelper:
  push {r0,lr}
  blx FloatToDoubleFromAsm
  pop {r0,pc}

// entry:
// r0,r1: input double
// exit:
// r0,r1: result long
art_DoubleToLongHelper:
    stmfd   sp!, {r4, lr}
    bl art_d2l
    ldmfd   sp!, {r4, pc}


// entry:
// r0, r1: double

// exit:
// r0: int

art_DoubleToIntHelper:
    stmfd   sp!, {r4, r5, lr}           @ save regs
    mov     r2, #0x80000000             @ maxint, as a double (low word)
    mov     r2, r2, asr #9              @  0xffc00000
    sub     sp, sp, #4                  @ align for EABI
    mvn     r3, #0xbe000000             @ maxint, as a double (high word)
    sub     r3, r3, #0x00200000         @  0x41dfffff
    mov     r4, r0                      @ save a copy of r0
    mov     r5, r1                      @  and r1
    bl      __aeabi_dcmpge              @ is arg >= maxint?
    cmp     r0, #0                      @ nonzero == yes
    mvnne   r0, #0x80000000             @ return maxint (0x7fffffff)
    bne     1f

    mov     r0, r4                      @ recover arg
    mov     r1, r5
    mov     r3, #0xc1000000             @ minint, as a double (high word)
    add     r3, r3, #0x00e00000         @  0xc1e00000
    mov     r2, #0                      @ minint, as a double (low word)
    bl      __aeabi_dcmple              @ is arg <= minint?
    cmp     r0, #0                      @ nonzero == yes
    movne   r0, #0x80000000             @ return minint (80000000)
    bne     1f

    mov     r0, r4                      @ recover arg
    mov     r1, r5
    mov     r2, r4                      @ compare against self
    mov     r3, r5
    bl      __aeabi_dcmpeq              @ is arg == self?
    cmp     r0, #0                      @ zero == no
    beq     1f                          @ return zero for NaN

    mov     r0, r4                      @ recover arg
    mov     r1, r5
    bl      __aeabi_d2iz                @ convert double to int

1:
    add     sp, sp, #4
    ldmfd   sp!, {r4, r5, pc}


// r0: address of vreg pair containing value
// r1: address of vreg for result
art_DoubleToFloatHelper:
  push {r0,lr}
  blx DoubleToFloatFromAsm
  pop {r0,pc}

// entry:
// r0: lhs float
// r1: rhs float

// exit:
// r0: result (as per DEX CMPL_FLOAT)
art_CompareFloatLessHelper:
    push {r2, lr}
    bl      __aeabi_cfcmple             @ cmp <=: C clear if <, Z set if eq
    bhi 1f
    mvncc   r0, #0                      @ (less than) r1<- -1
    moveq   r0, #0                      @ (equal) r1<- 0, trumps less than
    pop   {r2, pc}
1:
    mov   r2, r0                        // reverse
    mov   r0, r1
    mov   r1, r2
    bl      __aeabi_cfcmple             @ r0<- Z set if eq, C clear if <
    movcc   r0, #1                      @ (greater than) r1<- 1
    mvncs     r0, #0
    pop     {r2, pc}

// entry:
// r0: lhs float
// r1: rhs float

// exit:
// r0: result (as per DEX CMPG_FLOAT)
art_CompareFloatGreaterHelper:
    push {r2, lr}
    bl      __aeabi_cfcmple             @ cmp <=: C clear if <, Z set if eq
    movhi   r0, #1
    pophi   {r2, pc}
    mvncc   r0, #0                      @ (less than) r1<- -1
    moveq   r0, #0                      @ (equal) r1<- 0, trumps less than
    pop   {r2, pc}


// entry:
// r0, r1: lhs
// r2, r3: rhs

// exit:
// r0: result as per DEX CMPL_DOUBLE
art_CompareDoubleLessHelper:
    push    {r4, lr}
    bl      __aeabi_cdcmple             @ cmp <=: C clear if <, Z set if eq
    bhi     1f       @ C set and Z clear, disambiguate
    mvncc   r0, #0                      @ (less than) r1<- -1
    moveq   r0, #0                      @ (equal) r1<- 0, trumps less than
    pop     {r4, pc}
1:
    // swap r0/r2 and r1/r3
    mov     r4, r0
    mov     r0, r2
    mov     r2, r4
    mov     r4, r1
    mov     r1, r3
    mov     r3, r4
    bl      __aeabi_cdcmple             @ r0<- Z set if eq, C clear if <
    movcc   r0, #1                      @ (greater than) r1<- 1
    mvncs   r0, #0                          @ r1<- 1 or -1 for NaN
    pop     {r4,pc}

 // entry:
// r0, r1: lhs
// r2, r3: rhs

// exit:
// r0: result as per DEX CMPG_DOUBLE
art_CompareDoubleGreaterHelper:
    push    {r4, lr}
    bl      __aeabi_cdcmple             @ cmp <=: C clear if <, Z set if eq
    movhi   r0, #1
    pophi   {r4, pc}
    mvncc   r0, #0                      @ (less than) r1<- -1
    moveq   r0, #0                      @ (equal) r1<- 0, trumps less than
    pop     {r4, pc}

// entry:
// r7: vregs in shadow frame
// r9: self
art_PushShadowFrameHelper:
    push {r0-r4, lr}
    sub r0, r7, #shadow_frame_vregs_offset
    mov r1, r9
    blx PushShadowFrameFromAsm
    pop {r0-r4, pc}

// entry:
// r9: self

art_PopShadowFrameHelper:
    push {r0,lr}
    mov r0, r9
    blx PopShadowFrameFromAsm
    pop {r0, pc}
