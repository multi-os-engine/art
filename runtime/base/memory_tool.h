#ifndef ART_RUNTIME_BASE_SANITIZER_H_
#define ART_RUNTIME_BASE_SANITIZER_H_

#if !defined(__has_feature)
#define __has_feature(x) 0
#endif

#if __has_feature(address_sanitizer)

#include <sanitizer/asan_interface.h>
#define ADDRESS_SANITIZER
#define MAKE_MEM_NOACCESS(p, s) __asan_poison_memory_region(p, s)
#define MAKE_MEM_UNDEFINED(p, s) __asan_unpoison_memory_region(p, s)
#define MAKE_MEM_DEFINED(p, s) __asan_unpoison_memory_region(p, s)
#define RUNNING_ON_VALGRIND 0U
#define RUNNING_ON_MEMORY_TOOL 1U
#define RUNNING_ON_LEAK_CHECKER 1U
#define MEMORY_TOOL_ADDS_REDZONES 1U

#else

#include <valgrind.h>
#include <memcheck/memcheck.h>
#define MAKE_MEM_NOACCESS(p, s) VALGRIND_MAKE_MEM_NOACCESS(p, s)
#define MAKE_MEM_UNDEFINED(p, s) VALGRIND_MAKE_MEM_UNDEFINED(p, s)
#define MAKE_MEM_DEFINED(p, s) VALGRIND_MAKE_MEM_DEFINED(p, s)
#define RUNNING_ON_MEMORY_TOOL RUNNING_ON_VALGRIND
#define RUNNING_ON_LEAK_CHECKER RUNNING_ON_VALGRIND
#define MEMORY_TOOL_ADDS_REDZONES RUNNING_ON_VALGRIND

#endif

#endif  // ART_RUNTIME_BASE_SANITIZER_H_
