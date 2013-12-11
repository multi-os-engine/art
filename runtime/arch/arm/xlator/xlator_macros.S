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

// macro definitions for xlate.S

// Set to 1 to enable debug prints
.set debug_print, 0

// Instruction attributes
.set attr_inline, 1         // Instruction is inlined.
.set attr_shared, 2         // Instruction is shared.

.set current_attrs, attr_inline

// end of instruction
.macro ENDINST name
.if 0 && (current_attrs & attr_shared) != 0
  pop {r0, pc}
.endif
.global art_xlate_reloc_\name
art_xlate_reloc_\name:
.endm


// cause an exception on not yet implemented
.macro NYI
  bkpt
.endm

.macro REGDUMP reg
  ldr \reg, [r6, #art_RegDump*4]
  blx \reg
.endm

.macro PRINT str
  ldr lr,[r6,#art_Print*4]
  blx lr
  .asciz "\str"
  .align 2
.endm

.macro HEXDUMP reg, addr, len
  push {r0,r1,\reg}
  ldr \reg,[r6,#art_HexDump*4]
  mov r0, \addr
  mov r1, #\len
  blx \reg
  pop {r0, r1,\reg}
.endm

.macro HEXDUMP2 reg, addr, len
  push {r0,r1,\reg}
  ldr \reg,[r6,#art_HexDump*4]
  mov r0, \addr
  mov r1, \len
  blx \reg
  pop {r0, r1,\reg}
.endm

.macro FLUSH_INT
  str r11,[r7,#0]
.endm

.macro FLUSH_OBJ
  str r12,[r5,#0]
.endm

.macro FLUSH_ALL
  FLUSH_INT
  FLUSH_OBJ
.endm

.macro RESTORE_INT
  ldr r11,[r7,#0]
.endm

.macro RESTORE_OBJ
  ldr r12,[r5,#0]
.endm

.macro RESTORE_ALL
  RESTORE_INT
  RESTORE_OBJ
.endm

// start of an instruction
.macro INST name, attrs=attr_inline
.align 2
  .word attr_inline       //  TODO: \attrs        // Attributes precede code.
.global art_xlate_code_\name
art_xlate_code_\name:
99:
.if 0 && (\attrs & attr_shared) != 0
  push {r0, lr}
.endif

.if debug_print
  // sub r0, pc, #12
  // HEXDUMP lr, r0, 16
  ldr r0,[r6,#art_Print*4]
  blx r0
  .asciz "\name"
  .align 2
  // BREAK r0
.endif
.set current_attrs, \attrs
.endm

// start of relocations
.macro RELOC type, offset
    .hword \type
    .hword (\offset\()b-99b)/4
.endm

// end of reloctions
.macro ENDRELOC
    .word -1
.endm

// call the breakpoint helper function.
.macro BREAK reg
  ldr lr,[r6,#art_Breakpoint*4]
  blx lr
.endm

.macro CALL func
  .word 0xeb000000 | \func
.endm

// these are slow path.  Maybe find a way to encode the condition code

// Call an exception throwing function on EQ (equal)
.macro THROWEQ func
  .word 0x0b000000 | \func
  //  ldr\cc lr,[r6,#\func*4]
  // blx\cc lr
.endm

// Call an exception throwing function on CS (carry set)
.macro THROWCS func
  .word 0x2b000000 | \func
  //  ldr\cc lr,[r6,#\func*4]
  // blx\cc lr
.endm

// Load the address of the result into the given reg
.macro RESULT reg
  ldr \reg, [sp, #4]
.endm

.macro METHOD_RETURN
    ldr     r0, [r6, #art_PopShadowFrame*4]
    blx     r0
    add sp, sp, #8
    pop {r0-r12, pc}
.endm


// IF_ instructions
.macro IF_XX XX,xx, yy
INST IF_\XX
2:  ldr r1,[r7,#0]              // vreg2
1:  ldr r0,[r7,#0]              // vreg1
    cmp r0,r1
3:  b\xx 2b
ENDINST IF_\XX
RELOC reloc_vregA_22t, 1
RELOC reloc_vregB_22t, 2
RELOC reloc_offset, 3
ENDRELOC

INST IF_\XX\()_BACK
2:  ldr r1,[r7,#0]              // vreg2
1:  ldr r0,[r7,#0]              // vreg1
4:  CALL art_CheckSuspend
    cmp r0,r1
3:  b\xx 2b
ENDINST IF_\XX\()_BACK
RELOC reloc_vregA_22t, 1
RELOC reloc_vregB_22t, 2
RELOC reloc_offset, 3
RELOC reloc_helperaddr, 4
ENDRELOC
.endm


.macro IF_XXZ XX,xx, yy
INST IF_\XX
1:  ldr r0,[r7,#0]              // vreg1
    cmp r0,#0
3:  b\xx 1b
ENDINST IF_\XX
RELOC reloc_vregA_21t, 1
RELOC reloc_offset, 3
ENDRELOC

INST IF_\XX\()_BACK
1:  ldr r0,[r7,#0]              // vreg1
4:  CALL art_CheckSuspend
    cmp r0,#0
3:  b\xx 1b
ENDINST IF_\XX\()_BACK
RELOC reloc_vregA_21t, 1
RELOC reloc_offset, 3
RELOC reloc_helperaddr, 4
ENDRELOC
.endm

//
// arithmetic operations
//

.macro BINOP_INT code, op
INST \code
1:  ldr r0,[r7,#0]              // vreg
2:  ldr r1,[r7,#0]              // vreg
25: str     r8,[r5,#0]
    \op r0, r0, r1
3:  str r0, [r7, #0]            // vreg
ENDINST \code
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 3
RELOC reloc_vregA_23x, 25
ENDRELOC
.endm

.macro BINOP_INT_2ADDR code, op
INST \code\()_2ADDR
1:  ldr r0,[r7,#0]              // vreg
2:  ldr r1,[r7,#0]              // vreg
25: str     r8,[r5,#0]
    \op r0, r0, r1
3:  str r0, [r7, #0]            // vreg
ENDINST \code\()_2ADDR
RELOC reloc_vregA_12x, 1
RELOC reloc_vregB_12x, 2
RELOC reloc_vregA_12x, 3
RELOC reloc_vregA_12x, 25
ENDRELOC
.endm

// Binary integer operations with an extra instruction to
// modify the second operand of the main instruction.  Used
// by shift operations
.macro BINOP_INT2 code, op, preinstr
INST \code
1:  ldr r0,[r7,#0]              // vreg
2:  ldr r1,[r7,#0]              // vreg
25: str     r8,[r5,#0]
    \preinstr
    \op r0, r0, r1
3:  str r0, [r7, #0]            // vreg
ENDINST \code
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 3
RELOC reloc_vregA_23x, 25
ENDRELOC
.endm

.macro BINOP_INT_2ADDR2 code, op, preinstr
INST \code\()_2ADDR
1:  ldr r0,[r7,#0]              // vreg
2:  ldr r1,[r7,#0]              // vreg
25: str     r8,[r5,#0]
    \preinstr
    \op r0, r0, r1
3:  str r0, [r7, #0]            // vreg
ENDINST \code\()_2ADDR
RELOC reloc_vregA_12x, 1
RELOC reloc_vregB_12x, 2
RELOC reloc_vregA_12x, 3
RELOC reloc_vregA_12x, 25
ENDRELOC
.endm


.macro BINOP_LONG code, op1, op2
INST \code
1:  ldr r0,[r7,#0]              // vreg
2:  ldr r1,[r7,#4]              // vreg
3:  ldr r2,[r7,#0]              // vreg
4:  ldr r3,[r7,#4]              // vreg
25: str     r8,[r5,#0]
    \op1 r0, r0, r2
    \op2 r1, r1, r3
5:  str r0, [r7, #0]            // vreg
6:  str r1, [r7, #4]            // vreg
26: str     r8,[r5,#4]
ENDINST \code
RELOC reloc_vregB_23x, 1
RELOC reloc_vregB_23x, 2
RELOC reloc_vregC_23x, 3
RELOC reloc_vregC_23x, 4
RELOC reloc_vregA_23x, 5
RELOC reloc_vregA_23x, 6
RELOC reloc_vregA_23x, 25
RELOC reloc_vregA_23x, 26
ENDRELOC
.endm

.macro BINOP_LONG_2ADDR code, op1, op2
INST \code\()_2ADDR
1:  ldr r0,[r7,#0]              // vreg
2:  ldr r1,[r7,#4]              // vreg
3:  ldr r2,[r7,#0]              // vreg
4:  ldr r3,[r7,#4]              // vreg
25: str     r8,[r5,#0]
    \op1 r0, r0, r2
    \op2 r1, r1, r3
5:  str r0, [r7, #0]            // vreg
6:  str r1, [r7, #4]            // vreg
26: str     r8,[r5,#4]
ENDINST \code\()_2ADDR
RELOC reloc_vregA_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregB_12x, 3
RELOC reloc_vregB_12x, 4
RELOC reloc_vregA_12x, 5
RELOC reloc_vregA_12x, 6
RELOC reloc_vregA_12x, 25
RELOC reloc_vregA_12x, 26
ENDRELOC
.endm

.macro UNOP_INT code, instr
INST \code
1:  ldr r0,[r7,#0]              // vreg
25: str     r8,[r5,#0]
    \instr
2:  str r0, [r7, #0]            // vreg
ENDINST \code
RELOC reloc_vregB_12x, 1
RELOC reloc_vregA_12x, 2
RELOC reloc_vregA_12x, 25
ENDRELOC
.endm

.macro UNOP_LONG code, instr1, instr2
INST \code
1:  ldr r0,[r7,#0]              // vreg
2:  ldr r1,[r7,#4]              // vreg
25: str     r8,[r5,#0]
    \instr1
    \instr2
3:  str r0, [r7, #0]            // vreg
4:  str r1, [r7, #4]            // vreg
26: str     r8,[r5,#4]
ENDINST \code
RELOC reloc_vregB_12x, 1
RELOC reloc_vregB_12x, 2
RELOC reloc_vregA_12x, 3
RELOC reloc_vregA_12x, 4
RELOC reloc_vregA_12x, 25
RELOC reloc_vregA_12x, 26
ENDRELOC
.endm

.macro BINOP_FLOAT code, op
INST \code
    FLUSH_INT
1:  movw r2,#0
2:  movw r3,#0
    add r2, r7, r2, lsl #2
    add r3, r7, r3, lsl #2
    flds s0,[r2,#0]
    flds s1,[r3,#0]
25: str     r8,[r5,#0]
    \op s0, s0, s1
3:  movw r2, #0
    add r2, r7, r2, lsl #2
    fsts s0, [r2,#0]
ENDINST \code
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 3
RELOC reloc_vregA_23x, 25
ENDRELOC
.endm

.macro BINOP_DOUBLE code, op
INST \code
    FLUSH_INT
1:  movw r2,#0
2:  movw r3,#0
    add r2, r7, r2, lsl #2
    add r3, r7, r3, lsl #2
    fldd d0,[r2,#0]
    fldd d1,[r3,#0]
25: str     r8,[r5,#0]
    \op d0, d0, d1
3:  movw r2, #0
    add r2, r7, r2, lsl #2
    fstd d0, [r2,#0]
    RESTORE_INT
26: str     r8,[r5,#4]
ENDINST \code
RELOC reloc_vregB_23x, 1
RELOC reloc_vregC_23x, 2
RELOC reloc_vregA_23x, 3
RELOC reloc_vregA_23x, 25
RELOC reloc_vregA_23x, 26
ENDRELOC
.endm

.macro BINOP_FLOAT_2ADDR code, op
INST \code\()_2ADDR
    FLUSH_INT
1:  movw r2,#0
2:  movw r3,#0
    add r2, r7, r2, lsl #2
    add r3, r7, r3, lsl #2
    flds s0,[r2,#0]
    flds s1,[r3,#0]
25: str     r8,[r5,#0]
    \op s0, s0, s1
    fsts s0, [r2,#0]
ENDINST \code\()_2ADDR
RELOC reloc_vregA_12x, 1
RELOC reloc_vregB_12x, 2
RELOC reloc_vregA_12x, 25
ENDRELOC
.endm

.macro BINOP_DOUBLE_2ADDR code, op
INST \code\()_2ADDR
    FLUSH_INT
1:  movw r2,#0
2:  movw r3,#0
    add r2, r7, r2, lsl #2
    add r3, r7, r3, lsl #2
    fldd d0,[r2,#0]
    fldd d1,[r3,#0]
25: str     r8,[r5,#0]
    \op d0, d0, d1
    fstd d0, [r2,#0]
    RESTORE_INT
26: str     r8,[r5,#4]
ENDINST \code\()_2ADDR
RELOC reloc_vregA_12x, 1
RELOC reloc_vregB_12x, 2
RELOC reloc_vregA_12x, 25
RELOC reloc_vregA_12x, 26
ENDRELOC
.endm


.macro IGET code, load, fieldtype, findtype
INST IGET\code
1:  movw r0, #0             // field index
    mov r1, #\fieldtype
4:  CALL art_ResolveField_\findtype
    mov r1, r0
2:  ldr r2,[r5,#0]          // obj pointer (vreg reference)
    cmp r2,#0
    moveq r1, #1            // doread = true
5:  THROWEQ art_ThrowNullPointerExceptionForFieldAccess      // r0 = field
    ldr r0,[r1,#field_offset]       // offset into object
25: str     r8,[r5,#0]
    \load r0,[r2,r0]
    ldr r1,[r1,#field_access_flags]
3:  str r0,[r7,#0]          // store result
    // if volatile, memory barrier
    tst r1,#0x40            // kAccVolatile
    beq 10f
    dmb
10:
ENDINST IGET\code
RELOC reloc_vregC_22c, 1
RELOC reloc_vregB_22c, 2
RELOC reloc_vregA_22c, 3
RELOC reloc_helperaddr, 4
RELOC reloc_helperaddr, 5
RELOC reloc_vregA_22c, 25
ENDRELOC
.endm

.macro IPUT code, store, fieldtype, findtype
INST IPUT\code
1:  movw r0, #0              // field index
    mov r1, #\fieldtype
4:  CALL art_ResolveField_\findtype
    mov r1, r0
2:  ldr r2,[r5,#0]          // obj pointer (vreg reference)
    cmp r2,#0
    moveq r1, #0            // doread = false
5:  THROWEQ art_ThrowNullPointerExceptionForFieldAccess      // r0 = field
    ldr r0,[r1,#field_offset]       // offset into object
3:  ldr r3,[r7,#0]              // get value to store
    ldr r1,[r1,#field_access_flags]
    tst r1,#0x40            // kAccVolatile
    beq 10f
    dmb st
    \store r3,[r2,r0]           // store result
    // if volatile, memory barrier
    dmb
    b 11f
10:
    \store r3,[r2,r0]           // store result
11:
ENDINST IPUT\code
RELOC reloc_vregC_22c, 1
RELOC reloc_vregB_22c, 2
RELOC reloc_vregA_22c, 3
RELOC reloc_helperaddr, 4
RELOC reloc_helperaddr, 5
ENDRELOC
.endm

.macro IPUT_XXX_QUICK code, store
INST IPUT_\code\()_QUICK
1: ldr r1,[r5,#0]     // object
2: ldr r2,[r7,#0]     // vreg
   cmp r1,#0
4: THROWEQ art_ThrowNullPointerException
3: \store r2,[r1,#0]     // field member
ENDINST IPUT_\code\()_QUICK
RELOC reloc_vregB_22c, 1
RELOC reloc_vregA_22c, 2
RELOC reloc_vregC_22c+reloc_raw, 3
RELOC reloc_helperaddr, 4
ENDRELOC
.endm

.macro SGET code, load, fieldtype, findtype
INST SGET\code
1:  movw r0, #0              // field index
    mov r1, #\fieldtype
4:  CALL art_ResolveField_\findtype
    mov r1, r0
    ldr r2,[r1,#field_declaring_class]          // class pointer
    ldr r0,[r1,#field_offset]       // offset into object
    \load r0,[r2,r0]
2:  str r0,[r7,#0]          // store result
25: str     r8,[r5,#0]
    // if volatile, memory barrier
    ldr r0,[r1,#field_access_flags]
    tst r0,#0x40            // kAccVolatile
    beq 10f
    dmb
10:
ENDINST SGET\code
RELOC reloc_vregB_21c, 1
RELOC reloc_vregA_21c, 2
RELOC reloc_helperaddr, 4
RELOC reloc_vregA_21c, 25
ENDRELOC
.endm

.macro SPUT code, store, fieldtype, findtype
INST SPUT\code
1:  movw r0, #0              // field index
    mov r1, #\fieldtype
4:  CALL art_ResolveField_\findtype
    mov r1, r0
    ldr r2,[r1,#field_declaring_class]          // class pointer
    ldr r0,[r1,#field_offset]       // offset into object
2:  ldr r3,[r7,#0]              // get value to store
    ldr r1,[r1,#field_access_flags]
    tst r1,#0x40            // kAccVolatile
    beq 10f
    dmb st
    \store r3,[r2,r0]           // store result
    // if volatile, memory barrier
    dmb
    b 11f
10:
    \store r3,[r2,r0]           // store result
11:
  REGDUMP lr
ENDINST SPUT\code
RELOC reloc_vregB_21c, 1
RELOC reloc_vregA_21c, 2
RELOC reloc_helperaddr, 4
ENDRELOC
.endm

//
// literals
//
// There are both positive and negative versions of these opcodes.
//

.macro INT_LIT16 code, op, isdiv
INST \code\()_INT_LIT16
1:  ldr r0,[r7,#0]
2:  movw r1,#0
.if \isdiv
    cmp r1,#0
4:  THROWEQ art_ThrowDivideByZeroException
.endif
    \op r0,r0,r1
3:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST \code\()_INT_LIT16
RELOC reloc_vregB_22s, 1
RELOC reloc_constC_22s, 2
RELOC reloc_vregA_22s, 3
RELOC reloc_vregA_22s, 25
.if \isdiv
RELOC reloc_helperaddr, 4
.endif
ENDRELOC

INST \code\()_INT_LIT16_NEGATIVE
1:  ldr r0,[r7,#0]
2:  movw r1,#0
.if \isdiv
    cmp r1,#0
4:  THROWEQ art_ThrowDivideByZeroException
.endif
    rsb r1, r1, #0
    \op r0,r0,r1
3:  str r0,[r7,#0]
25: str     r8,[r5,#0]
ENDINST \code\()_INT_LIT16_NEGATIVE
RELOC reloc_vregB_22s, 1
RELOC reloc_constC_22s+reloc_negative, 2
RELOC reloc_vregA_22s, 3
RELOC reloc_vregA_22s, 25
.if \isdiv
RELOC reloc_helperaddr, 4
.endif
ENDRELOC
.endm

.macro INT_LIT8 code, op
INST \code\()_INT_LIT8
1:  ldr r0,[r7,#0]
25: str     r8,[r5,#0]
2:  \op r1,r0,#0
3:  str r1,[r7,#0]
ENDINST \code\()_INT_LIT8
RELOC reloc_vregB_22b, 1
RELOC reloc_constC_22b, 2
RELOC reloc_vregA_22b, 3
RELOC reloc_vregA_22b, 25
ENDRELOC

INST \code\()_INT_LIT8_NEGATIVE
1:  ldr r0,[r7,#0]
2:  movw r1,#0
25: str     r8,[r5,#0]
    rsb r1, r1, #0
    \op r0,r0,r1
3:  str r0,[r7,#0]
ENDINST \code\()_INT_LIT8_NEGATIVE
RELOC reloc_vregB_22b, 1
RELOC reloc_constC_22b+reloc_negative, 2
RELOC reloc_vregA_22b, 3
RELOC reloc_vregA_22b, 25
ENDRELOC

.endm

// invoke a method, non static, non range
.macro INVOKE code,type
INST INVOKE_\code
1:  movw r0, #0          // method index
2:  ldr r1,[r5,#0]          // receiver
7:  CALL art_Resolve\type\()Method
3:  .word 0           // dex pc
    // receiver is in r1
    // method is in r0
4:  ldr r1,[r5,#0]              // reload receiver in case GC moved it
8:  CALL art_DoCall
5:  .word 0         // Address of dex instruction
9:  .word 0         // dex pc

ENDINST INVOKE_\code
RELOC reloc_vregB_35c, 1
RELOC reloc_vregC_35c, 2
RELOC reloc_dexpc, 3
RELOC reloc_vregC_35c, 4
RELOC reloc_instruction, 5
RELOC reloc_helperaddr, 7
RELOC reloc_helperaddr, 8
RELOC reloc_dexpc, 9
ENDRELOC
.endm

// invoke a method, non static, range
.macro INVOKE_RANGE code, type
INST INVOKE_\code\()_RANGE
1:  movw r0, #0          // method index
2:  ldr r1,[r5,#0]          // receiver
7:  CALL art_Resolve\type\()Method
3:  .word 0           // dex pc
    // receiver is in r1
    // method is in r0
4:  ldr r1,[r5,#0]              // reload receiver in case GC moved it
8:  CALL art_DoCallRange
5:  .word 0
9:  .word 0
ENDINST INVOKE_\code\()_RANGE
RELOC reloc_vregB_3rc, 1
RELOC reloc_vregC_3rc, 2
RELOC reloc_dexpc, 3
RELOC reloc_vregC_3rc, 4
RELOC reloc_instruction, 5
RELOC reloc_helperaddr, 7
RELOC reloc_helperaddr, 8
RELOC reloc_dexpc, 9
ENDRELOC
.endm

