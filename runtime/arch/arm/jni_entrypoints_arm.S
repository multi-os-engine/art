/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright 2014-2016 Intel Corporation
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

#include "asm_support_arm.S"

#ifdef MOE
#define artFindNativeMethod _artFindNativeMethod
#define artWorkAroundAppJniBugs _artWorkAroundAppJniBugs
#endif

    /*
     * Jni dlsym lookup stub.
     */
    .extern artFindNativeMethod
ENTRY art_jni_dlsym_lookup_stub
    #ifndef MOE
    push   {r0, r1, r2, r3, lr}           @ spill regs
    .cfi_adjust_cfa_offset 20
    .cfi_rel_offset r0, 0
    .cfi_rel_offset r1, 4
    .cfi_rel_offset r2, 8
    .cfi_rel_offset r3, 12
    .cfi_rel_offset lr, 16
    #else
    push   {r0, r1, r2, r3, r10, lr}           @ spill regs
    .cfi_adjust_cfa_offset 24
    .cfi_rel_offset r0, 0
    .cfi_rel_offset r1, 4
    .cfi_rel_offset r2, 8
    .cfi_rel_offset r3, 12
    .cfi_rel_offset r10, 16
    .cfi_rel_offset lr, 20
    #endif
    #ifndef MOE
    sub    sp, #12                        @ pad stack pointer to align frame
    .cfi_adjust_cfa_offset 12
    #else
    mov     r10, r9
    #endif
    blx    artFindNativeMethod
    #ifdef MOE
    mov     r9, r10
    #endif
    mov    r12, r0                        @ save result in r12
    #ifndef MOE
    add    sp, #12                        @ restore stack pointer
    .cfi_adjust_cfa_offset -12
    #endif
    cbz    r0, 1f                         @ is method code null?
    #ifndef MOE
    pop    {r0, r1, r2, r3, lr}           @ restore regs
    .cfi_adjust_cfa_offset -20
    .cfi_restore r0
    .cfi_restore r1
    .cfi_restore r2
    .cfi_restore r3
    .cfi_restore lr
    #else
    pop    {r0, r1, r2, r3, r10, lr}           @ restore regs
    .cfi_adjust_cfa_offset -24
    .cfi_restore r0
    .cfi_restore r1
    .cfi_restore r2
    .cfi_restore r3
    .cfi_restore r10
    .cfi_restore lr
    #endif
    bx     r12                            @ if non-null, tail call to method's code
1:
    #ifndef MOE
    pop    {r0, r1, r2, r3, pc}           @ restore regs and return to caller to handle exception
    #else
    pop    {r0, r1, r2, r3, r10, pc}           @ restore regs and return to caller to handle exception
    #endif
END art_jni_dlsym_lookup_stub
