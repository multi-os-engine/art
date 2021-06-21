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
#include <TargetConditionals.h>

#define MOE_TLS_THREAD_KEY 340
#define MOE_TLS_SCRATCH_KEY (MOE_TLS_THREAD_KEY + 1)

//#if TARGET_OS_OSX

// When running on host, i.e., dex2oat
//#define MOE_MAP_BEGIN 0x000000000
//#define MOE_MAP_END 0x100000000

// #else

// When running on target, i.e., moe application
#ifdef __aarch64__
#define MOE_MAP_BEGIN 0x100000000
#define MOE_MAP_END 0x200000000
#else
#define MOE_MAP_BEGIN 0x000000000
#define MOE_MAP_END 0x100000000
#endif

// #endif

#if TARGET_OS_OSX

#include "base/logging.h"

static inline uintptr_t MOE_PTR_COMPRESS(uintptr_t ptr) {
  if (LIKELY(ptr != 0)) {
    CHECK_GE(ptr, MOE_MAP_BEGIN);
    CHECK_LT(ptr, MOE_MAP_END);

    ptr -= MOE_MAP_BEGIN;
  }

  return ptr;
}

static inline uintptr_t MOE_PTR_UNCOMPRESS(uintptr_t ptr) {
    if (ptr != 0) {
        ptr |= MOE_MAP_BEGIN;
    }

    return ptr;
}

#else
#define MOE_PTR_COMPRESS(x) (x)
#define MOE_PTR_UNCOMPRESS(x) (x)
#endif

static inline void reserve_tls_key() {
#if defined(__i386__) || defined(__x86_64__)
    pthread_key_t expected;
    std::list<pthread_key_t> to_release;

    while (true) {
        if (pthread_key_create(&expected, NULL) == EAGAIN || expected > MOE_TLS_THREAD_KEY) {
            assert(!"Could not reserve expected TLS slot!");
        }
        if (expected == MOE_TLS_THREAD_KEY) {
            break;
        }
        to_release.push_back(expected);
    }

    if (pthread_key_create(&expected, NULL) == EAGAIN || expected != MOE_TLS_SCRATCH_KEY) {
        assert(!"Could not reserve expected TLS slot!");
    }

    for (pthread_key_t key : to_release) {
        pthread_key_delete(key);
    }
#endif
}

#endif  // ART_RUNTIME_MOE_ENTRY_H_
