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

#include "asm_support_x86.S"

DEFINE_FUNCTION art_arm64_trampoline_begin
    call    1f
1:
    popl    %eax
    ret
END_FUNCTION art_arm64_trampoline_begin

    /*
     * Foreign architecture simulation call wrapper for x86->A64.
     *
     * Allow calling A64 code via a simulator.
     */
DEFINE_FUNCTION art_quick_foreign_code_call
    pushl   %ebp          // Save %ebp.
    movl    4(%esp), %ebp // %ebp = pointer to frame size of A64 method.
    movl    0(%ebp), %ebp // %ebp = frame size.
    subl    %ebp, %esp    // Allocate space for the A64 frame in the stack.
    addl    $12, %esp     // Reduce stack to take into account the size of the
                          // stuff we have pushed already (%ebp, ret addrs).
    addl    %esp, %ebp    // %ebp = %esp at entry of A64 code.

    // Reconstruct a semi-sane stack layout (stack is aligned here).
    pushl   -4(%ebp)      // Move x86 return address at the top of the stack.
    pushl   -8(%ebp)      // Move the A64 address at the top of stack.
    pushl   -12(%ebp)     // Move the old value of %ebp at the top of the stack.
    pushl   %ebp          // Save pointer to A64 stack.
    movl    %esp, %ebp
    // 8(%ebp) is the pointer to the frame size of the A64 method, which is
    // followed by A64 code.

    // Create a table of x86 registers.
    pushl   %edi
    pushl   %esi
    pushl   %fs
    pushl   %ebx
    // Stack aligned here.
    pushl   %edx
    pushl   %ecx
    pushl   %eax

    // call artArm64CodeCall(void *a64Code, uintptr_t *regValues, uintptr_t *sp).
    movl    %esp, %eax     // %eax = pointer to register table.
    subl    $20, %esp
    movl    %eax, 4(%esp)  // %eax --> arg 1 (regValues).
    movl    0(%ebp), %eax  // %eax = stack pointer for A64 code.
    movl    %eax, 8(%esp)  // %eax --> arg 2 (sp)
    movl    8(%ebp), %eax  // %eax = pointer to frame size.
    addl    $4, %eax       //      = pointer to A64 code.
    movl    %eax, (%esp)   // %eax --> arg 0 (a64code).
    call    SYMBOL(artArm64CodeCall)
    addl    $20, %esp

    // Restore the x86 registers (possibly modified by the simulator).
    popl    %eax
    popl    %ecx
    popl    %edx
    popl    %ebx
    popl    %fs
    popl    %esi
    popl    %edi

    // Reconstruct the x86 stack.
    popl    %ebp           // %ebp = pointer to the A64 stack.
    subl    $8, %ebp       // Allocate space for two values.
    popl    0(%ebp)        // Restore the old value of %ebp.
    addl    $4, %esp       // Discard pointer to frame size.
    popl    4(%ebp)        // Restore the return address.

    // Stack reconstructed: ready to return.
    movl    %ebp, %esp
    popl    %ebp
    ret
END_FUNCTION art_quick_foreign_code_call

    /*
     * Foreign architecture simulation call wrapper for A64->x86.
     *
     * Arguments:
     *    8(%ebp) pc from which the A64 method made the call.
     *   12(%ebp) Stack pointer at entry of the A64 method.
     *   16(%ebp) Frame size for the A64 method.
     *   20(%ebp) x86 register table (used to set the x86 register before the call).
     *   24(%ebp) x86 function to call.
     */
DEFINE_FUNCTION art_quick_foreign_code_call_back
    pushl   %ebp
    movl    %esp, %ebp
    pushl   %eax
    pushl   %ecx
    pushl   %edx
    pushl   %ebx
    pushl   %fs
    pushl   %esi
    pushl   %edi

    subl    $8, %esp       // Alignment padding.
    pushl   %ebp
    // FIXME: the value above is accessed in
    //   art/runtime/entrypoints/quick/quick_trampoline_entrypoints.S.
    //   The way this is done is very hacky at the moment.

    // Copy the A64 frame at the top of the stack, so that the called function
    // has the perception of a sane native stack.
    movl    16(%ebp), %ecx // %ecx = A64 frame size.
    movl    12(%ebp), %esi // %esi = source pointer (original A64 &esp).
    subl    %ecx, %esp     // Allocate space for the frame in the stack.
    subl    %ecx, %esi     // %esi = positon from where to copy.
    shrl    $2, %ecx       // %ecx = size in words.
    xorl    %eax, %eax     // %eax = 0

    // Copy the original A64 frame to the top of the stack.
1:
    movl    (%esi,%eax,4), %edx
    movl    %edx, (%esp,%eax,4)
    addl    $1, %eax
    cmpl    %ecx, %eax
    jne     1b

    movl    20(%ebp), %eax // %eax = pointer to register table.

    // Load registers.
    movl    4(%eax), %ecx
    movl    8(%eax), %edx
    movl    12(%eax), %ebx
    mov     16(%eax), %fs
    movl    20(%eax), %esi
    movl    24(%eax), %edi
    movl    0(%eax), %eax

    call    *24(%ebp)      // Call function.

    // Save registers.
    pushl   %eax
    movl    20(%ebp), %eax // %eax = pointer to register table.
    popl    0(%eax)
    movl    %ecx, 4(%eax)
    movl    %edx, 8(%eax)
    movl    %ebx, 12(%eax)
    mov     %fs, 16(%eax)
    movl    %esi, 20(%eax)
    movl    %edi, 24(%eax)

    // Restore registers.
    movl    -4(%ebp), %eax
    movl    -8(%ebp), %ecx
    movl    -12(%ebp), %edx
    movl    -16(%ebp), %ebx
    mov     -20(%ebp), %fs
    movl    -24(%ebp), %esi
    movl    -28(%ebp), %edi

    leave
    ret
END_FUNCTION art_quick_foreign_code_call_back

DEFINE_FUNCTION art_arm64_trampoline_end
    call    1f
1:
    popl    %eax
    ret
END_FUNCTION art_arm64_trampoline_end

    /*
     * Whether the given program counter points to arm64 trampoline code.
     *
     * Arguments:
     *    8(%ebp) pc as left by the call x86 instruction (obtained from stack).
     *
     * Return:
     *    %eax whether the PC belongs to the arm64 trampoline code.
     */
DEFINE_FUNCTION art_is_arm64_trampoline
    call    art_arm64_trampoline_begin
    cmpl    %eax, 4(%esp)
    jbe     1f
    call    art_arm64_trampoline_end
    cmpl    %eax, 4(%esp)
    jae     1f
    mov     $1, %eax
    ret
1:
    xorl    %eax, %eax
    ret
END_FUNCTION art_is_arm64_trampoline
