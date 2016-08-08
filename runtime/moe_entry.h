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

#ifndef ART_RUNTIME_MOE_ENTRY_H_
#define ART_RUNTIME_MOE_ENTRY_H_

#include <pthread.h>
#include <assert.h>
#include <list>
#include <errno.h>

#ifdef __LP64__
#define MOE_MAP_BEGIN 0x100000000
#define MOE_MAP_END 0x200000000
#else
#define MOE_MAP_BEGIN 0x000000000
#define MOE_MAP_END 0x100000000
#endif

#endif  // ART_RUNTIME_MOE_ENTRY_H_
