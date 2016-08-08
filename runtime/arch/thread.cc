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
#include "arm/thread_arm.cc"
#elif defined(__arm64__)
#include "arm64/thread_arm64.cc"
#elif defined(__x86_64__)
#include "x86_64/thread_x86_64.cc"
#else
#include "x86/thread_x86.cc"
#endif
