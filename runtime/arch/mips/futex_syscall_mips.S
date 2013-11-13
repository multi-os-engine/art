/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "asm_support_mips.S"

#include <asm/unistd.h>

    .set noreorder

/*
 * futex_syscall(volatile int *, int op, int val, const struct timespec *timeout, volatile int *uaddr2, int val3);
 */
ENTRY futex_syscall
    GENERATE_GLOBAL_POINTER
    li     $v0, __NR_futex
    syscall
    bnez   $a3, 1f
     move  $a0, $v0
    j      $ra
     nop
1:
    la     $t9, __set_errno
    j      $t9
     nop
END futex_syscall
