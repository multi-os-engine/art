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

// Object members (fields, methods and arrays are derived from this).
.set object_klass_offset, 0
.set object_monitor_offset, 4
.set object_size, 8

// ArtField member offsets

.set field_declaring_class, object_size + 0
.set field_access_flags, object_size + 4
.set field_field_dex_idx, object_size + 8
.set field_offset, object_size + 12

// ArtMethod offset
.set method_declaring_class_offset, object_size+0
.set method_dex_stuff, object_size+4
.set method_access_flags_offset, object_size+56

// array constants
.set array_length_offset, object_size + 0
.set array_data_offset, object_size + 4

.set shadow_frame_number_of_vregs_offset, 0
.set shadow_frame_link_offset, 4
.set shadow_frame_method_offset, 8
.set shadow_frame_dexpc_offset, 12
.set shadow_frame_vregs_offset, 16

// Thread offsets
.set thread_flags_offset, 0
.set thread_suspend_count_offset, 4
.set thread_card_table_offset, 120
.set thread_exception_offset, 124
.set thread_stack_end_offset, 128

// Code item offsets (these are uint16_t)
.set code_item_registers_size_offset, 0
.set code_item_ins_size_offset, 2
.set code_item_outs_size_offset, 4
.set code_item_tries_size_offset,6
