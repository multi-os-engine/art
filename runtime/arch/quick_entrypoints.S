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

#ifdef __arm__
#include "arm/quick_entrypoints_arm.S"

.section __IMPORT, __pointers, non_lazy_symbol_pointers

L__ZN3art7Runtime9instance_E$non_lazy_ptr:
	.indirect_symbol __ZN3art7Runtime9instance_E
    .long 0

.section __TEXT, __text

.macro RUNTIME_LOAD_DEF dest_reg
    ENTRY geteip_\dest_reg
        movw	\dest_reg, :lower16:(L__ZN3art7Runtime9instance_E$non_lazy_ptr-(.Lgeteip_\dest_reg+4))
        movt	\dest_reg, :upper16:(L__ZN3art7Runtime9instance_E$non_lazy_ptr-(.Lgeteip_\dest_reg+4))
    .Lgeteip_\dest_reg:
        add	\dest_reg, pc
        ldr \dest_reg, [\dest_reg]
        ldr \dest_reg, [\dest_reg]
        mov pc, lr
    END geteip_\dest_reg
.endm

RUNTIME_LOAD_DEF r0
RUNTIME_LOAD_DEF r1
RUNTIME_LOAD_DEF r2
RUNTIME_LOAD_DEF r3
RUNTIME_LOAD_DEF r12
#elif defined(__arm64__)
#define beq b.eq
#define bne b.ne
#define bpl b.pl
#define bhi b.hi
#define bgt b.gt

#include "arm64/quick_entrypoints_arm64.S"
#elif defined(__x86_64__)
#include "x86_64/quick_entrypoints_x86_64.S"
#else
#include "x86/quick_entrypoints_x86.S"
#endif
