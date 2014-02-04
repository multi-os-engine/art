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

#include "asm_support_arm64.S"

#include "arch/quick_alloc_entrypoints.S"


    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveAll)
     */
.macro SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    brk 0
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kRefsOnly).
     */
.macro SETUP_REF_ONLY_CALLEE_SAVE_FRAME
    brk 0
.endm

.macro RESTORE_REF_ONLY_CALLEE_SAVE_FRAME
    brk 0
.endm

.macro RESTORE_REF_ONLY_CALLEE_SAVE_FRAME_AND_RETURN
    brk 0
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kRefsAndArgs).
     */
.macro SETUP_REF_AND_ARGS_CALLEE_SAVE_FRAME
    brk 0
.endm

.macro RESTORE_REF_AND_ARGS_CALLEE_SAVE_FRAME
    brk 0
.endm

.macro RETURN_IF_RESULT_IS_ZERO
    brk 0
.endm

.macro RETURN_IF_RESULT_IS_NON_ZERO
    brk 0
.endm

    /*
     * Macro that set calls through to artDeliverPendingExceptionFromCode, where the pending
     * exception is Thread::Current()->exception_
     */
.macro DELIVER_PENDING_EXCEPTION
    brk 0
.endm

.macro NO_ARG_RUNTIME_EXCEPTION c_name, cxx_name
    .extern \cxx_name
ENTRY \c_name
    brk 0
END \c_name
.endm

.macro ONE_ARG_RUNTIME_EXCEPTION c_name, cxx_name
    .extern \cxx_name
ENTRY \c_name
    brk 0
END \c_name
.endm

.macro TWO_ARG_RUNTIME_EXCEPTION c_name, cxx_name
    .extern \cxx_name
ENTRY \c_name
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME  // save all registers as basis for long jump context
    brk 0
END \c_name
.endm

    /*
     * Called by managed code, saves callee saves and then calls artThrowException
     * that will place a mock Method* at the bottom of the stack. Arg1 holds the exception.
     */
ONE_ARG_RUNTIME_EXCEPTION art_quick_deliver_exception, artDeliverExceptionFromCode

    /*
     * Called by managed code to create and deliver a NullPointerException.
     */
NO_ARG_RUNTIME_EXCEPTION art_quick_throw_null_pointer_exception, artThrowNullPointerExceptionFromCode

    /*
     * Called by managed code to create and deliver an ArithmeticException.
     */
NO_ARG_RUNTIME_EXCEPTION art_quick_throw_div_zero, artThrowDivZeroFromCode

    /*
     * Called by managed code to create and deliver an ArrayIndexOutOfBoundsException. Arg1 holds
     * index, arg2 holds limit.
     */
TWO_ARG_RUNTIME_EXCEPTION art_quick_throw_array_bounds, artThrowArrayBoundsFromCode

    /*
     * Called by managed code to create and deliver a StackOverflowError.
     */
NO_ARG_RUNTIME_EXCEPTION art_quick_throw_stack_overflow, artThrowStackOverflowFromCode

    /*
     * Called by managed code to create and deliver a NoSuchMethodError.
     */
ONE_ARG_RUNTIME_EXCEPTION art_quick_throw_no_such_method, artThrowNoSuchMethodFromCode

    /*
     * All generated callsites for interface invokes and invocation slow paths will load arguments
     * as usual - except instead of loading arg0/r0 with the target Method*, arg0/r0 will contain
     * the method_idx.  This wrapper will save arg1-arg3, load the caller's Method*, align the
     * stack and call the appropriate C helper.
     * NOTE: "this" is first visible argument of the target, and so can be found in arg1/r1.
     *
     * The helper will attempt to locate the target and return a 64-bit result in r0/r1 consisting
     * of the target Method* in r0 and method->code_ in r1.
     *
     * If unsuccessful, the helper will return NULL/NULL. There will bea pending exception in the
     * thread and we branch to another stub to deliver it.
     *
     * On success this wrapper will restore arguments and *jump* to the target, leaving the lr
     * pointing back to the original caller.
     */
.macro INVOKE_TRAMPOLINE c_name, cxx_name
    .extern \cxx_name
ENTRY \c_name
    brk 0
END \c_name
.endm

INVOKE_TRAMPOLINE art_quick_invoke_interface_trampoline, artInvokeInterfaceTrampoline
INVOKE_TRAMPOLINE art_quick_invoke_interface_trampoline_with_access_check, artInvokeInterfaceTrampolineWithAccessCheck

INVOKE_TRAMPOLINE art_quick_invoke_static_trampoline_with_access_check, artInvokeStaticTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_direct_trampoline_with_access_check, artInvokeDirectTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_super_trampoline_with_access_check, artInvokeSuperTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_virtual_trampoline_with_access_check, artInvokeVirtualTrampolineWithAccessCheck

/*
 *  extern"C" void art_quick_invoke_stub(ArtMethod *method,   x0
 *                                       uint32_t  *args,     x1
 *                                       uint32_t argsize,    w2
 *                                       Thread *self,        x3
 *                                       JValue *result,      x4
 *                                       char   *shorty);     x5
 *  +----------------------+
 *  |                      |
 *  |  C/C++ frame         |
 *  |       LR''           |
 *  |       FP''           | <- SP'
 *  +----------------------+
 *  +----------------------+
 *  |        SP'           |
 *  |        X5            |
 *  |        X4            |        Saved registers
 *  |        LR'           |
 *  |        FP'           | <- FP
 *  +----------------------+
 *  | uint32_t out[n-1]    |
 *  |    :      :          |        Outs
 *  | uint32_t out[0]      |
 *  | ArtMethod* NULL      | <- SP
 *  +----------------------+
 *
 * Outgoing registers:
 *  X0    - Method*
 *  X1-X7 - integer parameters
 *  D0-D7 - Floating point parameters
 *  rSUSPEND = #SUSPEND_CHECK_INTERVAL
 *  rSELF = self
 *  SP = & of ArtMethod*
 *
 */
ENTRY art_quick_invoke_stub
    // Spill registers as per AACPS64 calling convention.

SAVE_SIZE=5*8   // x4, x5, SP, LR & FP saved
SAVE_SIZE_AND_METHOD=SAVE_SIZE+8

    mov x9, sp     // Save stack pointer.

    mov x10, x29   // Save frame pointer
    .cfi_register x29,x10
    add x11, x2, # SAVE_SIZE_AND_METHOD  // Calculate SP position - saves + ArtMethod* +  args

    and x11, x11, # ~0xf  // Enforce 16 byte stack alignment

    sub x29, sp, #SAVE_SIZE   // Calculate new FP. Don't store here until SP moved.

    mov sp, x11        // set new SP

    .cfi_def_cfa_register   29

    str x9, [x29, #32]     // Save old stack pointer

    .cfi_offset x9, 32

    stp x4, x5, [x29, #16]  // Save result and shorty addresses

    .cfi_offset x4, 16
    .cfi_offset x5, 24

    stp x30, x10, [x29]   // Store lr & old fp @ fp

    .cfi_offset x30, 0
    .cfi_offset x10, 8

    mov xSELF, x3       // Move thread pointer into SELF register
    mov xSUSPEND, #SUSPEND_CHECK_INTERVAL // Reset suspension counter

    // Copy arguments into stack frame.
    // Use simple copy routine for now.
    // 4 bytes per slot.
    // X1 - source address
    // W2 - args length
    // X10 - destination address.
    add x9, sp, #8     // Destination address is bottom of stack + NULL

    // w2 = argsize parameter
.LcopyParams:
    cmp w2, #0
    beq .LendCopyParams
    sub w2, w2, #4      // Need 65536 bytes of range.
    ldr w10, [sp, x2]
    str w10, [x9, x2]

    b .LcopyParams

.LendCopyParams:

    // Store NULL into Method* at bottom of frame
    str xzr, [sp]

    // Fill registers x/w1 to x/w7 and s/d0 to s/d7 with parameters.
    // Parse the passed shorty to determine which register to load.
    // Load addresses for routines that load WXSD registers
    adr  x11, .LstoreW1
    adr  x12, .LstoreX1
    adr  x13, .LstoreSingle0
    adr  x14, .LstoreDouble0

    // Initialize routine offsets to 0 for integers and floats.
    // x8 for integers, x15 for floating point.
    mov x8, #0
    mov x15, #0

    add x10, x5, #1         // Load shorty address, plus one to skip return value

    // Loop to fill registers
.LfillRegisters:
    ldrb w17, [x10], #1       // Load next character in signature, and increment.
    cbz w17, .LcallFunction   // Exit at end of signature. Shorty 0 terminated.

    cmp  w17, #'F' // is this a float?
    bne .LisDouble

    cmp x15, # 8*12         // Skip this load if all registers full.
    beq .LfillRegisters

    add x17, x13, x15       // Calculate subroutine to jump to
    br  x17

.LisDouble:
    cmp w17, #'D'           // is this a double?
    bne .LisLong

    cmp x15, # 8*12         // Skip this load if all registers full.
    beq .LfillRegisters


    add x17, x14, x15       // Calculate subroutine to jump to
    br x17

.LisLong:
    cmp w17, #'J'           // is this a long?
    bne .LisOther

    cmp x8, # 7*12          // Skip this load if all registers full.
    beq .LfillRegisters

    add x17, x12, x8        // Calculate subroutine to jump to
    br x17


.LisOther:                  // Everything else takes one vReg.
    cmp x8, # 7*12          // Skip this load if all registers full.
    beq .LfillRegisters
    add x17, x11, x8        // Calculate subroutine to jump to
    br x17

// Macro for loading a parameter into a register.
//  counter - the register with offset into these tables
//  size - the size of the register - 4 or 8 bytes.
//  register - the name of the register to be loaded.
.macro LOADREG counter size register
    ldr \register , [x9], #\size
    add \counter, \counter, 12
    b .LfillRegisters
.endm

// Store ints
.LstoreW1:
    LOADREG x8 4 w1
    LOADREG x8 4 w2
    LOADREG x8 4 w3
    LOADREG x8 4 w4
    LOADREG x8 4 w5
    LOADREG x8 4 w6
    LOADREG x8 4 w7

// Store longs
.LstoreX1:
    LOADREG x8 8 x1
    LOADREG x8 8 x2
    LOADREG x8 8 x3
    LOADREG x8 8 x4
    LOADREG x8 8 x5
    LOADREG x8 8 x6
    LOADREG x8 8 x7

// Store singles.
.LstoreS0:
    LOADREG x15 4 s0
    LOADREG x15 4 s1
    LOADREG x15 4 s2
    LOADREG x15 4 s3
    LOADREG x15 4 s4
    LOADREG x15 4 s5
    LOADREG x15 4 s6
    LOADREG x15 4 s7

// Store doubles.
.LstoreD0:
    LOADREG x15 8 d0
    LOADREG x15 8 d1
    LOADREG x15 8 d2
    LOADREG x15 8 d3
    LOADREG x15 8 d4
    LOADREG x15 8 d5
    LOADREG x15 8 d6
    LOADREG x15 8 d7


.LcallFunction:

    // load method-> METHOD_QUICK_CODE_OFFSET
    // TODO This constant will be wrong on 64bit.
    ldr x9, [xSELF, #METHOD_QUICK_CODE_OFFSET]
    // Branch to method.
    blr x9

    // Restore return value address and shorty address.
    ldp x4,x5, [x29, #16]
    .cfi_restore x4
    .cfi_restore x5

    // Store result (w0/x0/s0/d0) appropriately, depending on resultType
    ldrb w10, [x5]

    // Don't set anything for a void type.
    cmp w10, #'V'
    beq .exit_art_quick_invoke_stub

    cmp w10, #'D'
    bne .Lreturn_is_float
    str d0, [x4]
    b .Lexit_art_quick_invoke_stub

.Lreturn_is_float:
    cmp w10, #'F'
    bne .Lreturn_is_long
    str s0, [x4]
    b .Lexit_art_quick_invoke_stub

    // Just store x0. Doesn't matter if it is 64 or 32 bits.
.Lreturn_is_int:
    str x0, [x4]

.Lexit_art_quick_invoke_stub:

    ldp x29, x30, [x29]    // Restore old frame pointer and link register
    .cfi_restore x29
    .cfi_restore x30
    ldr x2, [x29, #32]   // Restore stack pointer.
    mov sp, x2
    .cfi_restore sp
    ret
END art_quick_invoke_stub

UNIMPLEMENTED art_quick_do_long_jump
UNIMPLEMENTED art_quick_handle_fill_data

UNIMPLEMENTED art_quick_lock_object
UNIMPLEMENTED art_quick_unlock_object
UNIMPLEMENTED art_quick_check_cast
UNIMPLEMENTED art_quick_aput_obj_with_null_and_bound_check
UNIMPLEMENTED art_quick_aput_obj_with_bound_check
UNIMPLEMENTED art_quick_aput_obj
UNIMPLEMENTED art_quick_initialize_static_storage
UNIMPLEMENTED art_quick_initialize_type
UNIMPLEMENTED art_quick_initialize_type_and_verify_access
UNIMPLEMENTED art_quick_get32_static
UNIMPLEMENTED art_quick_get64_static
UNIMPLEMENTED art_quick_get_obj_static
UNIMPLEMENTED art_quick_get32_instance
UNIMPLEMENTED art_quick_get64_instance
UNIMPLEMENTED art_quick_get_obj_instance
UNIMPLEMENTED art_quick_set32_static
UNIMPLEMENTED art_quick_set64_static
UNIMPLEMENTED art_quick_set_obj_static
UNIMPLEMENTED art_quick_set32_instance
UNIMPLEMENTED art_quick_set64_instance
UNIMPLEMENTED art_quick_set_obj_instance
UNIMPLEMENTED art_quick_resolve_string

// Macro to facilitate adding new allocation entrypoints.
.macro TWO_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    brk 0
END \name
.endm

// Macro to facilitate adding new array allocation entrypoints.
.macro THREE_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    brk 0
END \name
.endm

// Generate the allocation entrypoints for each allocator.
GENERATE_ALL_ALLOC_ENTRYPOINTS

UNIMPLEMENTED art_quick_test_suspend

/**
 * Returned by ClassLinker::GetOatCodeFor
 *
 */
UNIMPLEMENTED art_quick_proxy_invoke_handler

UNIMPLEMENTED art_quick_imt_conflict_trampoline
UNIMPLEMENTED art_quick_resolution_trampoline
UNIMPLEMENTED art_quick_to_interpreter_bridge
UNIMPLEMENTED art_quick_instrumentation_entry
UNIMPLEMENTED art_quick_deoptimize
UNIMPLEMENTED art_quick_mul_long
UNIMPLEMENTED art_quick_shl_long
UNIMPLEMENTED art_quick_shr_long
UNIMPLEMENTED art_quick_ushr_long
UNIMPLEMENTED art_quick_indexof
UNIMPLEMENTED art_quick_string_compareto
