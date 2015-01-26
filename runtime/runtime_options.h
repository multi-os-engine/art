/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_RUNTIME_OPTIONS_H_
#define ART_RUNTIME_RUNTIME_OPTIONS_H_

#include <memory_representation.h>
#include "runtime/base/variant_map.h"

// Map keys
#include <vector>
#include <string>
#include "runtime/base/logging.h"
#include "cmdline/unit.h"
#include "jdwp/jdwp.h"
#include "gc/collector_type.h"
#include "gc/space/large_object_space.h"
#include "profiler_options.h"

namespace art {

#define DECLARE_KEY(Type, Name) static const Key<Type> Name
#define INSTANTIATE_KEY(Type, Name) const RuntimeArgumentMap::Key<Type> RuntimeArgumentMap::Name;

  // Define a key that is usable with a RuntimeArgumentMap.
  // This key will *not* work with other subtypes of VariantMap.
  template <typename TValue>
  struct RuntimeArgumentMapKey : VariantMapKey<TValue> {
    RuntimeArgumentMapKey() {}
  };

  // Defines a type-safe heterogeneous key->value map.
  // Use the VariantMap interface to look up or to store a RuntimeArgumentMapKey,Value pair.
  //
  // Example:
  //    auto map = RuntimeArgumentMap();
  //    map.Set(RuntimeArgumentMap::HeapTargetUtilization, 5.0);
  //    double *target_utilization = map.Get(RuntimeArgumentMap);
  //
  struct RuntimeArgumentMap : VariantMap<RuntimeArgumentMap, RuntimeArgumentMapKey> {
    // This 'using' line is necessary to inherit the variadic constructor.
    using VariantMap<RuntimeArgumentMap, RuntimeArgumentMapKey>::VariantMap;

    // Make the next many usages of Key slightly shorter to type.
    template <typename TValue>
    using Key = RuntimeArgumentMapKey<TValue>;

    using MemoryKiB = Memory<KB>;

    // List of key declarations, shorthand for 'static const Key<T> Name'

    DECLARE_KEY(Unit, Zygote);
    DECLARE_KEY(Unit, Help);
    DECLARE_KEY(Unit, ShowVersion);
    DECLARE_KEY(std::string, BootClassPath);
    DECLARE_KEY(std::string, ClassPath);
    DECLARE_KEY(std::string, Image);
    DECLARE_KEY(Unit, CheckJni);
    DECLARE_KEY(Unit, JniOptsForceCopy);
    DECLARE_KEY(JDWP::JdwpOptions, JdwpOptions);
    DECLARE_KEY(MemoryKiB, MemoryMaximumSize);  // -Xms
    DECLARE_KEY(MemoryKiB, MemoryInitialSize);  // -Xmx
    DECLARE_KEY(MemoryKiB, HeapGrowthLimit);
    DECLARE_KEY(MemoryKiB, HeapMinFree);
    DECLARE_KEY(MemoryKiB, HeapMaxFree);
    DECLARE_KEY(MemoryKiB, NonMovingSpaceCapacity);
    DECLARE_KEY(double, HeapTargetUtilization);
    DECLARE_KEY(double, ForegroundHeapGrowthMultiplier);
    DECLARE_KEY(unsigned int, ParallelGCThreads);
    DECLARE_KEY(unsigned int, ConcGCThreads);
    DECLARE_KEY(Memory<1>, StackSize);  // -Xss
    DECLARE_KEY(unsigned int, MaxSpinsBeforeThinLockInflation);
    DECLARE_KEY(unsigned int, LongPauseLogThreshold);
    DECLARE_KEY(unsigned int, LongGCLogThreshold);
    DECLARE_KEY(Unit, DumpGCPerformanceOnShutdown);
    DECLARE_KEY(Unit, IgnoreMaxFootprint);
    DECLARE_KEY(Unit, LowMemoryMode);
    DECLARE_KEY(Unit, UseTLAB);
    DECLARE_KEY(bool, EnableHSpaceCompactForOOM);
    DECLARE_KEY(std::vector<std::string>, PropertiesList);  // -D<whatever> -D<whatever> ...
    DECLARE_KEY(std::string, JniTrace);
    DECLARE_KEY(std::string, PatchOat);
    DECLARE_KEY(bool, Relocate);
    DECLARE_KEY(bool, Dex2Oat);
    DECLARE_KEY(bool, ImageDex2Oat);
    DECLARE_KEY(Unit, Interpret);  // -Xint
    DECLARE_KEY(XGcOption, GcOption);  // -Xgc:
    DECLARE_KEY(gc::space::LargeObjectSpaceType, LargeObjectSpace);
    DECLARE_KEY(Memory<1>, LargeObjectThreshold);
    DECLARE_KEY(BackgroundGcOption, BackgroundGc);
    DECLARE_KEY(Unit, DisableExplicitGC);
    DECLARE_KEY(LogVerbosity, Verbose);
    DECLARE_KEY(unsigned int, LockProfThreshold);
    DECLARE_KEY(std::string, StackTraceFile);
    DECLARE_KEY(Unit, MethodTrace);
    DECLARE_KEY(std::string, MethodTraceFile);
    DECLARE_KEY(std::string, MethodTraceFileSize);
    DECLARE_KEY(TraceClockSource, ProfileClock);  // -Xprofile:
    DECLARE_KEY(TestProfilerOptions, ProfilerOpts);  // -Xenable-profiler, -Xprofile-*
    DECLARE_KEY(std::string, Compiler);
    DECLARE_KEY(std::vector<std::string>, CompilerOptions);  // -Xcompiler-option ...
    DECLARE_KEY(std::vector<std::string>, ImageCompilerOptions);  // -Ximage-compiler-option ...
    DECLARE_KEY(bool, Verify);
    DECLARE_KEY(std::string, NativeBridge);
  };

  using MemoryKiB = Memory<1024>;

  // List of key initializations

  INSTANTIATE_KEY(Unit, Zygote);
  INSTANTIATE_KEY(Unit, Help);
  INSTANTIATE_KEY(Unit, ShowVersion);
  INSTANTIATE_KEY(std::string, BootClassPath);
  INSTANTIATE_KEY(std::string, ClassPath);
  INSTANTIATE_KEY(std::string, Image);
  INSTANTIATE_KEY(Unit, CheckJni);
  INSTANTIATE_KEY(Unit, JniOptsForceCopy);
  INSTANTIATE_KEY(JDWP::JdwpOptions, JdwpOptions);
  INSTANTIATE_KEY(MemoryKiB, MemoryMaximumSize);  // -Xmx
  INSTANTIATE_KEY(MemoryKiB, MemoryInitialSize);  // -Xms
  INSTANTIATE_KEY(MemoryKiB, HeapGrowthLimit);
  INSTANTIATE_KEY(MemoryKiB, HeapMinFree);
  INSTANTIATE_KEY(MemoryKiB, HeapMaxFree);
  INSTANTIATE_KEY(MemoryKiB, NonMovingSpaceCapacity);
  INSTANTIATE_KEY(double, HeapTargetUtilization);
  INSTANTIATE_KEY(double, ForegroundHeapGrowthMultiplier);
  INSTANTIATE_KEY(unsigned int, ParallelGCThreads);
  INSTANTIATE_KEY(unsigned int, ConcGCThreads);
  INSTANTIATE_KEY(Memory<1>, StackSize);  // -Xss
  INSTANTIATE_KEY(unsigned int, MaxSpinsBeforeThinLockInflation);
  INSTANTIATE_KEY(unsigned int, LongPauseLogThreshold);
  INSTANTIATE_KEY(unsigned int, LongGCLogThreshold);
  INSTANTIATE_KEY(Unit, DumpGCPerformanceOnShutdown);
  INSTANTIATE_KEY(Unit, IgnoreMaxFootprint);
  INSTANTIATE_KEY(Unit, LowMemoryMode);
  INSTANTIATE_KEY(Unit, UseTLAB);
  INSTANTIATE_KEY(bool, EnableHSpaceCompactForOOM);
  INSTANTIATE_KEY(std::vector<std::string>, PropertiesList);  // -D<whatever> -D<whatever> ...
  INSTANTIATE_KEY(std::string, JniTrace);
  INSTANTIATE_KEY(std::string, PatchOat);
  INSTANTIATE_KEY(bool, Relocate);
  INSTANTIATE_KEY(bool, Dex2Oat);
  INSTANTIATE_KEY(bool, ImageDex2Oat);
  INSTANTIATE_KEY(Unit, Interpret);  // -Xint
  INSTANTIATE_KEY(XGcOption, GcOption);  // -Xgc:
  INSTANTIATE_KEY(gc::space::LargeObjectSpaceType, LargeObjectSpace);
  INSTANTIATE_KEY(Memory<1>, LargeObjectThreshold);
  INSTANTIATE_KEY(BackgroundGcOption, BackgroundGc);
  INSTANTIATE_KEY(Unit, DisableExplicitGC);
  INSTANTIATE_KEY(LogVerbosity, Verbose);
  INSTANTIATE_KEY(unsigned int, LockProfThreshold);
  INSTANTIATE_KEY(std::string, StackTraceFile);
  INSTANTIATE_KEY(Unit, MethodTrace);
  INSTANTIATE_KEY(std::string, MethodTraceFile);
  INSTANTIATE_KEY(std::string, MethodTraceFileSize);
  INSTANTIATE_KEY(TraceClockSource, ProfileClock);  // -Xprofile:
  INSTANTIATE_KEY(TestProfilerOptions, ProfilerOpts);  // -Xenable-profiler, -Xprofile-*
  INSTANTIATE_KEY(std::string, Compiler);
  INSTANTIATE_KEY(std::vector<std::string>, CompilerOptions);  // -Xcompiler-option ...
  INSTANTIATE_KEY(std::vector<std::string>, ImageCompilerOptions);  // -Ximage-compiler-option ...
  INSTANTIATE_KEY(bool, Verify);
  INSTANTIATE_KEY(std::string, NativeBridge);

#undef DECLARE_KEY
#undef INSTANTIATE_KEY

  // using RuntimeOptions = RuntimeArgumentMap;
}  // namespace art

#endif  // ART_RUNTIME_RUNTIME_OPTIONS_H_
