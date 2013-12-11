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

.section ".text"
.arm

// Register usage:
// r0..r4: scratch
// r5 holds the address of the VRegReference table in the shadow frame
// r6 holds the address of the interpreter function array (helpers)
// r7 holds the address of the vregs_ fields in the current shadow frame
// r8: holds zero
// r9 is the current thread
// r10: not guaranteed to be preserved over helper functions
// r11: cached v0
// r12: cached v0 (shadow copy)


#include "xlator_macros.S"
#include "xlator_relocs.S"
#include "xlator_offsets.S"
#include "xlator_helpers.S"



// The arguments for this function are the same as that for an interpreter.  This
// is defined as the following typedef:
// typedef void (EntryPointFromInterpreter)(Thread* self, MethodHelper& mh,
//    const DexFile::CodeItem* code_item, ShadowFrame* shadow_frame, JValue* result);

// Execute translated code
// entry:
// r0: thread
// r1: address of method helper
// r2: address of code item
// r3: address of shadow frame
// [sp,#0]: address for result JValue

// This code is placed before the table of chunk addresses (the program)

// The stack contains the following information:
// [sp,#0]: Address of TranslatedMethod
// [sp,#4]: address of JValue for result
// [sp,#8]: thread pointer
// [sp,#12]: method helper
// [sp,#16]: code item
// [sp,#20]: shadow frame

.global art_xlate_code_entry
.global art_xlate_code_entry_end
.align 2
art_xlate_code_entry:
    push    {r0-r12, lr}           // 13 registers pushed
    ldr     r6, 101f                // address of helper array
    mov     r9, r0                  // thread
    ldr     r0,[r9,#thread_stack_end_offset]      // Get lowest stack address
    cmp     sp, r0                                // are we below it?
    bgt     106f
    ldr     lr, [r6, #art_ThrowStackOverflow*4]
    blx     lr
    pop     {r0-r12, pc}
106:
    add     r7, r3, #shadow_frame_vregs_offset      // set vregs pointer
    ldr     r0, [r6, #art_PushShadowFrame*4]
    blx     r0
    ldr     r1,[r3,#shadow_frame_number_of_vregs_offset]
    add     r5, r7, r1, lsl #2      // address of vref references
    adr     r0, 102f                // address of TranslatedMethod
    ldr     r1,[sp,#56]             // address of JValue for result
    push    {r0, r1}               // push TranslatedMethod and result address on stack
    mov     r8, #0                  // constant zero register
    ldr     r11, [r7, #0]
    ldr     r12, [r5, #0]
    ldr     pc, 100f                // and we're off...
101:
    .word   0       // helper array
102:
    .word   0       // program index
    .word   0       // dex pc map size
    .word   0       // address of dex pc map/whole map
    .word   0       // address of last instruction + 4
    .word   0       // number of entries in index
    .word   0       // address of ppc/dexpc map/whole map
    .word   0       // Translator
    .word   0       // next_
    .word   0       // prev_
    .word   0       // program size
    .word   0       // mirror::ArtMethod*
    .word   0       // entry_count_
100:
art_xlate_code_entry_end:
    // now follows the array of pointers to translated instructions (program index)
    // and then the translated instructions themselves

INST NOP
ENDINST NOP
ENDRELOC

// move a 32 bit value from one vreg to another
INST MOVE
1:  ldr     r0,[r7,#0]            // vreg offset
25: str     r8,[r5,#0]
2:  str     r0,[r7,#0]            // vreg offset
ENDINST MOVE
RELOC reloc_vregB_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregA_12x, 25
ENDRELOC

INST MOVE_FROM16
1:  ldr     r0,[r7,#0]            // vreg offset
25: str     r8,[r5,#0]
2:  str     r0,[r7,#0]            // vreg offset
ENDINST MOVE_FROM16
RELOC reloc_vregB_22x, 1
RELOC reloc_vregA_22x, 2
RELOC reloc_vregA_22x, 25
ENDRELOC


INST MOVE_16
1:  ldr     r0,[r7,#0]            // vreg offset
25: str     r8,[r5,#0]
2:  str     r0,[r7,#0]            // vreg offset
ENDINST MOVE_16
RELOC reloc_vregB_32x, 1
RELOC reloc_vregA_32x, 2
RELOC reloc_vregA_32x, 25
ENDRELOC


INST MOVE_WIDE
    FLUSH_INT
1:  movw    r0,#0                // src vreg
2:  movw    r1,#0                // dest vreg
    add     r0, r7, r0, lsl #2
    add     r1, r7, r1, lsl #2
    ldmia   r0,{r2,r3}            // vreg offset
    stmia   r1,{r2,r3}            // vreg offset
    RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST MOVE_WIDE
RELOC reloc_vregB_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregA_12x, 25
RELOC reloc_vregA_12x, 26
ENDRELOC

INST MOVE_WIDE_FROM16
    FLUSH_INT
1:  movw    r0,#0                // src vreg
2:  movw    r1,#0                // dest vreg
    add     r0, r7, r0, lsl #2
    add     r1, r7, r1, lsl #2
    ldmia   r0,{r2,r3}            // vreg offset
    stmia   r1,{r2,r3}            // vreg offset
    RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST MOVE_WIDE_FROM16
RELOC reloc_vregB_22x, 1
RELOC reloc_vregA_22x, 2
RELOC reloc_vregA_22x, 25
RELOC reloc_vregA_22x, 26
ENDRELOC


INST MOVE_WIDE_16
    FLUSH_INT
1:  movw    r0,#0                // src vreg
2:  movw    r1,#0                // dest vreg
    add     r0, r7, r0, lsl #2
    add     r1, r7, r1, lsl #2
    ldmia   r0,{r2,r3}            // vreg offset
    stmia   r1,{r2,r3}            // vreg offset
    RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST MOVE_WIDE_16
RELOC reloc_vregB_32x, 1
RELOC reloc_vregA_32x, 2
RELOC reloc_vregA_32x, 25
RELOC reloc_vregA_32x, 26
ENDRELOC

INST MOVE_OBJECT
1:  ldr     r0,[r5,#0]            // vregreference offset
2:  str     r0,[r7,#0]            // vreg offset
3:  str     r0,[r5,#0]            // vreg reference
ENDINST MOVE_OBJECT
RELOC reloc_vregB_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregA_12x, 3
ENDRELOC

INST MOVE_OBJECT_FROM16
1:  ldr     r0,[r5,#0]            // vregreference offset
2:  str     r0,[r7,#0]            // vreg offset
3:  str     r0,[r5,#0]            // vreg ref offset
ENDINST MOVE_OBJECT_FROM16
RELOC reloc_vregB_22x, 1
RELOC reloc_vregA_22x, 2
RELOC reloc_vregA_22x, 3
ENDRELOC

INST MOVE_OBJECT_16
1:  ldr     r0,[r5,#0]            // vregreference offset
2:  str     r0,[r7,#0]            // vreg offset
3:  str     r0,[r5,#0]            // vreg reference offset
ENDINST MOVE_OBJECT_16
RELOC reloc_vregB_32x, 1
RELOC reloc_vregA_32x, 2
RELOC reloc_vregA_32x, 3
ENDRELOC


INST MOVE_RESULT
    RESULT  r0
    ldr     r0,[r0,#0]            // result
25: str     r8,[r5,#0]
1:  str     r0,[r7,#0]            // vreg offset
ENDINST MOVE_RESULT
RELOC reloc_vregA_11x, 1
RELOC reloc_vregA_11x, 25
ENDRELOC

INST MOVE_RESULT_WIDE
    RESULT  r2
    ldmia   r2,{r0,r1}            // result
1:  movw    r2,#0                    // vreg offset
    add     r2, r7, r2, lsl #2
    stmia   r2, {r0,r1}            // vreg offset
    RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST MOVE_RESULT_WIDE
RELOC reloc_vregA_11x, 1
RELOC reloc_vregA_11x, 25
RELOC reloc_vregA_11x, 26
ENDRELOC

INST MOVE_RESULT_OBJECT
    RESULT  r1
    ldr     r0,[r1,#0]            // result
2:  str     r0,[r5,#0]
1:  str     r0,[r7,#0]            // vreg offset
ENDINST MOVE_RESULT_OBJECT
RELOC reloc_vregA_11x, 1
RELOC reloc_vregA_11x, 2
ENDRELOC

INST MOVE_EXCEPTION
    ldr     r0,[r9,#thread_exception_offset]
    mov     r1, #0
    str     r1,[r9,#thread_exception_offset]
    // XXX: clear throw_location here
2:  str     r0,[r5,#0]            // vreg reference offset
1:  str     r0,[r7,#0]            // vreg offset
ENDINST MOVE_EXCEPTION
RELOC reloc_vregA_11x, 1
RELOC reloc_vregA_11x, 2
ENDRELOC


INST RETURN_VOID
    ldrh     r0,[r9,#0]
    cmp     r0,#0
    beq     1f
2:  CALL art_CheckSuspend
1:
    METHOD_RETURN
ENDINST RETURN_VOID
RELOC reloc_helperaddr, 2
ENDRELOC

INST RETURN_VOID_BARRIER
    dmb     st
    ldrh     r0,[r9,#0]
    cmp     r0,#0
    beq     1f
2:  CALL art_CheckSuspend
1:
    METHOD_RETURN
ENDINST RETURN_VOID_BARRIER
RELOC reloc_helperaddr, 2
ENDRELOC


INST RETURN
    ldrh     r0,[r9,#0]
    cmp     r0,#0
    beq     1f
2:  CALL art_CheckSuspend
1:  ldr     r0,[r7,#0]            // vreg offset
    RESULT  r2
    str     r0,[r2,#0]
    METHOD_RETURN

ENDINST RETURN
RELOC reloc_vregA_11x, 1
RELOC reloc_helperaddr, 2
ENDRELOC


INST RETURN_WIDE
    ldrh     r0,[r9,#0]
    cmp     r0,#0
    beq     1f
3:  CALL art_CheckSuspend
1:  ldr     r0,[r7,#0]            // vreg offset
2:  ldr     r1,[r7,#4]            // vreg offset
    RESULT  r2
    stmia   r2,{r0,r1}
    METHOD_RETURN
ENDINST RETURN_WIDE
RELOC reloc_vregA_11x, 1
RELOC reloc_vregA_11x, 2
RELOC reloc_helperaddr, 3
ENDRELOC

INST RETURN_OBJECT
    ldrh     r0,[r9,#0]
    cmp     r0,#0
    beq     1f
2:  CALL art_CheckSuspend
1:  ldr     r0,[r5,#0]            // vregreference offset
    RESULT  r2
    str     r0,[r2,#0]
    // TODO: assignability_check here
    METHOD_RETURN
ENDINST RETURN_OBJECT
RELOC reloc_vregA_11x, 1
RELOC reloc_helperaddr, 2
ENDRELOC


INST CONST_4        // 4 bit constant (non zero)
1:  mov     r0,#0
2:  str     r0,[r7,#0]            // vreg offset
25: str     r8,[r5,#0]
ENDINST CONST_4
RELOC reloc_constB_11n, 1
RELOC reloc_vregA_11n, 2
RELOC reloc_vregA_11n, 25
ENDRELOC

INST CONST_4_ZERO        // 4 bit constant
1:  str     r8,[r5,#0]            // vregreference offset
2:  str     r8,[r7,#0]            // vreg offset
ENDINST CONST_4_ZERO
RELOC reloc_vregA_11n, 1
RELOC reloc_vregA_11n, 2
ENDRELOC

INST CONST_4_NEGATIVE        // 4 bit negative constant
1:  mvn     r0,#0                // positive version (but this will perform onescomp)
    add     r0, r0, #1           // add one to get correct value
2:  str     r0,[r7,#0]            // vreg offset
25: str     r8,[r5,#0]
ENDINST CONST_4_NEGATIVE
RELOC reloc_constB_11n+reloc_negative, 1
RELOC reloc_vregA_11n, 2
RELOC reloc_vregA_11n, 25
ENDRELOC

INST CONST_16       // 16 bit constant (non zero)
1:  movw    r0,#0
2:  str     r0,[r7,#0]            // vreg offset
25: str     r8,[r5,#0]
ENDINST CONST_16
RELOC reloc_constB_21s, 1
RELOC reloc_vregA_21s, 2
RELOC reloc_vregA_21s, 25
ENDRELOC

INST CONST_16_ZERO       // 16 bit constant
1:  str     r8,[r5,#0]            // vregreference offset
2:  str     r8,[r7,#0]            // vreg offset
ENDINST CONST_16_ZERO
RELOC reloc_vregA_21s, 1
RELOC reloc_vregA_21s, 2
ENDRELOC


INST CONST_16_NEGATIVE       // 16 bit negative constant
1:  movw    r0,#0               // positive value of constant
    rsb     r0, r0, #0          // substract from 0 to make negative
2:  str     r0,[r7,#0]            // vreg offset
25: str     r8,[r5,#0]
ENDINST CONST_16_NEGATIVE
RELOC reloc_constB_21s+reloc_negative, 1
RELOC reloc_vregA_21s, 2
RELOC reloc_vregA_21s, 25
ENDRELOC

INST CONST       // 32 bit constant
1:  movw    r0,#0
2:  movt    r0,#0
3:  str     r8,[r5,#0]            // vregreference offset
4:  str     r0,[r7,#0]            // vreg offset
ENDINST CONST
RELOC reloc_constB_31i+reloc_lo, 1
RELOC reloc_constB_31i+reloc_hi, 2
RELOC reloc_vregA_31i, 3
RELOC reloc_vregA_31i, 4
ENDRELOC

INST CONST_HIGH16       // 32 bit constant
    movw    r0,#0
1:  movt    r0,#0
2:  str     r8,[r5,#0]            // vregreference offset
3:  str     r0,[r7,#0]            // vreg offset
ENDINST CONST_HIGH16
RELOC reloc_constB_21h, 1
RELOC reloc_vregA_21h, 2
RELOC reloc_vregA_21h, 3
ENDRELOC


INST CONST_WIDE_16
1:  movw    r0,#0
    lsl     r0, #16         // Sign extend to 64 bits
    asr     r0, #16
    mov     r1, r0, asr #31     // copy sign bit into r1
2:  movw    r2, #0              // vreg A
    add     r2, r7,r2, lsl #2
    stmia   r2,{r0,r1}
    RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST CONST_WIDE_16
RELOC reloc_constB_21s, 1
RELOC reloc_vregA_21s, 2
RELOC reloc_vregA_21s, 25
RELOC reloc_vregA_21s, 26
ENDRELOC

INST CONST_WIDE_32
1:  movw    r0,#0        // low 16 bits
2:  movt    r0,#0        // high 16 bits
    mov     r1, r0, asr #31   // copy sign bit into r1
3:  movw    r2,#0          // vreg
    add     r2, r7,r2, lsl #2
    stmia   r2,{r0,r1}
    RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST CONST_WIDE_32
RELOC reloc_constB_31i+reloc_lo, 1
RELOC reloc_constB_31i+reloc_hi, 2
RELOC reloc_vregA_31i, 3
RELOC reloc_vregA_31i, 25
RELOC reloc_vregA_31i, 26
ENDRELOC

INST CONST_WIDE
1:  movw    r0,#0
2:  movt    r0,#0
3:  movw    r1,#0
4:  movt    r1,#0
5:  movw    r2,#0                    // vreg offset
    add     r2, r7,r2, lsl #2
    stmia   r2,{r0,r1}
    RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST CONST_WIDE
RELOC reloc_constB_51l+reloc_lo, 1
RELOC reloc_constB_51l+reloc_hi, 2
RELOC reloc_constB_51l_2+reloc_lo, 3
RELOC reloc_constB_51l_2+reloc_hi, 4
RELOC reloc_vregA_51l, 5
RELOC reloc_vregA_51l, 25
RELOC reloc_vregA_51l, 26
ENDRELOC

INST CONST_WIDE_HIGH16
    mov     r0,#0
1:  movw    r1,#0        // 16 bit
    lsl     r1, #16
2:  movw    r2,#0        // vreg result
    add     r2, r7,r2, lsl #2
    stmia   r2,{r0,r1}
    RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST CONST_WIDE_HIGH16
RELOC reloc_constB_21h, 1
RELOC reloc_vregA_21h, 2
RELOC reloc_vregA_21h, 25
RELOC reloc_vregA_21h, 26
ENDRELOC

INST CONST_STRING
1:  movw    r0,#0
2:  movt    r0,#0
5:  CALL    art_ResolveString
3:  str     r0,[r5,#0]            // vregreference offset
4:  str     r0,[r7,#0]
ENDINST CONST_STRING
RELOC reloc_constB_21c+reloc_lo, 1
RELOC reloc_constB_21c+reloc_hi, 2
RELOC reloc_vregA_21c, 3
RELOC reloc_vregA_21c, 4
RELOC reloc_helperaddr, 5
ENDRELOC

INST CONST_STRING_JUMBO
1:  movw    r0,#0
2:  movt    r0,#0
5:  CALL    art_ResolveString
3:  str     r0,[r5,#0]            // vregreference offset
4:  str     r0,[r7,#0]
ENDINST CONST_STRING_JUMBO
RELOC reloc_constB_31c+reloc_lo, 1
RELOC reloc_constB_31c+reloc_hi, 2
RELOC reloc_vregA_31c, 3
RELOC reloc_vregA_31c, 4
RELOC reloc_helperaddr, 5
ENDRELOC

INST CONST_CLASS
1:  movw    r0,#0          // field index
    ldr     r1,[r7,#(shadow_frame_method_offset - shadow_frame_vregs_offset)]  // method_ from shadow frame
4:  CALL    art_ResolveVerifyAndClinit
2:  str     r0,[r5,#0]            // vregreference offset
3:  str     r0,[r7,#0]
ENDINST CONST_CLASS
RELOC reloc_constB_21c, 1
RELOC reloc_vregA_21c, 2
RELOC reloc_vregA_21c, 3
RELOC reloc_helperaddr, 4
ENDRELOC

INST MONITOR_ENTER, attr_shared
1:  ldr     r0,[r5,#0]              // vreg
2:  CALL    art_MonitorEnter
ENDINST MONITOR_ENTER
RELOC reloc_vregA_11x, 1
RELOC reloc_helperaddr, 2
ENDRELOC

INST MONITOR_EXIT, attr_shared
1:  ldr     r0,[r5,#0]              // vreg
2:  CALL    art_MonitorExit
ENDINST MONITOR_EXIT
RELOC reloc_vregA_11x, 1
RELOC reloc_helperaddr, 2
ENDRELOC

INST CHECK_CAST, attr_shared
1:  movw    r0,#0                  // type index
    ldr     r1,[r7,#(shadow_frame_method_offset - shadow_frame_vregs_offset)] // method_
4:  CALL    art_ResolveVerifyAndClinit   // exit with r0 = class
2:  ldr     r1,[r5, #0]         // vreg reference object
    cmp     r1,#0               // null pointers are nop
    beq     3f
    mov     r2, r0
5:  CALL    art_InstanceOf
    cmp     r0,#0
    mov     r0, r2        // r0 = class, r1 = object
6:  THROWEQ art_ThrowClassCastException
3:
ENDINST CHECK_CAST
RELOC reloc_vregB_21c, 1
RELOC reloc_vregA_21c, 2
RELOC reloc_helperaddr, 4
RELOC reloc_helperaddr, 5
RELOC reloc_helperaddr, 6
ENDRELOC

INST INSTANCE_OF, attr_shared
1:  movw    r0,#0                  // type index
    ldr     r1,[r7,#(shadow_frame_method_offset - shadow_frame_vregs_offset)] // method_
4:  CALL    art_ResolveVerifyAndClinit   // exit with r0 = class
    mov     r3, #0              // assume false
2:  ldr     r1,[r5, #0]         // vreg reference object
    cmp     r1,#0               // null pointers are nop
    beq     3f
5:  CALL    art_InstanceOf
    mov     r3, r0
3:  str     r3,[r7,#0]        // store result
25: str     r8,[r5,#0]
ENDINST INSTANCE_OF
RELOC reloc_vregC_22c, 1
RELOC reloc_vregB_22c, 2
RELOC reloc_vregA_22c, 3
RELOC reloc_helperaddr, 4
RELOC reloc_helperaddr, 5
RELOC reloc_vregA_22c, 25
ENDRELOC


INST ARRAY_LENGTH
1:  ldr     r0,[r5,#0]              // vreg (Array*)
    cmp     r0,#0
3:  THROWEQ art_ThrowNullPointerException
    ldr     r0,[r0,#array_length_offset]
2:  str     r0,[r7,#0]          // store in vreg
25: str     r8,[r5,#0]
ENDINST ARRAY_LENGTH
RELOC reloc_vregB_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_helperaddr, 3
RELOC reloc_vregA_12x, 25
ENDRELOC


INST NEW_INSTANCE
1:  movw    r0, #0          // type index
4:  CALL    art_AllocObjectFromCode
2:  str     r0,[r5,#0]          // output vreg reference
3:  str     r0,[r7,#0]
ENDINST NEW_INSTANCE
RELOC reloc_vregB_21c, 1
RELOC reloc_vregA_21c, 2
RELOC reloc_vregA_21c, 3
RELOC reloc_helperaddr, 4
ENDRELOC

INST NEW_ARRAY
1:  movw    r0, #0          // type index
2:  ldr     r1,[r7,#0]          // component count
5:  CALL    art_AllocArrayFromCode
3:  str     r0,[r5,#0]          // output vreg reference
4:  str     r0,[r7,#0]
ENDINST NEW_ARRAY
RELOC reloc_vregC_22c, 1
RELOC reloc_vregB_22c, 2
RELOC reloc_vregA_22c, 3
RELOC reloc_vregA_22c, 4
RELOC reloc_helperaddr, 5
ENDRELOC


INST FILLED_NEW_ARRAY
4:  CALL    art_DoFilledNewArray
1:  .word 0
ENDINST FILLED_NEW_ARRAY
RELOC reloc_instruction, 1
RELOC reloc_helperaddr, 4
ENDRELOC

INST FILLED_NEW_ARRAY_RANGE
4:  CALL    art_DoFilledNewArrayRange
1: .word 0
ENDINST FILLED_NEW_ARRAY_RANGE
RELOC reloc_instruction, 1
RELOC reloc_helperaddr, 4
ENDRELOC

INST FILL_ARRAY_DATA
1:  ldr     r0,[r5,#0]              // array object
    cmp     r0, #0
    bne 2f
5:  CALL art_ThrowNullPointerException
2:  movw    r1, #0                 // payload address lo
3:  movt    r1, #0                 // payload address hi
    ldrh    r3,[r1,#2]             // payload->element_width
    ldr     r2,[r1,#4]              // payload->element_count
    mul     r2, r2, r3              // size in bytes
    add     r1, r1, #8              // payload data
    add     r0, r0, #array_data_offset    // array data
    mov     r4,r0
    cmp     r3,#8
    addeq   r0, r0, #4            // longs and doubles are aligned to 8 byte boundary
    // r0: destination addr
    // r1: src addr
    // r2: size in bytes
4:  CALL    art_memcpy
ENDINST FILL_ARRAY_DATA
RELOC reloc_vregA_31t, 1
RELOC reloc_const_special, 2
RELOC reloc_const_special, 3
RELOC reloc_helperaddr, 4
RELOC reloc_helperaddr, 5
ENDRELOC

INST THROW
    FLUSH_ALL
1:  ldr     r0,[r5,#0]      // exception to throw
    cmp     r0,#0
3:  THROWEQ art_ThrowNullPointerException
    mov r10, pc
2:  CALL    art_SetException
    mov r10, pc
4:  CALL  art_HandlePendingException
ENDINST THROW
RELOC reloc_vregA_11x, 1
RELOC reloc_helperaddr, 2
RELOC reloc_helperaddr, 3
RELOC reloc_helperaddr, 4
ENDRELOC

INST GOTO
1:  b 1b
ENDINST GOTO
RELOC reloc_offset, 1
ENDRELOC

INST GOTO_BACK
    ldrh     r0,[r9,#0]
    cmp     r0,#0
3:  beq    4b
1:  CALL art_CheckSuspend
2:  b 4b
ENDINST GOTO_BACK
RELOC reloc_helperaddr, 1
RELOC reloc_offset, 2
RELOC reloc_offset, 3
ENDRELOC


INST GOTO_16
1:  b 1b
ENDINST GOTO_16
RELOC reloc_offset, 1
ENDRELOC


INST GOTO_16_BACK
    ldrh     r0,[r9,#0]
    cmp     r0,#0
3:  beq     4b
2:  CALL art_CheckSuspend
1:  b 4b
ENDINST GOTO_16_BACK
RELOC reloc_offset, 1
RELOC reloc_helperaddr, 2
RELOC reloc_offset, 3
ENDRELOC

INST GOTO_32
1:  b 1b
ENDINST GOTO_32
RELOC reloc_offset, 1
ENDRELOC


INST GOTO_32_BACK
    ldrh     r0,[r9,#0]
    cmp     r0,#0
3:  beq     4b
2:  CALL art_CheckSuspend
1:  b 4b
ENDINST GOTO_32_BACK
RELOC reloc_offset, 1
RELOC reloc_helperaddr, 2
RELOC reloc_offset, 3
ENDRELOC

// TODO: do a small version of this that uses immediate operand2 instead of movw, movt
INST PACKED_SWITCH
1:  ldr     r0,[r7,#0]      // vreg switch value
2:  movw    r1,#0          // lowest value (lo)
3:  movt    r1,#0          // lowest value (hi)
    subs    r1, r0, r1     // diff from start of table
    blt     55f             // less than start, next
4:  movw    r2,#0          // size of table (lo)
5:  movt    r2,#0          // size of table (hi)
    cmp     r1,r2           // bigger than size
    bge     55f             // yes, next
6:  movw    r0,#0          // address of branch table (lo)
7:  movt    r0,#0          // address of branch table (hi)
    mov     r1, r1, lsl #2  // each table entry is 4 bytes
    ldr     r2,[r0,r1]      // load target address
    mov     pc, r2
55:
ENDINST PACKED_SWITCH
RELOC reloc_vregA_31t, 1
RELOC reloc_const_special, 2
RELOC reloc_const_special, 3
RELOC reloc_const_special, 4
RELOC reloc_const_special, 5
RELOC reloc_const_special, 6
RELOC reloc_const_special, 7
ENDRELOC

INST SPARSE_SWITCH
1:  ldr     r0,[r7,#0]      // vreg switch value
2:  movw    r2,#0          // size of table (lo)
3:  movt    r2,#0          // size of table (hi)
4:  movw    r10, #0        // keys (lo)
5:  movt    r10,#0         // keys (hi)
    mov     r1, #0          // lo = 0
    sub     r2, r2, #1      // hi = size - 1
11:
    cmp     r1, r2          // while (lo <= hi)
    bgt     12f
    add     r3, r1, r2      // mid = lo + hi
    lsr     r3, #1          // mid /= 2
    ldr     r4, [r10, r3, lsl #2]   // foundval = keys[mid]
    cmp     r0, r4          // if testval < foundval
    sublt   r2, r3, #1    // hi = mid - 1
    addgt   r1, r3, #1    // lo = mid + 1
    bne     11b
6:  movw    r0,#0          // address of branch table (lo)
7:  movt    r0,#0          // address of branch table (hi)
    mov     r3, r3, lsl #2    // tranch table entries are 4 bytes each
    ldr     r2,[r0,r3]   // load target address
    mov     pc, r2
12:
ENDINST SPARSE_SWITCH
RELOC reloc_vregA_31t, 1
RELOC reloc_const_special, 2
RELOC reloc_const_special, 3
RELOC reloc_const_special, 4
RELOC reloc_const_special, 5
RELOC reloc_const_special, 6
RELOC reloc_const_special, 7
ENDRELOC


INST CMPL_FLOAT
1:  ldr     r0,[r7,#0]
2:  ldr     r1,[r7,#0]
4:  CALL    art_CompareFloatLess
3:  str     r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST CMPL_FLOAT
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 3
RELOC reloc_vregA_23x, 25
RELOC reloc_helperaddr, 4
ENDRELOC

INST CMPG_FLOAT
1:  ldr     r0,[r7,#0]
2:  ldr     r1,[r7,#0]
4:  CALL    art_CompareFloatGreater
3:  str     r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST CMPG_FLOAT
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 3
RELOC reloc_vregA_23x, 25
RELOC reloc_helperaddr, 4
ENDRELOC

INST CMPL_DOUBLE, attr_shared
    FLUSH_INT
1:  movw    r2, #0
2:  movw    r3, #0
    add     r2, r7, r2, lsl #2
    add     r3, r7, r3, lsl #2
    ldmia   r2,{r0,r1}
    ldmia   r3,{r2,r3}
4:  CALL    art_CompareDoubleLess
3:  str     r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST CMPL_DOUBLE
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 3
RELOC reloc_vregA_23x, 25
RELOC reloc_helperaddr, 4
ENDRELOC

INST CMPG_DOUBLE, attr_shared
    FLUSH_INT
1:  movw    r2, #0
2:  movw    r3, #0
    add     r2, r7, r2, lsl #2
    add     r3, r7, r3, lsl #2
    ldmia   r2,{r0,r1}
    ldmia   r3,{r2,r3}
4:  CALL    art_CompareDoubleGreater
3:  str     r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST CMPG_DOUBLE
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 3
RELOC reloc_vregA_23x, 25
RELOC reloc_helperaddr, 4
ENDRELOC


INST CMP_LONG, attr_shared
    FLUSH_INT
1:  movw    r0, #0         // vregB
2:  movw    r1, #0         // vregC
    add     r0, r7, r0, lsl #2      // convert to address
    add     r1, r7, r1, lsl #2      // convert to address
    ldmia   r1,{r2, r3}
    ldmia   r0,{r0, r1}
    mvn     lr,#0           // assume less
    cmp     r1,r3           // signed compare high word
    blt     5f
    bgt     10f

    // hith word equal
    cmp     r0,r2
    blo     5f             // unsigned compare low
    bhi     10f
    mov     lr,#0
    b       5f
10:
    mov     lr,#1
5:  str     lr,[r7,#0]
25: str     r8,[r5,#0]
ENDINST CMP_LONG
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 5
RELOC reloc_vregA_23x, 25
ENDRELOC


//
// compare and branch instructions
//

IF_XX EQ,eq, ne
IF_XX NE,ne, eq
IF_XX LT,lt, ge
IF_XX GT,gt, le
IF_XX LE,le, gt
IF_XX GE,ge, lt


IF_XXZ EQZ,eq, ne
IF_XXZ NEZ,ne, eq
IF_XXZ LTZ,lt, ge
IF_XXZ GTZ,gt, le
IF_XXZ LEZ,le, gt
IF_XXZ GEZ,ge, lt



//
// arithmetic operations
//

UNOP_INT NEG_INT, "rsb r0, r0, #0"
UNOP_INT NOT_INT, "mvn r0, r0"

UNOP_LONG NEG_LONG, "rsbs r0, r0, #0", "rsc r1, r1, #0"
UNOP_LONG NOT_LONG, "mvn r0, r0", "mvn r1, r1"

// easy binary int opcodes
BINOP_INT ADD_INT, add
BINOP_INT SUB_INT, sub
BINOP_INT MUL_INT, mul
BINOP_INT2 SHL_INT, lsl, "and r1, r1, #31"
BINOP_INT2 SHR_INT, asr, "and r1, r1, #31"
BINOP_INT2 USHR_INT, lsr, "and r1, r1, #31"
BINOP_INT AND_INT, and
BINOP_INT OR_INT, orr
BINOP_INT XOR_INT, eor


// easy long binary ops
BINOP_LONG ADD_LONG, adds, adc
BINOP_LONG SUB_LONG, subs, sbc
BINOP_LONG AND_LONG, and, and
BINOP_LONG OR_LONG, orr, orr
BINOP_LONG XOR_LONG, eor, eor

// easy binary int ops (2 addr format)
BINOP_INT_2ADDR ADD_INT, add
BINOP_INT_2ADDR SUB_INT, sub
BINOP_INT_2ADDR MUL_INT, mul
BINOP_INT_2ADDR2 SHL_INT, lsl, "and r1, r1, #31"
BINOP_INT_2ADDR2 SHR_INT, asr, "and r1, r1, #31"
BINOP_INT_2ADDR2 USHR_INT, lsr, "and r1, r1, #31"
BINOP_INT_2ADDR AND_INT, and
BINOP_INT_2ADDR OR_INT, orr
BINOP_INT_2ADDR XOR_INT, eor


// easy long binary ops (2 addr format)
BINOP_LONG_2ADDR ADD_LONG, adds, adc
BINOP_LONG_2ADDR SUB_LONG, subs, sbc
BINOP_LONG_2ADDR AND_LONG, and, and
BINOP_LONG_2ADDR OR_LONG, orr, orr
BINOP_LONG_2ADDR XOR_LONG, eor, eor



// the following div and rem instructions are for
// architectures that have an sdiv instruction.  The
// _SLOW version are used for those without
INST DIV_INT
1:  ldr     r0,[r7,#0]              // vreg
2:  ldr     r1,[r7,#0]              // vreg
    cmp     r1,#0
4:  THROWEQ art_ThrowDivideByZeroException
    sdiv    r0, r0, r1
3:  str     r0, [r7, #0]            // vreg
25: str     r8,[r5,#0]
ENDINST DIV_INT
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 3
RELOC reloc_vregA_23x, 25
RELOC reloc_helperaddr, 4
ENDRELOC


// t = x / y
// t = t * y
// r = x - t

INST REM_INT
1:  ldr     r0,[r7,#0]              // vreg
2:  ldr     r1,[r7,#0]              // vreg
    cmp     r1,#0
4:  THROWEQ art_ThrowDivideByZeroException
    sdiv    r2, r0, r1
    mul     r2, r1
    sub     r0, r0, r2
3:  str     r0, [r7, #0]            // vreg
25: str     r8,[r5,#0]
ENDINST REM_INT
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 3
RELOC reloc_vregA_23x, 25
RELOC reloc_helperaddr, 4
ENDRELOC

INST DIV_INT_SLOW
1:  movw    r0,#0          // result vreg
2:  ldr     r1,[r7,#0]          // vreg
3:  ldr     r2,[r7,#0]
4:  CALL    art_DoIntDivide
25: str     r8,[r5,#0]
ENDINST DIV_INT_SLOW
RELOC reloc_vregA_23x, 1
RELOC reloc_vregB_23x, 2
RELOC reloc_vregC_23x, 3
RELOC reloc_helperaddr, 4
RELOC reloc_vregA_23x, 25
ENDRELOC


INST REM_INT_SLOW
1:  movw    r0,#0          // result vreg
2:  ldr     r1,[r7,#0]          // vreg
3:  ldr     r2,[r7,#0]
4:  CALL    art_DoIntRemainder
25: str     r8,[r5,#0]
ENDINST REM_SLOW
RELOC reloc_vregA_23x, 1
RELOC reloc_vregB_23x, 2
RELOC reloc_vregC_23x, 3
RELOC reloc_helperaddr, 4
RELOC reloc_vregA_23x, 25
ENDRELOC

INST DIV_LONG, attr_shared
    FLUSH_INT
1:  movw    r0,#0          // result vreg
2:  movw    r1, #0
3:  movw    r2, #0
    add     r1, r7, r1, lsl #2
    add     r2, r7, r2, lsl #2
    ldmia   r2,{r3,r4}
    ldmia   r1,{r1,r2}
4:  CALL    art_DoLongDivide
    RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST DIV_LONG
RELOC reloc_vregA_23x, 1
RELOC reloc_vregB_23x, 2
RELOC reloc_vregC_23x, 3
RELOC reloc_helperaddr, 4
RELOC reloc_vregA_23x, 25
RELOC reloc_vregA_23x, 26
ENDRELOC


INST REM_LONG, attr_shared
    FLUSH_INT
1:  movw    r0,#0          // result vreg
2:  movw    r1, #0
3:  movw    r2, #0
    add     r1, r7, r1, lsl #2
    add     r2, r7, r2, lsl #2
    ldmia   r2,{r3,r4}
    ldmia   r1,{r1,r2}
4:  CALL    art_DoLongRemainder
    RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST REM_LONG
RELOC reloc_vregA_23x, 1
RELOC reloc_vregB_23x, 2
RELOC reloc_vregC_23x, 3
RELOC reloc_helperaddr, 4
RELOC reloc_vregA_23x, 25
RELOC reloc_vregA_23x, 26
ENDRELOC

// 2 addr format for div and rem
// the following div and rem instructions are for
// architectures that have an sdiv instruction.  The
// _SLOW version are used for those without
INST DIV_INT_2ADDR
1:  ldr     r0,[r7,#0]              // vreg
2:  ldr     r1,[r7,#0]              // vreg
    cmp     r0,#0
4:  THROWEQ art_ThrowDivideByZeroException
    sdiv    r0, r0, r1
3:  str     r0, [r7, #0]            // vreg
25: str     r8,[r5,#0]
ENDINST DIV_INT_2ADDR
RELOC reloc_vregA_12x, 1
RELOC reloc_vregB_12x, 2
RELOC reloc_vregA_12x, 3
RELOC reloc_helperaddr, 4
RELOC reloc_vregA_12x, 25
ENDRELOC


// t = x / y
// t = t * y
// r = x - t

INST REM_INT_2ADDR
1:  ldr     r0,[r7,#0]              // vreg
2:  ldr     r1,[r7,#0]              // vreg
    cmp     r0,#0
4:  THROWEQ art_ThrowDivideByZeroException
    sdiv    r2, r0, r1
    mul     r2, r1
    sub     r0, r0, r2
3:  str     r0, [r7, #0]            // vreg
25: str     r8,[r5,#0]
ENDINST REM_INT_2ADDR
RELOC reloc_vregA_12x, 1
RELOC reloc_vregB_12x, 2
RELOC reloc_vregA_12x, 3
RELOC reloc_helperaddr, 4
RELOC reloc_vregA_12x, 25
ENDRELOC

INST DIV_INT_SLOW_2ADDR
    FLUSH_INT
1:  movw    r0, #0          // vreg number
2:  ldr     r1,[r7,#0]          // vreg
3:  ldr     r2,[r7,#0]
4:  CALL    art_DoIntDivide
25: str     r8,[r5,#0]
ENDINST DIV_INT_SLOW_2ADDR
RELOC reloc_vregA_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregB_12x, 3
RELOC reloc_helperaddr, 4
RELOC reloc_vregA_12x, 25
ENDRELOC


INST REM_INT_SLOW_2ADDR
    FLUSH_INT
1:  movw    r0, #0          // vreg number
2:  ldr     r1,[r7,#0]          // vreg
3:  ldr     r2,[r7,#0]
4:  CALL    art_DoIntRemainder
25: str     r8,[r5,#0]
ENDINST REM_SLOW_2ADDR
RELOC reloc_vregA_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregB_12x, 3
RELOC reloc_helperaddr, 4
RELOC reloc_vregA_12x, 25
ENDRELOC

INST DIV_LONG_2ADDR
    FLUSH_INT
1:  movw    r0,#0          // result vreg
2:  movw    r2, #0
    add     r1, r7, r0, lsl #2
    add     r2, r7, r2, lsl #2
    ldmia   r2,{r3,r4}
    ldmia   r1,{r1,r2}
4:  CALL    art_DoLongDivide
    RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST DIV_LONG_2ADDR
RELOC reloc_vregA_12x, 1
RELOC reloc_vregB_12x, 2
RELOC reloc_helperaddr, 4
RELOC reloc_vregA_12x, 25
RELOC reloc_vregA_12x, 26
ENDRELOC


INST REM_LONG_2ADDR
    FLUSH_INT
1:  movw    r0,#0          // result vreg
2:  movw    r2, #0
    add     r1, r7, r0, lsl #2
    add     r2, r7, r2, lsl #2
    ldmia   r2,{r3,r4}
    ldmia   r1,{r1,r2}
4:  CALL    art_DoLongRemainder
    RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST REM_LONG_2ADDR
RELOC reloc_vregA_12x, 1
RELOC reloc_vregB_12x, 2
RELOC reloc_helperaddr, 4
RELOC reloc_vregA_12x, 25
RELOC reloc_vregA_12x, 26
ENDRELOC

INST MUL_LONG, attr_shared
    FLUSH_INT
1: movw     r0, #0      // vreg B
2: movw     r2, #0      // vreg C
   add      r0, r7, r0, lsl #2
   add      r2, r7, r2, lsl #2
   ldmia    r0,{r0,r1}     // r0, r1 contain lhs
   ldmia    r2,{r2,r3}     // r2, r2 contain rhs
   mul      r10, r2, r1
   umull    r4, lr, r2, r0
   mla      r2, r0, r3, r10
   add      lr, r2, lr
   // r4 and lr contain the result
3: movw     r2, #0            // result A
   add      r2, r7, r2, lsl #2
   stmia    r2,{r4,lr}       // store result
   RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST MUL_LONG
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 3
RELOC reloc_vregA_23x, 25
RELOC reloc_vregA_23x, 26
ENDRELOC

INST MUL_LONG_2ADDR, attr_shared
    FLUSH_INT
1: movw     r0, #0      // vreg A
2: movw     r2, #0      // vreg B
   add      r0, r7, r0, lsl #2
   add      r2, r7, r2, lsl #2
   ldmia    r0,{r0,r1}     // r0, r1 contain lhs
   ldmia    r2,{r2,r3}     // r2, r2 contain rhs
   mul      r10, r2, r1
   umull    r4, lr, r2, r0
   mla      r2, r0, r3, r10
   add      lr, r2, lr
   // r4 and lr contain the result
3: movw     r2, #0            // result A
   add      r2, r7, r2, lsl #2
   stmia    r2,{r4,lr}       // store result
   RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST MUL_LONG_2ADDR
RELOC reloc_vregA_12x, 1
RELOC reloc_vregB_12x, 2
RELOC reloc_vregA_12x, 3
RELOC reloc_vregA_12x, 25
RELOC reloc_vregA_12x, 26
ENDRELOC


INST SHR_LONG, attr_shared
1:  ldr     r0, [r7,#0]
2:  ldr     r1, [r7,#4]
3:  ldr     r2, [r7,#0]
    and     r2, r2, #0x3f
    mov     r0, r0, lsr r2
    rsb     r3, r2, #32
    orr     r0, r0, r1, asl r3
    subs    r4, r2, #32
    movpl   r0, r1, lsr r4
    mov     r1, r1, asr r2
4:  str     r0, [r7,#0]         // store hi word
5:  str     r1, [r7,#4]         // store lo word
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST SHR_LONG
RELOC reloc_vregB_23x, 1
RELOC reloc_vregB_23x, 2
RELOC reloc_vregC_23x, 3
RELOC reloc_vregA_23x, 4
RELOC reloc_vregA_23x, 5
RELOC reloc_vregA_23x, 25
RELOC reloc_vregA_23x, 26
ENDRELOC

INST USHR_LONG, attr_shared
1:  ldr     r0, [r7,#0]
2:  ldr     r1, [r7,#4]
3:  ldr     r2, [r7,#0]
    mov     r0, r0, lsr r2
    rsb     r3, r2, #32
    orr     r0, r0, r1, asl r3
    subs    r4, r2, #32
    movpl   r0, r1, lsr r4
    mov     r1, r1, lsr r2
4:  str     r0, [r7,#0]         // store hi word
5:  str     r1, [r7,#4]         // store lo word
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST USHR_LONG
RELOC reloc_vregB_23x, 1
RELOC reloc_vregB_23x, 2
RELOC reloc_vregC_23x, 3
RELOC reloc_vregA_23x, 4
RELOC reloc_vregA_23x, 5
RELOC reloc_vregA_23x, 25
RELOC reloc_vregA_23x, 26
ENDRELOC

// two registers A and B (A is low word)

INST SHL_LONG, attr_shared
1:  ldr     r0, [r7,#0]         // lo
2:  ldr     r1, [r7,#4]         // hi
3:  ldr     r2, [r7,#0]         // x
    and     r2, r2, #0x3f
    mov     r1, r1, lsl r2      // hi <<= x
    rsb     r3, r2, #32         // t = 32 - x
    orr     r1, r1, r0, lsr r3  // or in top bits of lo
    subs    r4, r2, #32        // was the shift > 32
    movpl   r1, r0, lsl r4    // if so, hi is lo << x-32
    mov     r0, r0, lsl r2      // set lo
4:  str     r0, [r7,#0]         // store hi word
5:  str     r1, [r7,#4]         // store lo word
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST SHL_LONG
RELOC reloc_vregB_23x, 1
RELOC reloc_vregB_23x, 2
RELOC reloc_vregC_23x, 3
RELOC reloc_vregA_23x, 4
RELOC reloc_vregA_23x, 5
RELOC reloc_vregA_23x, 25
RELOC reloc_vregA_23x, 26
ENDRELOC



// 2 addr version

INST SHR_LONG_2ADDR, attr_shared
1:  ldr     r0, [r7,#0]
2:  ldr     r1, [r7,#4]
3:  ldr     r2, [r7,#0]
    and     r2, r2, #0x3f
    mov     r0, r0, lsr r2
    rsb     r3, r2, #32
    orr     r0, r0, r1, asl r3
    subs    r4, r2, #32
    movpl   r0, r1, lsr r4
    mov     r1, r1, asr r2
4:  str     r0, [r7,#0]         // store hi word
5:  str     r1, [r7,#4]         // store lo word
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST SHR_LONG_2ADDR
RELOC reloc_vregA_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregB_12x, 3
RELOC reloc_vregA_12x, 4
RELOC reloc_vregA_12x, 5
RELOC reloc_vregA_12x, 25
RELOC reloc_vregA_12x, 26
ENDRELOC

INST USHR_LONG_2ADDR, attr_shared
1:  ldr     r0, [r7,#0]
2:  ldr     r1, [r7,#4]
3:  ldr     r2, [r7,#0]
    and     r2, r2, #0x3f
    mov     r0, r0, lsr r2
    rsb     r3, r2, #32
    orr     r0, r0, r1, asl r3
    subs    r4, r2, #32
    movpl   r0, r1, lsr r4
    mov     r1, r1, lsr r2
4:  str     r0, [r7,#0]         // store hi word
5:  str     r1, [r7,#4]         // store lo word
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST USHR_LONG_2ADDR
RELOC reloc_vregA_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregB_12x, 3
RELOC reloc_vregA_12x, 4
RELOC reloc_vregA_12x, 5
RELOC reloc_vregA_12x, 25
RELOC reloc_vregA_12x, 26
ENDRELOC


INST SHL_LONG_2ADDR, attr_shared
1:  ldr     r0, [r7,#0]
2:  ldr     r1, [r7,#4]
3:  ldr     r2, [r7,#0]
    and     r2, r2, #0x3f
    mov     r1, r1, lsl r2      // hi <<= x
    rsb     r3, r2, #32         // t = 32 - x
    orr     r1, r1, r0, lsr r3  // or in top bits of lo
    subs    r4, r2, #32        // was the shift > 32
    movpl   r1, r0, lsl r4    // if so, hi is lo << x-32
    mov     r0, r0, lsl r2      // set lo
4:  str     r0, [r7,#0]         // store hi word
5:  str     r1, [r7,#4]         // store lo word
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST SHL_LONG_2ADDR
RELOC reloc_vregA_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregB_12x, 3
RELOC reloc_vregA_12x, 4
RELOC reloc_vregA_12x, 5
RELOC reloc_vregA_12x, 25
RELOC reloc_vregA_12x, 26

ENDRELOC


// floating point

// single precision
BINOP_FLOAT ADD_FLOAT, fadds
BINOP_FLOAT SUB_FLOAT, fsubs
BINOP_FLOAT MUL_FLOAT, fmuls
BINOP_FLOAT DIV_FLOAT, fdivs

BINOP_FLOAT_2ADDR ADD_FLOAT, fadds
BINOP_FLOAT_2ADDR SUB_FLOAT, fsubs
BINOP_FLOAT_2ADDR MUL_FLOAT, fmuls
BINOP_FLOAT_2ADDR DIV_FLOAT, fdivs

// Note, the UNOP_INT is correct
UNOP_INT NEG_FLOAT, "add r0, r0, #0x80000000"

INST REM_FLOAT
1:  ldr     r0,[r7,#0]
2:  ldr     r1,[r7,#0]
4:  CALL    art_fmodf
3:  str     r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST REM_FLOAT
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 3
RELOC reloc_vregA_23x, 25
RELOC reloc_helperaddr, 4
ENDRELOC

INST REM_FLOAT_2ADDR
1:  ldr     r0,[r7,#0]
2:  ldr     r1,[r7,#0]
4:  CALL    art_fmodf
3:  str     r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST REM_FLOAT_2ADDR
RELOC reloc_vregA_12x, 1
RELOC reloc_vregB_12x, 2
RELOC reloc_vregA_12x, 3
RELOC reloc_vregA_12x, 25
RELOC reloc_helperaddr, 4
ENDRELOC

// double precision
BINOP_DOUBLE ADD_DOUBLE, faddd
BINOP_DOUBLE SUB_DOUBLE, fsubd
BINOP_DOUBLE MUL_DOUBLE, fmuld
BINOP_DOUBLE DIV_DOUBLE, fdivd

BINOP_DOUBLE_2ADDR ADD_DOUBLE, faddd
BINOP_DOUBLE_2ADDR SUB_DOUBLE, fsubd
BINOP_DOUBLE_2ADDR MUL_DOUBLE, fmuld
BINOP_DOUBLE_2ADDR DIV_DOUBLE, fdivd

INST NEG_DOUBLE
1:  ldr     r0,[r7,#0]              // vreg
2:  ldr     r1,[r7,#4]              // vreg
    add     r1, r1, #0x80000000
3:  str     r0, [r7, #0]            // vreg
4:  str     r1, [r7, #4]            // vreg
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST NEG_DOUBLE
RELOC reloc_vregB_12x, 1
RELOC reloc_vregB_12x, 2
RELOC reloc_vregA_12x, 3
RELOC reloc_vregA_12x, 4
RELOC reloc_vregA_12x, 25
RELOC reloc_vregA_12x, 26
ENDRELOC

INST REM_DOUBLE, attr_shared
    FLUSH_INT
1:  movw    r2, #0
2:  movw    r3, #0
    add     r2, r7, r2, lsl #2
    add     r3, r7, r3, lsl #2
    ldmia   r2,{r0,r1}
    ldmia   r3,{r2,r3}
4:  CALL    art_fmod
3:  movw    r2, #0
    add     r2, r7, r2, lsl #2
    stmia   r2,{r0, r1}
    RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST REM_DOUBLE
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 3
RELOC reloc_vregA_23x, 25
RELOC reloc_vregA_23x, 26
RELOC reloc_helperaddr, 4
ENDRELOC

INST REM_DOUBLE_2ADDR, attr_shared
    FLUSH_INT
1:  movw    r4,#0
2:  movw    r2,#0
    add     r4, r7, r4, lsl #2
    add     r2, r7, r2, lsl #2
    ldmia   r4,{r0,r1}
    ldmia   r2,{r2,r3}
4:  CALL    art_fmod
    stmia   r4,{r0, r1}
    RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST REM_DOUBLE_2ADDR
RELOC reloc_vregA_12x, 1
RELOC reloc_vregB_12x, 2
RELOC reloc_vregB_12x, 25
RELOC reloc_vregB_12x, 26
RELOC reloc_helperaddr, 4
ENDRELOC

// conversions

INST INT_TO_BYTE
1:  ldr r0,[r7,#0]      // low word
    sxtb r0, r0
2:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST INT_TO_BYTE
RELOC reloc_vregB_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregA_12x, 25
ENDRELOC

INST INT_TO_SHORT
1:  ldr r0,[r7,#0]      // low word
    sxth r0, r0
2:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST INT_TO_SHORT
RELOC reloc_vregB_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregA_12x, 25
ENDRELOC

INST INT_TO_CHAR
1:  ldr r0,[r7,#0]      // low word
    uxth r0, r0
2:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST INT_TO_CHAR
RELOC reloc_vregB_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregA_12x, 25
ENDRELOC


INST INT_TO_LONG
1:  ldr r0,[r7,#0]      // low word
2:  str r0,[r7,#0]
    mov r0, r0, asr #31   // fill with sign bit
3:  str r0,[r7,#4]
25: str     r8,[r5,#0]
ENDINST INT_TO_LONG
RELOC reloc_vregB_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregA_12x, 3
RELOC reloc_vregA_12x, 25
ENDRELOC


INST INT_TO_FLOAT
    FLUSH_INT
1:  vldr.32 s0,[r7,#0]
    vcvt.f32.s32 s0, s0
2:  vstr.32 s0,[r7,#0]
// 1:  movw r2,#0
//     add r2, r7, r2, lsl #2
//     flds s0,[r2,#0]
//     fsitos s0, s0
// 2:  movw r2, #0
//     add r2, r7, r2, lsl #2
//     fsts s0,[r2,#0]
25: str     r8,[r5,#0]
ENDINST INT_TO_FLOAT
RELOC reloc_vregB_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregA_12x, 25
ENDRELOC

INST INT_TO_DOUBLE
    FLUSH_INT
1:  vldr.32 s0,[r7,#0]
    vcvt.f64.s32 d0, s0
2:  vstr.64 d0,[r7,#0]
//1:  movw r2,#0
//    add r2, r7, r2, lsl #2
//    fldd d0,[r2,#0]
//    fsitod d0, s0
//2:  movw r2, #0
//    add r2, r7, r2, lsl #2
//    fstd d0,[r2,#0]
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST INT_TO_DOUBLE
RELOC reloc_vregB_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregA_12x, 25
RELOC reloc_vregA_12x, 26
ENDRELOC

INST LONG_TO_FLOAT
1:  ldr r0,[r7,#0]
2:  ldr r1,[r7,#4]
    vldr.64 d3, 31f
    vmov.f64 d0, r0, r1
    vcvt.f64.s32 d1, s1
    vcvt.f64.u32 d2, s0
    vmla.f64 d2, d1, d3
    vcvt.f32.f64 s0, d2
3:  vstr.32 s0,[r7,#0]
25: str     r8,[r5,#0]
   b 30f
31: .hword 0
    .hword 0
    .hword 0
    .hword 0x41f0
30:
ENDINST LONG_TO_FLOAT
RELOC reloc_vregB_12x, 1
RELOC reloc_vregB_12x, 2
RELOC reloc_vregA_12x, 3
RELOC reloc_vregA_12x, 25
ENDRELOC

INST LONG_TO_DOUBLE
1:  ldr r0,[r7,#0]
2:  ldr r1,[r7,#4]
    vldr.64 d3, 31f
    vmov.f64 d0, r0, r1
    vcvt.f64.s32 d1, s1
    vcvt.f64.u32 d2, s0
    vmla.f64 d2, d1, d3
3:  vstr.64 d2,[r7,#0]
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
    b 30f
31: .hword 0
    .hword 0
    .hword 0
    .hword 0x41f0
30:
ENDINST LONG_TO_DOUBLE
RELOC reloc_vregB_12x, 1
RELOC reloc_vregB_12x, 2
RELOC reloc_vregA_12x, 3
RELOC reloc_vregA_12x, 25
RELOC reloc_vregA_12x, 26
ENDRELOC



INST LONG_TO_INT
1:  ldr r0,[r7,#0]      // low word
2:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST LONG_TO_INT
RELOC reloc_vregB_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregA_12x, 25
ENDRELOC

INST FLOAT_TO_INT
    FLUSH_INT
1:  vldr.32 s0,[r7,#0]
    vcvt.s32.f32 s0, s0
2:  vstr.32 s0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST FLOAT_TO_INT
RELOC reloc_vregB_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregA_12x, 25
//RELOC reloc_helperaddr, 4
ENDRELOC

INST FLOAT_TO_LONG
1:  ldr r0,[r7,#0]
4:  CALL art_FloatToLong
2:  movw r2,#0
    add r2, r7, r2, lsl #2
    stmia r2,{r0, r1}
    RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST FLOAT_TO_LONG
RELOC reloc_vregB_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregA_12x, 25
RELOC reloc_vregA_12x, 26
RELOC reloc_helperaddr, 4
ENDRELOC

INST FLOAT_TO_DOUBLE
    FLUSH_INT
1:  vldr.32 s0,[r7,#0]
    vcvt.f64.f32 d0, s0
2:  vstr.64 d0,[r7,#0]
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST FLOAT_TO_DOUBLE
RELOC reloc_vregB_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregA_12x, 25
RELOC reloc_vregA_12x, 26
ENDRELOC


INST DOUBLE_TO_INT
    FLUSH_INT
1:  vldr.64 d0,[r7,#0]
    vcvt.s32.f64 s0, d0
//1:  ldr r0,[r7,#0]
//2:  ldr r1,[r7,#4]
//4:  CALL art_DoubleToInt
3:  vstr.32 s0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST DOUBLE_TO_INT
RELOC reloc_vregB_12x, 1
//RELOC reloc_vregB_12x, 2
RELOC reloc_vregA_12x, 3
RELOC reloc_vregA_12x, 25
//RELOC reloc_helperaddr, 4
ENDRELOC

INST DOUBLE_TO_LONG
1:  ldr r0,[r7, #0]
2:  ldr r1,[r7, #4]
4:  CALL art_DoubleToLong
5:  str r0,[r7,#0]
6:  str r1,[r7,#4]
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST DOUBLE_TO_LONG
RELOC reloc_vregB_12x, 1
RELOC reloc_vregB_12x, 2
RELOC reloc_vregA_12x, 5
RELOC reloc_vregA_12x, 6
RELOC reloc_vregA_12x, 25
RELOC reloc_vregA_12x, 26
RELOC reloc_helperaddr, 4
ENDRELOC

INST DOUBLE_TO_FLOAT
    FLUSH_INT
1:  vldr.64 d0, [r7, #0]
    vcvt.f32.f64 s0, d0
2:  vstr.32 s0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST DOUBLE_TO_FLOAT
RELOC reloc_vregB_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregA_12x, 25
ENDRELOC


// array get and put

.macro AGET type,load,shift
INST AGET\type
1:  ldr r2,[r5,#0]      // array object
2:  ldr r0,[r7,#0]      // index
    cmp r2,#0
4:  THROWEQ art_ThrowNullPointerException
    ldr r1,[r2,#array_length_offset]      // array length
    cmp r0, r1
5:  THROWCS art_ThrowArrayIndexOutOfBoundsException
    add r1, r2, r0, lsl #\shift
    \load r0,[r1, #array_data_offset]         // load data
25: str     r8,[r5,#0]
3:  str r0,[r7,#0]              // store result
ENDINST AGET\type
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 3
RELOC reloc_vregA_23x, 25
RELOC reloc_helperaddr, 4
RELOC reloc_helperaddr, 5
ENDRELOC
.endm


.macro APUT type,store,shift
INST APUT\type
1:  ldr r2,[r5,#0]      // array object
    cmp r2,#0
4:  THROWEQ art_ThrowNullPointerException
2:  ldr r0,[r7,#0]      // index
    ldr r1,[r2,#array_length_offset]      // array length
    cmp r0, r1
5:  THROWCS art_ThrowArrayIndexOutOfBoundsException
    add r1, r2, r0, lsl #\shift
3:  ldr r0,[R7,#0]                              // load value
    \store r0,[r1, #array_data_offset]         // store data
ENDINST APUT\type
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 3
RELOC reloc_helperaddr, 4
RELOC reloc_helperaddr, 5
ENDRELOC
.endm


AGET _BOOLEAN,ldrb,0
AGET _BYTE,ldrsb,0
AGET ,ldr,2
AGET _CHAR,ldrh,1
AGET _SHORT,ldrsh,1


INST AGET_OBJECT
1:  ldr r2,[r5,#0]      // array object
    cmp r2,#0
5:  THROWEQ art_ThrowNullPointerException
2:  ldr r0,[r7,#0]      // index
    ldr r1,[r2,#array_length_offset]      // array length
    cmp r0, r1
6:  THROWCS art_ThrowArrayIndexOutOfBoundsException
    add r0, r2, r0, lsl #2
    ldr r0,[r0, #array_data_offset]         // load data
3:  str r0,[r7,#0]              // store result
4:  str r0,[r5,#0]              // store result
ENDINST AGET_OBJECT
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 3
RELOC reloc_vregA_23x, 4
RELOC reloc_helperaddr, 5
RELOC reloc_helperaddr, 6
ENDRELOC

APUT _BOOLEAN,strb,0
APUT _BYTE,strb,0
APUT ,str,2
APUT _CHAR,strh,1
APUT _SHORT,strh,1

INST AGET_WIDE
1:  ldr r2,[r5,#0]      // array object
    cmp r2,#0
4:  THROWEQ art_ThrowNullPointerException
2:  ldr r0,[r7,#0]      // index
    ldr r1,[r2,#array_length_offset]      // array length
    cmp r0, r1
5:  THROWCS art_ThrowArrayIndexOutOfBoundsException
    add r2, r2, #array_data_offset+4          // data address + 4 (need 8 byte alignment)
    add r0, r2, r0, lsl #3
    ldmia r0, {r2,r3}                         // load data
3:  movw r0,#0                  // result vreg
    add r0,r7, r0, lsl #2
    stmia r0,{r2,r3}              // store result
    RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST AGET_WIDE
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 3
RELOC reloc_vregA_23x, 25
RELOC reloc_vregA_23x, 26
RELOC reloc_helperaddr, 4
RELOC reloc_helperaddr, 5
ENDRELOC


INST APUT_WIDE
1:  ldr r2,[r5,#0]      // array object
    cmp r2,#0
4:  THROWEQ art_ThrowNullPointerException
2:  ldr r0,[r7,#0]      // index
    ldr r1,[r2,#array_length_offset]      // array length
    cmp r0, r1
5:  THROWCS art_ThrowArrayIndexOutOfBoundsException
    add r2, r2, #array_data_offset+4          // data address + 4 (need 8 byte alignment)
    add r0, r2, r0, lsl #3
    FLUSH_INT
3:  movw r1,#0            // vreg
    add r1, r7, r1, lsl #2
    ldmia r1,{r2,r3}              // load data
    stmia r0,{r2,r3}        // store result
ENDINST APUT_WIDE
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 3
RELOC reloc_helperaddr, 4
RELOC reloc_helperaddr, 5
ENDRELOC

INST APUT_OBJECT
1:  ldr r2,[r5,#0]      // array object
    cmp r2,#0
5:  THROWEQ art_ThrowNullPointerException
2:  ldr r4,[r7,#0]      // index
    ldr r1,[r2,#array_length_offset]      // array length
    cmp r4, r1
6:  THROWCS art_ThrowArrayIndexOutOfBoundsException
    add r3, r2, #array_data_offset
    add r3, r3, r4, lsl #2
3:  ldr r1,[r5,#0]                              // load value
    cmp r1,#0                   // null pointer?
    beq 10f
    // check that the assignment is possible
4:  CALL art_CheckArrayAssign     // saves r0-r4
10:
    str r1,[r3, #0]         // store data
    mov r0, r2              // destination
    mov r2, r1              // new value
    mov r1, r4, lsl #2      // offset in bytes
7:  CALL art_WriteBarrierField

ENDINST APUT_OBJECT
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 3
RELOC reloc_helperaddr, 4
RELOC reloc_helperaddr, 5
RELOC reloc_helperaddr, 6
RELOC reloc_helperaddr, 7
ENDRELOC


//
// Field access
//

IGET ,ldr, kPrimInt, InstancePrimitiveRead
IGET _BOOLEAN, ldrb, kPrimBoolean, InstancePrimitiveRead
IGET _BYTE, ldrsb, kPrimByte, InstancePrimitiveRead
IGET _SHORT, ldrsh, kPrimShort, InstancePrimitiveRead
IGET _CHAR, ldrh, kPrimChar, InstancePrimitiveRead

INST IGET_OBJECT, attr_shared
1:  movw r0, #0             // field index
    mov r1, #kPrimNot
5:  CALL art_ResolveField_InstanceObjectRead
    mov r1, r0
2:  ldr r2,[r5,#0]          // obj pointer (vreg reference)
    cmp r2,#0
    moveq r1, #1                // doread = true
6:  THROWEQ art_ThrowNullPointerExceptionForFieldAccess
    ldr r0,[r1,#field_offset]       // offset into object
    ldr r0,[r2,r0]
3:  str r0,[r5,#0]          // store result (vreg reference)
4:  str r0,[r7,#0]
    // if volatile, memory barrier
    ldr r1,[r1,#field_access_flags]
    tst r1,#0x40            // kAccVolatile
    beq 10f
    dmb
10:
ENDINST IGET_OBJECT
RELOC reloc_vregC_22c, 1
RELOC reloc_vregB_22c, 2
RELOC reloc_vregA_22c, 3
RELOC reloc_vregA_22c, 4
RELOC reloc_helperaddr, 5
RELOC reloc_helperaddr, 6
ENDRELOC



SGET ,ldr, kPrimInt, StaticPrimitiveRead
SGET _BOOLEAN, ldrb, kPrimBoolean, StaticPrimitiveRead
SGET _BYTE, ldrsb, kPrimByte, StaticPrimitiveRead
SGET _SHORT, ldrsh, kPrimShort, StaticPrimitiveRead
SGET _CHAR, ldrh, kPrimChar, StaticPrimitiveRead

INST SGET_OBJECT
1:  movw r0, #0              // field index
    mov r1, #kPrimNot
4:  CALL art_ResolveField_StaticObjectRead
    mov r1, r0
    ldr r2,[r1,#field_declaring_class]          // class pointer
    ldr r0,[r1,#field_offset]       // offset into object
    ldr r0,[r2,r0]
2:  str r0,[r5,#0]          // store result (vreg reference)
3:  str r0,[r7,#0]
    // if volatile, memory barrier
    ldr r1,[r1,#field_access_flags]
    tst r1,#0x40            // kAccVolatile
    beq 10f
    dmb
10:
ENDINST SGET_OBJECT
RELOC reloc_vregB_21c, 1
RELOC reloc_vregA_21c, 2
RELOC reloc_vregA_21c, 3
RELOC reloc_helperaddr, 4
ENDRELOC


INST IGET_WIDE, attr_shared
1:  movw r0, #0              // field index
    mov r1, #kPrimLong
4:  CALL art_ResolveField_InstancePrimitiveRead
    mov r1, r0
2:  ldr r2,[r5,#0]          // obj pointer (reference)
    cmp r2,#0
    moveq r1, #1            // doread = true
5:  THROWEQ art_ThrowNullPointerExceptionForFieldAccess // r0 = field
    ldr r3,[r1,#field_offset]       // offset into object
    add r2,r2,r3
    ldmia r2,{r0,r3}
3:  movw r1, #0             // result vreg
    add r1, r7, r1, lsl #2    // convert to address
    stmia r1,{r0,r3}
    RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]

    // if volatile, memory barrier
    ldr r1,[r1,#field_access_flags]
    tst r1,#0x40            // kAccVolatile
    beq 10f
    dmb
10:
ENDINST IGET_WIDE
RELOC reloc_vregC_22c, 1
RELOC reloc_vregB_22c, 2
RELOC reloc_vregA_22c, 3
RELOC reloc_vregA_22c, 25
RELOC reloc_vregA_22c, 26
RELOC reloc_helperaddr, 4
RELOC reloc_helperaddr, 5
ENDRELOC

INST IGET_WIDE_QUICK
1: ldr r1,[r5,#0]     // object
   cmp r1,#0
5: THROWEQ art_ThrowNullPointerException
2: ldr r0,[r1,#0]     // field member (lo word)
3: ldr r2,[r1,#4]     // field member (hi word)
4: movw r1, #0        // vreg
   add r1, r7, r1, lsl #2
   stmia r1,{r0,r2}
   RESTORE_INT
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
ENDINST IGET_WIDE_QUICK
RELOC reloc_vregB_22c, 1
RELOC reloc_vregC_22c+reloc_raw, 2
RELOC reloc_vregC_22c+reloc_raw, 3
RELOC reloc_vregA_22c, 4
RELOC reloc_vregA_22c, 25
RELOC reloc_vregA_22c, 26
RELOC reloc_helperaddr, 5
ENDRELOC


INST IGET_OBJECT_QUICK
1: ldr r1,[r5,#0]     // object
   cmp r1,#0
5: THROWEQ art_ThrowNullPointerException
2: ldr r0,[r1,#0]     // field member
3: str r0,[r7,#0]     // store in vreg
4: str r0,[r5,#0]     // store in vreg reference
ENDINST IGET_OBJECT_QUICK
RELOC reloc_vregB_22c, 1
RELOC reloc_vregC_22c+reloc_raw, 2
RELOC reloc_vregA_22c, 3
RELOC reloc_vregA_22c, 4
RELOC reloc_helperaddr, 5
ENDRELOC


INST IGET_QUICK
1: ldr r1,[r5,#0]     // object
   cmp r1,#0
4: THROWEQ art_ThrowNullPointerException
2: ldr r0,[r1,#0]     // field member
3: str r0,[r7,#0]     // store in vreg
25: str     r8,[r5,#0]
ENDINST IGET_QUICK
RELOC reloc_vregB_22c, 1
RELOC reloc_vregC_22c+reloc_raw, 2
RELOC reloc_vregA_22c, 3
RELOC reloc_vregA_22c, 25
RELOC reloc_helperaddr, 4
ENDRELOC


INST SGET_WIDE
1:  movw r0, #0              // field index
    mov r1, #kPrimLong
4:  CALL art_ResolveField_StaticPrimitiveRead
    mov r1, r0
    ldr r2,[r1,#field_declaring_class]          // declaring class
    ldr r3,[r1,#field_offset]       // offset into object
    add r2,r2,r3

    // TODO: for volatile we need to use ldrexd
    ldmia r2,{r0,r3}
2:  str r0,[r7,#0]          // store result lo
3:  str r3,[r7,#4]          // store result hi
25: str     r8,[r5,#0]
26: str     r8,[r5,#4]
    // if volatile, memory barrier
    ldr r1,[r1,#field_access_flags]
    tst r1,#0x40            // kAccVolatile
    beq 10f
    dmb
10:
ENDINST SGET_WIDE
RELOC reloc_vregB_21c, 1
RELOC reloc_vregA_21c, 2
RELOC reloc_vregA_21c, 3
RELOC reloc_vregA_21c, 25
RELOC reloc_vregA_21c, 26
RELOC reloc_helperaddr, 4
ENDRELOC

// instance put
IPUT ,str, kPrimInt, InstancePrimitiveWrite
IPUT _BOOLEAN, strb, kPrimBoolean, InstancePrimitiveWrite
IPUT _BYTE, strb, kPrimByte, InstancePrimitiveWrite
IPUT _SHORT, strh, kPrimShort, InstancePrimitiveWrite
IPUT _CHAR, strh, kPrimChar, InstancePrimitiveWrite

INST IPUT_OBJECT, attr_shared
1:  movw r0, #0              // field index
    mov r1, #kPrimNot
4:  CALL art_ResolveField_InstanceObjectWrite
    mov r1, r0
2:  ldr r2,[r5,#0]          // obj pointer (vreg reference)
    cmp r2,#0
    moveq r1, #0                  // doread = false
5:  THROWEQ art_ThrowNullPointerExceptionForFieldAccess     // r0 = field
    ldr r0,[r1,#field_offset]       // offset into object
3:  ldr r3,[r5,#0]              // get value to store (vreg reference)
    // TODO: assignability check here
    ldr r1,[r1,#field_access_flags]
    tst r1,#0x40            // kAccVolatile
    beq 10f
    dmb st
    str r3,[r2,r0]           // store result
    // if volatile, memory barrier
    dmb
    b 11f
10:
    str r3,[r2,r0]           // store result
11:
    mov r1, r0    // field offset
    mov r0, r2    // destination object
    mov r2, r3    // new value
6:  CALL art_WriteBarrierField
ENDINST IPUT_OBJECT
RELOC reloc_vregC_22c, 1
RELOC reloc_vregB_22c, 2
RELOC reloc_vregA_22c, 3
RELOC reloc_helperaddr, 4
RELOC reloc_helperaddr, 5
RELOC reloc_helperaddr, 6
ENDRELOC

INST IPUT_QUICK
1: ldr r1,[r5,#0]     // object
   cmp r1,#0
4: THROWEQ art_ThrowNullPointerException
2: ldr r0,[r7,#0]     // vreg
3: str r0,[r1,#0]     // field member
ENDINST IPUT_QUICK
RELOC reloc_vregB_22c, 1
RELOC reloc_vregA_22c, 2
RELOC reloc_vregC_22c+reloc_raw, 3
RELOC reloc_helperaddr, 4
ENDRELOC

IPUT_XXX_QUICK BOOLEAN, str
IPUT_XXX_QUICK SHORT, strh
IPUT_XXX_QUICK CHAR, strh
IPUT_XXX_QUICK BYTE, strb

INST IPUT_WIDE, attr_shared
1:  movw r0, #0              // field index
    mov r1, #kPrimLong
5:  CALL art_ResolveField_InstancePrimitiveWrite
    mov r1, r0
2:  ldr r2,[r5,#0]          // obj pointer (vreg reference)
    cmp r2,#0
    moveq r1, #0              // doread = false
6:  THROWEQ art_ThrowNullPointerExceptionForFieldAccess
    ldr r0,[r1,#field_offset]       // offset into object
3:  ldr r3,[r7,#0]              // get value to store (lo word)
4:  ldr r4,[r7,#4]              // get value to store (hi word)
    add r2, r2, r0
    // if volatile, memory barrier
    ldr r1,[r1,#field_access_flags]
    tst r1,#0x40            // kAccVolatile
    beq 10f
    dmb st
    stmia r2,{r3,r4}
    dmb
    b 11f
10:
    stmia r2,{r3,r4}
11:
    RESTORE_INT
ENDINST IPUT_WIDE
RELOC reloc_vregC_22c, 1
RELOC reloc_vregB_22c, 2
RELOC reloc_vregA_22c, 3
RELOC reloc_vregA_22c, 4
RELOC reloc_helperaddr, 5
RELOC reloc_helperaddr, 6
ENDRELOC


INST IPUT_WIDE_QUICK
1: ldr r1,[r5,#0]     // object
   cmp r1,#0
6: THROWEQ art_ThrowNullPointerException
2: ldr r2,[r7,#0]     // vreg
3: str r2,[r1,#0]     // field member
4: ldr r3,[r7,#4]
5: str r3,[r1,#4]
ENDINST IPUT_WIDE_QUICK
RELOC reloc_vregB_22c, 1
RELOC reloc_vregA_22c, 2
RELOC reloc_vregC_22c+reloc_raw, 3
RELOC reloc_vregA_22c, 4
RELOC reloc_vregC_22c+reloc_raw, 5
RELOC reloc_helperaddr, 6
ENDRELOC

INST IPUT_OBJECT_QUICK
1: ldr r0,[r5,#0]     // object
   cmp r0,#0
4: THROWEQ art_ThrowNullPointerException
2: ldr r2,[r5,#0]     // vreg reference
3: str r2,[r0,#0]     // field member
5:  movw r1, #0   // field offset
6:  CALL art_WriteBarrierField
ENDINST IPUT_OBJECT_QUICK
RELOC reloc_vregB_22c, 1
RELOC reloc_vregA_22c, 2
RELOC reloc_vregC_22c+reloc_raw, 3
RELOC reloc_helperaddr, 4
RELOC reloc_vregC_22c, 5
RELOC reloc_helperaddr, 6
ENDRELOC



// static put
SPUT ,str, kPrimInt, StaticPrimitiveWrite
SPUT _BOOLEAN, strb, kPrimBoolean, StaticPrimitiveWrite
SPUT _BYTE, strb, kPrimByte, StaticPrimitiveWrite
SPUT _SHORT, strh, kPrimShort, StaticPrimitiveWrite
SPUT _CHAR, strh, kPrimChar, StaticPrimitiveWrite

INST SPUT_WIDE
1:  movw r0, #0              // field index
    mov r1, #kPrimLong
4:  CALL art_ResolveField_StaticPrimitiveWrite
    mov r1, r0
    ldr r2,[r1,#field_declaring_class]          // class pointer
    ldr r0,[r1,#field_offset]       // offset into object
2:  ldr r3,[r7,#0]              // get value to store (lo word)
3:  ldr r4,[r7,#4]              // get value to store (hi word)
    add r2, r2, r0
    // if volatile, memory barrier
    ldr r1,[r1,#field_access_flags]
    tst r1,#0x40            // kAccVolatile
    beq 10f
    dmb st
    stmia r2,{r3,r4}
    dmb
    b 11f
10:
    stmia r2,{r3,r4}
11:

ENDINST SPUT_WIDE
RELOC reloc_vregB_21c, 1
RELOC reloc_vregA_21c, 2
RELOC reloc_vregA_21c, 3
RELOC reloc_helperaddr, 4
ENDRELOC


INST SPUT_OBJECT
1:  movw r0, #0              // field index
    mov r1, #kPrimNot
4:  CALL art_ResolveField_StaticObjectWrite
    mov r1, r0
    ldr r2,[r1,#field_declaring_class]          // class pointer
    ldr r0,[r1,#field_offset]       // offset into object
2:  ldr r3,[r5,#0]              // get value to store (vreg reference))
    ldr lr,[r1,#field_access_flags]
    tst lr,#0x40            // kAccVolatile
    bne 3f
    // TODO: assignabilty check
    str r3,[r2,r0]           // store result
    b 5f
3:
    // volatile field
    dmb st
    str r3,[r2,r0]           // store result
    dmb sy
5:
    mov r1, r0    // field offset
    mov r0, r2    // destination object
    mov r2, r3    // new value
6:  CALL art_WriteBarrierField
ENDINST SPUT_OBJECT
RELOC reloc_vregB_21c, 1
RELOC reloc_vregA_21c, 2
RELOC reloc_helperaddr, 4
RELOC reloc_helperaddr, 6
ENDRELOC


//
// literals
//

INT_LIT16 ADD, add, 0
INT_LIT16 MUL, mul, 0
INT_LIT16 DIV, sdiv, 1
INT_LIT16 AND, and, 0
INT_LIT16 OR, orr, 0
INT_LIT16 XOR, eor, 0


// RSUB is slightly different (no _INT suffix)
INST RSUB_INT
1:  ldr r0,[r7,#0]
2:  movw r1,#0
    rsb r0,r0,r1
3:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST RSUB_INT
RELOC reloc_vregB_22s, 1
RELOC reloc_constC_22s, 2
RELOC reloc_vregA_22s, 3
RELOC reloc_vregA_22s, 25
ENDRELOC

INST RSUB_INT_NEGATIVE
1:  ldr r0,[r7,#0]
2:  movw r1,#0
    rsb r1, r1, #0
    rsb r0,r0,r1
3:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST RSUB_INT_NEGATIVE
RELOC reloc_vregB_22s, 1
RELOC reloc_constC_22s+reloc_negative, 2
RELOC reloc_vregA_22s, 3
RELOC reloc_vregA_22s, 25
ENDRELOC


// for LIT16, we need special handling for DIV and REM

INST DIV_INT_LIT16_SLOW
1:  add r0, r7, #0          // vreg address
2:  ldr r1,[r7,#0]          // vreg
3:  movw r2,#0
5:  CALL art_DoIntDivide
4:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST DIV_INT_LIT16_SLOW
RELOC reloc_vregA_22s, 1
RELOC reloc_vregB_22s, 2
RELOC reloc_constC_22s, 3
RELOC reloc_vregA_22s, 4
RELOC reloc_vregA_22s, 25
RELOC reloc_helperaddr, 5
ENDRELOC

INST REM_INT_LIT16_SLOW
1:  add r0, r7, #0          // vreg address
2:  ldr r1,[r7,#0]          // vreg
3:  movw r2,#0
5:  CALL art_DoIntRemainder
4:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST REM_INT_LIT16_SLOW
RELOC reloc_vregA_22s, 1
RELOC reloc_vregB_22s, 2
RELOC reloc_constC_22s, 3
RELOC reloc_vregA_22s, 4
RELOC reloc_vregA_22s, 25
RELOC reloc_helperaddr, 5
ENDRELOC

INST DIV_INT_LIT16_SLOW_NEGATIVE
1:  add r0, r7, #0          // vreg address
2:  ldr r1,[r7,#0]          // vreg
3:  movw r2,#0
    rsb r2, r2, #0
5:  CALL art_DoIntDivide
4:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST DIV_INT_LIT16_SLOW_NEGATIVE
RELOC reloc_vregA_22s, 1
RELOC reloc_vregB_22s, 2
RELOC reloc_constC_22s+reloc_negative, 3
RELOC reloc_vregA_22s, 4
RELOC reloc_vregA_22s, 25
RELOC reloc_helperaddr, 5
ENDRELOC

INST REM_INT_LIT16_SLOW_NEGATIVE
1:  add r0, r7, #0          // vreg address
2:  ldr r1,[r7,#0]          // vreg
3:  movw r2,#0
    rsb r2, r2, #0
5:  CALL art_DoIntRemainder
4:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST REM_INT_LIT16_SLOW_NEGATIVE
RELOC reloc_vregA_22s, 1
RELOC reloc_vregB_22s, 2
RELOC reloc_constC_22s+reloc_negative, 3
RELOC reloc_vregA_22s, 4
RELOC reloc_vregA_22s, 25
RELOC reloc_helperaddr, 5
ENDRELOC


INST REM_INT_LIT16
1:  ldr r0,[r7,#0]
2:  movw r1,#0
    cmp r1,#0
4:  THROWEQ art_ThrowDivideByZeroException
    sdiv r2, r0, r1
    mul r2, r1
    sub r0, r0, r2
3:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST REM_INT_LIT16
RELOC reloc_vregB_22s, 1
RELOC reloc_constC_22s, 2
RELOC reloc_vregA_22s, 3
RELOC reloc_vregA_22s, 25
RELOC reloc_helperaddr, 4
ENDRELOC

INST REM_INT_LIT16_NEGATIVE
1:  ldr r0,[r7,#0]
2:  movw r1,#0
    cmp r1,#0
4:  THROWEQ art_ThrowDivideByZeroException
    rsb r1, r1, #0
    sdiv r2, r0, r1
    mul r2, r1
    sub r0, r0, r2
3:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST REM_INT_LIT16_NEGATIVE
RELOC reloc_vregB_22s, 1
RELOC reloc_constC_22s+reloc_negative, 2
RELOC reloc_vregA_22s, 3
RELOC reloc_vregA_22s, 25
RELOC reloc_helperaddr, 4
ENDRELOC

INT_LIT8 ADD, add
INT_LIT8 RSUB, rsb
INT_LIT8 AND, and
INT_LIT8 OR, orr
INT_LIT8 XOR, eor


INST USHR_INT_LIT8
1:  ldr r0,[r7,#0]
2:  lsr r1,r0,#1      // NB: Careful here, assembler will not work if this is #0
3:  str r1,[r7,#0]
25: str     r8,[r5,#0]
ENDINST USHR_INT_LIT8
RELOC reloc_vregB_22b, 1
RELOC reloc_constC_22b, 2
RELOC reloc_vregA_22b, 3
RELOC reloc_vregA_22b, 25
ENDRELOC

INST SHR_INT_LIT8
1:  ldr r0,[r7,#0]
2:  asr r1,r0,#1      // NB: Careful here, assembler will not work if this is #0
3:  str r1,[r7,#0]
25: str     r8,[r5,#0]
ENDINST SHR_INT_LIT8
RELOC reloc_vregB_22b, 1
RELOC reloc_constC_22b, 2
RELOC reloc_vregA_22b, 3
RELOC reloc_vregA_22b, 25
ENDRELOC

INST SHL_INT_LIT8
1:  ldr r0,[r7,#0]
2:  lsl r1,r0,#1      // NB: Careful here, assembler will not work if this is #0
3:  str r1,[r7,#0]
25: str     r8,[r5,#0]
ENDINST SHL_INT_LIT8
RELOC reloc_vregB_22b, 1
RELOC reloc_constC_22b, 2
RELOC reloc_vregA_22b, 3
RELOC reloc_vregA_22b, 25
ENDRELOC

// for LIT8, we need special handling for MUL, DIV and REM

INST MUL_INT_LIT8
1:  ldr r0,[r7,#0]
2:  movw r1,#0
    mul r0,r0,r1
3:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST MUL_INT_LIT8
RELOC reloc_vregB_22b, 1
RELOC reloc_constC_22b, 2
RELOC reloc_vregA_22b, 3
RELOC reloc_vregA_22b, 25
ENDRELOC

INST MUL_INT_LIT8_NEGATIVE
1:  ldr r0,[r7,#0]
2:  movw r1,#0
    rsb r1, r1, #0
    mul r0,r0,r1
3:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST MUL_INT_LIT8_NEGATIVE
RELOC reloc_vregB_22b, 1
RELOC reloc_constC_22b+reloc_negative, 2
RELOC reloc_vregA_22b, 3
RELOC reloc_vregA_22b, 25
ENDRELOC

INST DIV_INT_LIT8
1:  ldr r0,[r7,#0]
2:  movw r1,#0
    cmp r1,#0
4:  THROWEQ art_ThrowDivideByZeroException
    sdiv r0,r0,r1
3:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST DIV_INT_LIT8
RELOC reloc_vregB_22b, 1
RELOC reloc_constC_22b, 2
RELOC reloc_vregA_22b, 3
RELOC reloc_vregA_22b, 25
RELOC reloc_helperaddr, 4
ENDRELOC

INST DIV_INT_LIT8_NEGATIVE
1:  ldr r0,[r7,#0]
2:  movw r1,#0
    cmp r1,#0
4:  THROWEQ art_ThrowDivideByZeroException
    rsb r1, r1, #0
    sdiv r0,r0,r1
3:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST DIV_INT_LIT8_NEGATIVE
RELOC reloc_vregB_22b, 1
RELOC reloc_constC_22b+reloc_negative, 2
RELOC reloc_vregA_22b, 3
RELOC reloc_vregA_22b, 25
RELOC reloc_helperaddr, 4
ENDRELOC


INST REM_INT_LIT8
1:  ldr r0,[r7,#0]
2:  movw r1,#0
    cmp r1,#0
4:  THROWEQ art_ThrowDivideByZeroException
    sdiv r2, r0, r1
    mul r2, r1
    sub r0, r0, r2
3:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST REM_INT_LIT8
RELOC reloc_vregB_22b, 1
RELOC reloc_constC_22b, 2
RELOC reloc_vregA_22b, 3
RELOC reloc_vregA_22b, 25
RELOC reloc_helperaddr, 4
ENDRELOC


INST REM_INT_LIT8_NEGATIVE
1:  ldr r0,[r7,#0]
2:  movw r1,#0
    cmp r1,#0
4:  THROWEQ art_ThrowDivideByZeroException
    rsb r1, r1, #0
    sdiv r2, r0, r1
    mul r2, r1
    sub r0, r0, r2
3:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST REM_INT_LIT8_NEGATIVE
RELOC reloc_vregB_22b, 1
RELOC reloc_constC_22b+reloc_negative, 2
RELOC reloc_vregA_22b, 3
RELOC reloc_vregA_22b, 25
RELOC reloc_helperaddr, 4
ENDRELOC


INST DIV_INT_LIT8_SLOW
    FLUSH_INT
1:  add r0, r7, #0          // vreg address
2:  ldr r1,[r7,#0]          // vreg
3:  movw r2,#0
5:  CALL art_DoIntDivide
4:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST DIV_INT_LIT8_SLOW
RELOC reloc_vregA_22b, 1
RELOC reloc_vregB_22b, 2
RELOC reloc_constC_22b, 3
RELOC reloc_vregA_22b, 4
RELOC reloc_vregA_22b, 25
RELOC reloc_helperaddr, 5
ENDRELOC


INST DIV_INT_LIT8_SLOW_NEGATIVE
    FLUSH_INT
1:  add r0, r7, #0          // vreg address
2:  ldr r1,[r7,#0]          // vreg
3:  movw r2,#0
    rsb r2, r2, #0
5:  CALL art_DoIntDivide
4:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST DIV_INT_LIT8_SLOW_NEGATIVE
RELOC reloc_vregA_22b, 1
RELOC reloc_vregB_22b, 2
RELOC reloc_constC_22b+reloc_negative, 3
RELOC reloc_vregA_22b, 4
RELOC reloc_vregA_22b, 25
RELOC reloc_helperaddr, 5
ENDRELOC


INST REM_INT_LIT8_SLOW
    FLUSH_INT
1:  add r0, r7, #0          // vreg address
2:  ldr r1,[r7,#0]          // vreg
3:  movw r2,#0
5:  CALL art_DoIntRemainder
4:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST REM_INT_LIT8_SLOW
RELOC reloc_vregA_22b, 1
RELOC reloc_vregB_22b, 2
RELOC reloc_constC_22b, 3
RELOC reloc_vregA_22b, 4
RELOC reloc_vregA_22b, 25
RELOC reloc_helperaddr, 5
ENDRELOC



INST REM_INT_LIT8_SLOW_NEGATIVE
    FLUSH_INT
1:  add r0, r7, #0          // vreg address
2:  ldr r1,[r7,#0]          // vreg
3:  movw r2,#0
    rsb r2, r2, #0
5:  CALL art_DoIntRemainder
4:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST REM_INT_LIT8_SLOW_NEGATIVE
RELOC reloc_vregA_22b, 1
RELOC reloc_vregB_22b, 2
RELOC reloc_constC_22b+reloc_negative, 3
RELOC reloc_vregA_22b, 4
RELOC reloc_vregA_22b, 25
RELOC reloc_helperaddr, 5
ENDRELOC

// method invocations

INVOKE VIRTUAL,Virtual
INVOKE SUPER, Super
INVOKE DIRECT, Direct
INVOKE INTERFACE, Interface

INVOKE_RANGE VIRTUAL, Virtual
INVOKE_RANGE SUPER, Super
INVOKE_RANGE DIRECT, Direct
INVOKE_RANGE INTERFACE, Interface

INST INVOKE_VIRTUAL_QUICK
1:  movw r0,#0          // vtable index
2:  ldr r1,[r5,#0]      // receiver
    cmp r1, #0
4:  THROWEQ art_ThrowNullPointerException
6:  CALL art_ResolveMethodQuick     // exit with r0 = method, preserves r1
8:  ldr r1,[r5,#0]      // receiver in case gc moved it
5:  CALL art_DoCall
3:  .word 0
7:  .word 0
ENDINST INVOKE_VIRTUAL_QUICK
RELOC reloc_vregB_35c, 1
RELOC reloc_vregC_35c, 2
RELOC reloc_instruction, 3
RELOC reloc_helperaddr, 4
RELOC reloc_helperaddr, 5
RELOC reloc_helperaddr, 6
RELOC reloc_dexpc, 7
RELOC reloc_vregC_35c, 8
ENDRELOC

INST INVOKE_VIRTUAL_RANGE_QUICK
1:  movw r0,#0          // vtable index
2:  ldr r1,[r5,#0]      // receiver
    cmp r0, #0
4:  THROWEQ art_ThrowNullPointerException
5:  CALL art_ResolveMethodQuick         // exit with r0 = method, preserves r1
8:  ldr r1,[r5,#0]      // receiver in case gc moved it
6:  CALL art_DoCallRange
3:  .word 0
7:  .word 0
ENDINST INVOKE_VIRTUAL_RANGE_QUICK
RELOC reloc_vregB_3rc, 1
RELOC reloc_vregC_3rc, 2
RELOC reloc_instruction, 3
RELOC reloc_helperaddr, 4
RELOC reloc_helperaddr, 5
RELOC reloc_helperaddr, 6
RELOC reloc_dexpc, 7
RELOC reloc_vregC_3rc, 8
ENDRELOC


INST INVOKE_STATIC
1:  movw r0, #0          // method index
5:  CALL art_ResolveStaticMethod
6:  CALL art_DoCall
2:  .word 0
7:  .word 0
ENDINST INVOKE_STATIC
RELOC reloc_vregB_35c, 1
RELOC reloc_instruction, 2
RELOC reloc_helperaddr, 6
RELOC reloc_helperaddr, 5
RELOC reloc_dexpc, 7
ENDRELOC

INST INVOKE_STATIC_RANGE
1:  movw r0, #0          // method index
5:  CALL art_ResolveStaticMethod
6:  CALL art_DoCallRange
2:  .word 0
7:  .word 0
ENDINST INVOKE_STATIC_RANGE
RELOC reloc_vregB_3rc, 1
RELOC reloc_instruction, 2
RELOC reloc_helperaddr, 5
RELOC reloc_helperaddr, 6
RELOC reloc_dexpc, 7
ENDRELOC

//
// unused opcodes
//


.macro UNUSED name
.global art_xlate_code_UNUSED_\name
art_xlate_code_UNUSED_\name:
.global art_xlate_reloc_UNUSED_\name
art_xlate_reloc_UNUSED_\name:
.endm


.irp p,3E,3F,40,41,42,43,79,7A
UNUSED \p
.endr


.irp p,EB,EC,ED,EE,EF
UNUSED \p
.endr

.irp p,F0,F1,F2,F3,F4,F5,F6,F7
UNUSED \p
.endr

.irp p,F8,F9,FA,FB,FC,FD,FE,FF
UNUSED \p
.endr

    // end of all unused instructions
    NYI
    .word -1

