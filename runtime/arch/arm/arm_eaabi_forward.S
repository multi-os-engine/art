/*
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

.extern ___divmoddi4
ENTRY __aeabi_ldivmod
    push    {r9, r11, lr}
    sub     sp, sp, #16
    add     r12, sp, #8
    str     r12, [sp]
    blx      ___divmoddi4
    ldr     r2, [sp, #8]
    ldr     r3, [sp, #12]
    add     sp, sp, #16
    pop     {r9, r11, pc}
END __aeabi_ldivmod

.extern ___divmodsi4
ENTRY __aeabi_idivmod
    push    {r9, r11, lr}
    sub     sp, sp, #8
    mov     r2, sp
    blx      ___divmodsi4
    ldr     r1, [sp]
    add     sp, sp, #8
    pop     {r9, r11, pc}
END __aeabi_idivmod
