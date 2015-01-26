/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "parsed_options.h"

#include <sstream>

#ifdef HAVE_ANDROID_OS
#include "cutils/properties.h"
#endif

#include "base/stringpiece.h"
#include "debugger.h"
#include "gc/heap.h"
#include "monitor.h"
#include "runtime.h"
#include "trace.h"
#include "utils.h"

#include "cmdline_parser.h"
#include "runtime_options.h"

namespace art {

ParsedOptions::ParsedOptions()
    :
    boot_class_path_(nullptr),
    check_jni_(kIsDebugBuild),                      // -Xcheck:jni is off by default for regular
                                                    // builds but on by default in debug builds.
    force_copy_(false),
    compiler_callbacks_(nullptr),
    is_zygote_(false),
    must_relocate_(kDefaultMustRelocate),
    dex2oat_enabled_(true),
    image_dex2oat_enabled_(true),
    interpreter_only_(kPoisonHeapReferences),       // kPoisonHeapReferences currently works with
                                                    // the interpreter only.
                                                    // TODO: make it work with the compiler.
    is_explicit_gc_disabled_(false),
    use_tlab_(false),
    verify_pre_gc_heap_(false),
    verify_pre_sweeping_heap_(kIsDebugBuild),       // Pre sweeping is the one that usually fails
                                                    // if the GC corrupted the heap.
    verify_post_gc_heap_(false),
    verify_pre_gc_rosalloc_(kIsDebugBuild),
    verify_pre_sweeping_rosalloc_(false),
    verify_post_gc_rosalloc_(false),
    long_pause_log_threshold_(gc::Heap::kDefaultLongPauseLogThreshold),
    long_gc_log_threshold_(gc::Heap::kDefaultLongGCLogThreshold),
    dump_gc_performance_on_shutdown_(false),
    ignore_max_footprint_(false),
    heap_initial_size_(gc::Heap::kDefaultInitialSize),
    heap_maximum_size_(gc::Heap::kDefaultMaximumSize),
    heap_growth_limit_(0),                          // 0 means no growth limit.
    heap_min_free_(gc::Heap::kDefaultMinFree),
    heap_max_free_(gc::Heap::kDefaultMaxFree),
    heap_non_moving_space_capacity_(gc::Heap::kDefaultNonMovingSpaceCapacity),
    large_object_space_type_(gc::Heap::kDefaultLargeObjectSpaceType),
    large_object_threshold_(gc::Heap::kDefaultLargeObjectThreshold),
    heap_target_utilization_(gc::Heap::kDefaultTargetUtilization),
    foreground_heap_growth_multiplier_(gc::Heap::kDefaultHeapGrowthMultiplier),
    parallel_gc_threads_(1),
    conc_gc_threads_(0),                            // Only the main GC thread, no workers.
    collector_type_(                                // The default GC type is set in makefiles.
#if ART_DEFAULT_GC_TYPE_IS_CMS
        gc::kCollectorTypeCMS),
#elif ART_DEFAULT_GC_TYPE_IS_SS
        gc::kCollectorTypeSS),
#elif ART_DEFAULT_GC_TYPE_IS_GSS
    gc::kCollectorTypeGSS),
#else
    gc::kCollectorTypeCMS),
#error "ART default GC type must be set"
#endif
    background_collector_type_(gc::kCollectorTypeNone),
                                                    // If background_collector_type_ is
                                                    // kCollectorTypeNone, it defaults to the
                                                    // collector_type_ after parsing options. If
                                                    // you set this to kCollectorTypeHSpaceCompact
                                                    // then we will do an hspace compaction when
                                                    // we transition to background instead of a
                                                    // normal collector transition.
    stack_size_(0),                                 // 0 means default.
    max_spins_before_thin_lock_inflation_(Monitor::kDefaultMaxSpinsBeforeThinLockInflation),
    low_memory_mode_(false),
    lock_profiling_threshold_(0),
    method_trace_(false),
    method_trace_file_("/data/method-trace-file.bin"),
    method_trace_file_size_(10 * MB),
    hook_is_sensitive_thread_(nullptr),
    hook_vfprintf_(vfprintf),
    hook_exit_(exit),
    hook_abort_(nullptr),                           // We don't call abort(3) by default; see
                                                    // Runtime::Abort.
    profile_clock_source_(kDefaultTraceClockSource),
    verify_(true),
    image_isa_(kRuntimeISA),
    use_homogeneous_space_compaction_for_oom_(true),  // Enable hspace compaction on OOM by default.
    min_interval_homogeneous_space_compaction_by_oom_(MsToNs(100 * 1000))  // 100s.
    {}

ParsedOptions* ParsedOptions::Create(const RuntimeOptions& options, bool ignore_unrecognized) {
  std::unique_ptr<ParsedOptions> parsed(new ParsedOptions());
  if (parsed->Parse(options, ignore_unrecognized)) {
    return parsed.release();
  }
  return nullptr;
}

using RuntimeParser = CmdlineParser<RuntimeArgumentMap, RuntimeArgumentMap::Key>;

// Yes, the stack frame is huge. But we get called super early on (and just once)
// to pass the command line arguments, so we'll probably be ok.
// Ideas to avoid suppressing this diagnostic are welcome!
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wframe-larger-than="

static std::unique_ptr<RuntimeParser> MakeParser(bool ignore_unrecognized) {
  using M = RuntimeArgumentMap;

  std::unique_ptr<RuntimeParser::Builder> parser_builder =
      std::unique_ptr<RuntimeParser::Builder>(new RuntimeParser::Builder());

  auto&& partial_definition = parser_builder->
       Define("-Xzygote")
          .IntoKey(M::Zygote)
      .Define("-help")
          .IntoKey(M::Help)
      .Define("-showversion")
          .IntoKey(M::ShowVersion)
      .Define("-Xbootclasspath:_")
          .WithType<std::string>()
          .IntoKey(M::BootClassPath)
      .Define({"-classpath _", "-cp _"})
          .WithType<std::string>()
          .IntoKey(M::ClassPath)
      .Define("-Ximage:_")
          .WithType<std::string>()
          .IntoKey(M::Image)
      .Define("-Xcheck:jni")
          .IntoKey(M::CheckJni)
      .Define("-Xjniopts:forcecopy")
          .IntoKey(M::JniOptsForceCopy)
      .Define({"-Xrunjdwp:_", "-agentlib:jdwp=_"})
          .WithType<JDWP::JdwpOptions>()
          .IntoKey(M::JdwpOptions)
      .Define("-Xms_")
          .WithType<MemoryKiB>()
          .IntoKey(M::MemoryInitialSize)
      .Define("-Xmx_")
          .WithType<MemoryKiB>()
          .IntoKey(M::MemoryMaximumSize)
      .Define("-XX:HeapGrowthLimit=_")
          .WithType<MemoryKiB>()
          .IntoKey(M::HeapGrowthLimit)
      .Define("-XX:HeapMinFree=_")
          .WithType<MemoryKiB>()
          .IntoKey(M::HeapMinFree)
      .Define("-XX:HeapMaxFree=_")
          .WithType<MemoryKiB>()
          .IntoKey(M::HeapMaxFree)
      .Define("-XX:NonMovingSpaceCapacity=_")
          .WithType<MemoryKiB>()
          .IntoKey(M::NonMovingSpaceCapacity)
      .Define("-XX:HeapTargetUtilization=_")
          .WithType<double>().WithRange(0.1, 0.9)
          .IntoKey(M::HeapTargetUtilization)
      .Define("-XX:ForegroundHeapGrowthMultiplier=_")
          .WithType<double>().WithRange(0.1, 1.0)
          .IntoKey(M::ForegroundHeapGrowthMultiplier)
      .Define("-XX:ParallelGCThreads=_")
          .WithType<unsigned int>()
          .IntoKey(M::ParallelGCThreads)
      .Define("-XX:ConcGCThreads=_")
          .WithType<unsigned int>()
          .IntoKey(M::ConcGCThreads)
      .Define("-Xss_")
          .WithType<Memory<1>>()
          .IntoKey(M::StackSize)
      .Define("-XX:MaxSpinsBeforeThinLockInflation=_")
          .WithType<unsigned int>()
          .IntoKey(M::MaxSpinsBeforeThinLockInflation)
      .Define("-XX:LongPauseLogThreshold=_")
          .WithType<unsigned int>()
          .IntoKey(M::LongPauseLogThreshold)
      .Define("-XX:LongGCLogThreshold=_")
          .WithType<unsigned int>()
          .IntoKey(M::LongGCLogThreshold);

  partial_definition
      .Define("-XX:DumpGCPerformanceOnShutdown")
          .IntoKey(M::DumpGCPerformanceOnShutdown)
      .Define("-XX:IgnoreMaxFootprint")
          .IntoKey(M::IgnoreMaxFootprint)
      .Define("-XX:LowMemoryMode")
          .IntoKey(M::LowMemoryMode)
      .Define("-XX:UseTLAB")
          .IntoKey(M::UseTLAB)
      .Define({"-XX:EnableHSpaceCompactForOOM", "-XX:DisableHSpaceCompactForOOM"})
          .WithValues({true, false})
          .IntoKey(M::EnableHSpaceCompactForOOM)
      .Define("-D_")
          .WithType<std::vector<std::string>>().AppendValues()
          .IntoKey(M::PropertiesList)
      .Define("-Xjnitrace:_")
          .WithType<std::string>()
          .IntoKey(M::JniTrace)
      .Define("-Xpatchoat:_")
          .WithType<std::string>()
          .IntoKey(M::PatchOat)
      .Define({"-Xrelocate", "-Xnorelocate"})
          .WithValues({true, false})
          .IntoKey(M::Relocate)
      .Define({"-Xdex2oat", "-Xnodex2oat"})
          .WithValues({true, false})
          .IntoKey(M::Dex2Oat)
      .Define({"-Ximage-dex2oat", "-Xnoimage-dex2oat"})
          .WithValues({true, false})
          .IntoKey(M::ImageDex2Oat)
      .Define("-Xint")
          .IntoKey(M::Interpret)
      .Define("-Xgc:_")
          .WithType<XGcOption>()
          .IntoKey(M::GcOption)
      .Define("-XX:LargeObjectSpace=_")
          .WithType<gc::space::LargeObjectSpaceType>()
          .WithValueMap({{"disabled", gc::space::LargeObjectSpaceType::kDisabled},
                         {"freelist", gc::space::LargeObjectSpaceType::kFreeList},
                         {"map",      gc::space::LargeObjectSpaceType::kMap}})
          .IntoKey(M::LargeObjectSpace)
      .Define("-XX:LargeObjectThreshold=_")
          .WithType<Memory<1>>()
          .IntoKey(M::LargeObjectThreshold)
      .Define("-XX:BackgroundGC=_")
          .WithType<BackgroundGcOption>()
          .IntoKey(M::BackgroundGc)
      .Define("-XX:+DisableExplicitGC")
          .IntoKey(M::DisableExplicitGC)
      .Define("-verbose:_")
          .WithType<LogVerbosity>()
          .IntoKey(M::Verbose)
      .Define("-Xlockprofthreshold:_")
          .WithType<unsigned int>()
          .IntoKey(M::LockProfThreshold)
      .Define("-Xstacktracefile:_")
          .WithType<std::string>()
          .IntoKey(M::StackTraceFile)
      .Define("-Xmethod-trace")
          .IntoKey(M::MethodTrace)
      .Define("-Xmethod-trace-file:_")
          .WithType<std::string>()
      .Define("-Xmethod-trace-file-size:_")
          .WithType<unsigned int>()
      .Define("-Xprofile:_")
          .WithType<TraceClockSource>()
          .WithValueMap({{"threadcpuclock", TraceClockSource::kThreadCpu},
                         {"wallclock",      TraceClockSource::kWall},
                         {"dualclock",      TraceClockSource::kDual}})
          .IntoKey(M::ProfileClock)
      .Define("-Xenable-profiler")
          .WithType<TestProfilerOptions>()
          .AppendValues()
          .IntoKey(M::ProfilerOpts)  // NOTE: Appends into same key as -Xprofile-*
      .Define("-Xprofile-_")  // -Xprofile-<key>:<value>
          .WithType<TestProfilerOptions>()
          .AppendValues()
          .IntoKey(M::ProfilerOpts)  // NOTE: Appends into same key as -Xenable-profiler
      .Define("-Xcompiler:_")
          .WithType<std::string>()
          .IntoKey(M::Compiler)
      .Define("-Xcompiler-option _")
          .WithType<std::vector<std::string>>()
          .AppendValues()
          .IntoKey(M::CompilerOptions)
      .Define("-Ximage-compiler-option _")
          .WithType<std::vector<std::string>>()
          .AppendValues()
          .IntoKey(M::ImageCompilerOptions)
      .Define("-Xverify:_")
          .WithType<bool>()
          .WithValueMap({{"none", false},
                         {"remote", true},
                         {"all", true}})
          .IntoKey(M::Verify)
      .Define("-XX:NativeBridge=_")
          .WithType<std::string>()
          .IntoKey(M::NativeBridge)
      .Ignore({
          "-ea", "-da", "-enableassertions", "-disableassertions", "--runtime-arg", "-esa",
          "-dsa", "-enablesystemassertions", "-disablesystemassertions", "-Xrs", "-Xint:_",
          "-Xdexopt:_", "-Xnoquithandler", "-Xjnigreflimit:_", "-Xgenregmap", "-Xnogenregmap",
          "-Xverifyopt:_", "-Xcheckdexsum", "-Xincludeselectedop", "-Xjitop:_",
          "-Xincludeselectedmethod", "-Xjitthreshold:_", "-Xjitcodecachesize:_",
          "-Xjitblocking", "-Xjitmethod:_", "-Xjitclass:_", "-Xjitoffset:_",
          "-Xjitconfig:_", "-Xjitcheckcg", "-Xjitverbose", "-Xjitprofile",
          "-Xjitdisableopt", "-Xjitsuspendpoll", "-XX:mainThreadStackSize=_"})
      .IgnoreUnrecognized(ignore_unrecognized);

  return std::unique_ptr<RuntimeParser>(new RuntimeParser(parser_builder->Build()));
}

#pragma GCC diagnostic pop

static std::vector<std::string> ToArgvList(const RuntimeOptions& options) {
  std::vector<std::string> argv;
  for (auto&& pair : options) {
    argv.push_back(pair.first);
  }
  return std::move(argv);
}

bool ParsedOptions::Parse(const RuntimeOptions& options, bool ignore_unrecognized) {

//  gLogVerbosity.class_linker = true;  // TODO: don't check this in!
//  gLogVerbosity.compiler = true;  // TODO: don't check this in!
//  gLogVerbosity.gc = true;  // TODO: don't check this in!
//  gLogVerbosity.heap = true;  // TODO: don't check this in!
//  gLogVerbosity.jdwp = true;  // TODO: don't check this in!
//  gLogVerbosity.jni = true;  // TODO: don't check this in!
//  gLogVerbosity.monitor = true;  // TODO: don't check this in!
//  gLogVerbosity.profiler = true;  // TODO: don't check this in!
//  gLogVerbosity.signals = true;  // TODO: don't check this in!
//  gLogVerbosity.startup = true;  // TODO: don't check this in!
//  gLogVerbosity.third_party_jni = true;  // TODO: don't check this in!
//  gLogVerbosity.threads = true;  // TODO: don't check this in!
//  gLogVerbosity.verifier = true;  // TODO: don't check this in!

  for (size_t i = 0; i < options.size(); ++i) {
    if (true && options[0].first == "-Xzygote") {
      LOG(INFO) << "option[" << i << "]=" << options[i].first;
    }
  }

  auto parser = MakeParser(ignore_unrecognized);
  std::vector<std::string> argv_list = ToArgvList(options);
  CmdlineResult parse_result = parser->Parse(argv_list);

  // Handle parse errors by displaying the usage and potentially exiting.
  if (parse_result.IsError()) {
    if (parse_result.GetStatus() == CmdlineResult::kUsage) {
      UsageMessage(stdout, "%s\n", parse_result.GetMessage().c_str());
      Exit(0);
    } else if (parse_result.GetStatus() == CmdlineResult::kUnknown && !ignore_unrecognized) {
      Usage("%s\n", parse_result.GetMessage().c_str());
      return false;
    } else {
      Usage("%s\n", parse_result.GetMessage().c_str());
      Exit(0);
    }

    UNREACHABLE();
    return false;
  }

  using M = RuntimeArgumentMap;
  RuntimeArgumentMap args = parser->ReleaseArgumentsMap();

  // -help, -showversion, etc.
  if (args.Exists(M::Help)) {
    Usage(nullptr);
    return false;
  } else if (args.Exists(M::ShowVersion)) {
    UsageMessage(stdout, "ART version %s\n", Runtime::GetVersion());
    Exit(0);
  } else if (args.Exists(M::BootClassPath)) {
    LOG(INFO) << "setting boot class path to " << *args.Get(M::BootClassPath);
  }

  // Set a default boot class path if we didn't get an explicit one via command line.
  if (getenv("BOOTCLASSPATH") != nullptr) {
    args.SetIfMissing(M::BootClassPath, std::string(getenv("BOOTCLASSPATH")));
  }

  // Set a default class path if we didn't get an explicit one via command line.
  if (getenv("CLASSPATH") != nullptr) {
    args.SetIfMissing(M::ClassPath, std::string(getenv("CLASSPATH")));
  }

  // Default to number of processors minus one since the main GC thread also does work.
  args.SetIfMissing(M::ParallelGCThreads,
                    static_cast<unsigned int>(sysconf(_SC_NPROCESSORS_CONF) - 1u));

  // -Xverbose:
  {
    LogVerbosity *log_verbosity = args.Get(M::Verbose);
    if (log_verbosity != nullptr) {
      gLogVerbosity = *log_verbosity;
    }
  }

  // -Xprofile:
  Trace::SetDefaultClockSource(args.GetOrDefault(M::ProfileClock));

  // Handle MsToNs conversion for LogThreshold options
  {
    long_gc_log_threshold_ = MsToNs(args.GetOrDefault(M::LongGCLogThreshold));
    args.Set(M::LongGCLogThreshold, long_gc_log_threshold_);

    long_pause_log_threshold_ = MsToNs(args.GetOrDefault(M::LongPauseLogThreshold));
    args.Set(M::LongPauseLogThreshold, long_pause_log_threshold_);
  }

  // TODO: Set up keys for these instead, and move the below loop into JNI
  // Handle special options that set up hooks
  for (size_t i = 0; i < options.size(); ++i) {
    const std::string option(options[i].first);
    if (option == "-classpath" || option == "-cp") {
      // TODO: support -Djava.class.path
    } else if (option == "bootclasspath") {
      boot_class_path_
          = reinterpret_cast<const std::vector<const DexFile*>*>(options[i].second);
    } else if (option == "compilercallbacks") {
      compiler_callbacks_ =
          reinterpret_cast<CompilerCallbacks*>(const_cast<void*>(options[i].second));
    } else if (option == "imageinstructionset") {
      const char* isa_str = reinterpret_cast<const char*>(options[i].second);
      image_isa_ = GetInstructionSetFromString(isa_str);
      if (image_isa_ == kNone) {
        Usage("%s is not a valid instruction set.", isa_str);
        return false;
      }
    } else if (option == "sensitiveThread") {
      const void* hook = options[i].second;
      hook_is_sensitive_thread_ = reinterpret_cast<bool (*)()>(const_cast<void*>(hook));
    } else if (option == "vfprintf") {
      const void* hook = options[i].second;
      if (hook == nullptr) {
        Usage("vfprintf argument was NULL");
        return false;
      }
      hook_vfprintf_ =
          reinterpret_cast<int (*)(FILE *, const char*, va_list)>(const_cast<void*>(hook));
    } else if (option == "exit") {
      const void* hook = options[i].second;
      if (hook == nullptr) {
        Usage("exit argument was NULL");
        return false;
      }
      hook_exit_ = reinterpret_cast<void(*)(jint)>(const_cast<void*>(hook));
    } else if (option == "abort") {
      const void* hook = options[i].second;
      if (hook == nullptr) {
        Usage("abort was NULL\n");
        return false;
      }
      hook_abort_ = reinterpret_cast<void(*)()>(const_cast<void*>(hook));
    }
  }

  // If not set, background collector type defaults to homogeneous compaction.
  // If foreground is GSS, use GSS as background collector.
  // If not low memory mode, semispace otherwise.
  if (background_collector_type_ == gc::kCollectorTypeNone) {
    if (collector_type_ != gc::kCollectorTypeGSS) {
      background_collector_type_ = low_memory_mode_ ?
          gc::kCollectorTypeSS : gc::kCollectorTypeHomogeneousSpaceCompact;
    } else {
      background_collector_type_ = collector_type_;
    }
  }

  // If a reference to the dalvik core.jar snuck in, replace it with
  // the art specific version. This can happen with on device
  // boot.art/boot.oat generation by GenerateImage which relies on the
  // value of BOOTCLASSPATH.
#if defined(ART_TARGET)
  std::string core_jar("/core.jar");
  std::string core_libart_jar("/core-libart.jar");
#else
  // The host uses hostdex files.
  std::string core_jar("/core-hostdex.jar");
  std::string core_libart_jar("/core-libart-hostdex.jar");
#endif
  size_t core_jar_pos = boot_class_path_string_.find(core_jar);
  if (core_jar_pos != std::string::npos) {
    boot_class_path_string_.replace(core_jar_pos, core_jar.size(), core_libart_jar);
  }

  if (compiler_callbacks_ == nullptr && image_.empty()) {
    image_ += GetAndroidRoot();
    image_ += "/framework/boot.art";
  }
  if (heap_growth_limit_ == 0) {
    heap_growth_limit_ = heap_maximum_size_;
  }

  // TODO: return the arguments map instead of the ParsedOptions type.

  return true;
}

void ParsedOptions::Exit(int status) {
  hook_exit_(status);
}

void ParsedOptions::Abort() {
  hook_abort_();
}

void ParsedOptions::UsageMessageV(FILE* stream, const char* fmt, va_list ap) {
  hook_vfprintf_(stream, fmt, ap);
}

void ParsedOptions::UsageMessage(FILE* stream, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageMessageV(stream, fmt, ap);
  va_end(ap);
}

void ParsedOptions::Usage(const char* fmt, ...) {
  bool error = (fmt != nullptr);
  FILE* stream = error ? stderr : stdout;

  if (fmt != nullptr) {
    va_list ap;
    va_start(ap, fmt);
    UsageMessageV(stream, fmt, ap);
    va_end(ap);
  }

  const char* program = "dalvikvm";
  UsageMessage(stream, "%s: [options] class [argument ...]\n", program);
  UsageMessage(stream, "\n");
  UsageMessage(stream, "The following standard options are supported:\n");
  UsageMessage(stream, "  -classpath classpath (-cp classpath)\n");
  UsageMessage(stream, "  -Dproperty=value\n");
  UsageMessage(stream, "  -verbose:tag ('gc', 'jni', or 'class')\n");
  UsageMessage(stream, "  -showversion\n");
  UsageMessage(stream, "  -help\n");
  UsageMessage(stream, "  -agentlib:jdwp=options\n");
  UsageMessage(stream, "\n");

  UsageMessage(stream, "The following extended options are supported:\n");
  UsageMessage(stream, "  -Xrunjdwp:<options>\n");
  UsageMessage(stream, "  -Xbootclasspath:bootclasspath\n");
  UsageMessage(stream, "  -Xcheck:tag  (e.g. 'jni')\n");
  UsageMessage(stream, "  -XmsN (min heap, must be multiple of 1K, >= 1MB)\n");
  UsageMessage(stream, "  -XmxN (max heap, must be multiple of 1K, >= 2MB)\n");
  UsageMessage(stream, "  -XssN (stack size)\n");
  UsageMessage(stream, "  -Xint\n");
  UsageMessage(stream, "\n");

  UsageMessage(stream, "The following Dalvik options are supported:\n");
  UsageMessage(stream, "  -Xzygote\n");
  UsageMessage(stream, "  -Xjnitrace:substring (eg NativeClass or nativeMethod)\n");
  UsageMessage(stream, "  -Xstacktracefile:<filename>\n");
  UsageMessage(stream, "  -Xgc:[no]preverify\n");
  UsageMessage(stream, "  -Xgc:[no]postverify\n");
  UsageMessage(stream, "  -XX:HeapGrowthLimit=N\n");
  UsageMessage(stream, "  -XX:HeapMinFree=N\n");
  UsageMessage(stream, "  -XX:HeapMaxFree=N\n");
  UsageMessage(stream, "  -XX:NonMovingSpaceCapacity=N\n");
  UsageMessage(stream, "  -XX:HeapTargetUtilization=doublevalue\n");
  UsageMessage(stream, "  -XX:ForegroundHeapGrowthMultiplier=doublevalue\n");
  UsageMessage(stream, "  -XX:LowMemoryMode\n");
  UsageMessage(stream, "  -Xprofile:{threadcpuclock,wallclock,dualclock}\n");
  UsageMessage(stream, "\n");

  UsageMessage(stream, "The following unique to ART options are supported:\n");
  UsageMessage(stream, "  -Xgc:[no]preverify_rosalloc\n");
  UsageMessage(stream, "  -Xgc:[no]postsweepingverify_rosalloc\n");
  UsageMessage(stream, "  -Xgc:[no]postverify_rosalloc\n");
  UsageMessage(stream, "  -Xgc:[no]presweepingverify\n");
  UsageMessage(stream, "  -Ximage:filename\n");
  UsageMessage(stream, "  -XX:+DisableExplicitGC\n");
  UsageMessage(stream, "  -XX:ParallelGCThreads=integervalue\n");
  UsageMessage(stream, "  -XX:ConcGCThreads=integervalue\n");
  UsageMessage(stream, "  -XX:MaxSpinsBeforeThinLockInflation=integervalue\n");
  UsageMessage(stream, "  -XX:LongPauseLogThreshold=integervalue\n");
  UsageMessage(stream, "  -XX:LongGCLogThreshold=integervalue\n");
  UsageMessage(stream, "  -XX:DumpGCPerformanceOnShutdown\n");
  UsageMessage(stream, "  -XX:IgnoreMaxFootprint\n");
  UsageMessage(stream, "  -XX:UseTLAB\n");
  UsageMessage(stream, "  -XX:BackgroundGC=none\n");
  UsageMessage(stream, "  -XX:LargeObjectSpace={disabled,map,freelist}\n");
  UsageMessage(stream, "  -XX:LargeObjectThreshold=N\n");
  UsageMessage(stream, "  -Xmethod-trace\n");
  UsageMessage(stream, "  -Xmethod-trace-file:filename");
  UsageMessage(stream, "  -Xmethod-trace-file-size:integervalue\n");
  UsageMessage(stream, "  -Xenable-profiler\n");
  UsageMessage(stream, "  -Xprofile-filename:filename\n");
  UsageMessage(stream, "  -Xprofile-period:integervalue\n");
  UsageMessage(stream, "  -Xprofile-duration:integervalue\n");
  UsageMessage(stream, "  -Xprofile-interval:integervalue\n");
  UsageMessage(stream, "  -Xprofile-backoff:doublevalue\n");
  UsageMessage(stream, "  -Xprofile-start-immediately\n");
  UsageMessage(stream, "  -Xprofile-top-k-threshold:doublevalue\n");
  UsageMessage(stream, "  -Xprofile-top-k-change-threshold:doublevalue\n");
  UsageMessage(stream, "  -Xprofile-type:{method,stack}\n");
  UsageMessage(stream, "  -Xprofile-max-stack-depth:integervalue\n");
  UsageMessage(stream, "  -Xcompiler:filename\n");
  UsageMessage(stream, "  -Xcompiler-option dex2oat-option\n");
  UsageMessage(stream, "  -Ximage-compiler-option dex2oat-option\n");
  UsageMessage(stream, "  -Xpatchoat:filename\n");
  UsageMessage(stream, "  -X[no]relocate\n");
  UsageMessage(stream, "  -X[no]dex2oat (Whether to invoke dex2oat on the application)\n");
  UsageMessage(stream, "  -X[no]image-dex2oat (Whether to create and use a boot image)\n");
  UsageMessage(stream, "\n");

  UsageMessage(stream, "The following previously supported Dalvik options are ignored:\n");
  UsageMessage(stream, "  -ea[:<package name>... |:<class name>]\n");
  UsageMessage(stream, "  -da[:<package name>... |:<class name>]\n");
  UsageMessage(stream, "   (-enableassertions, -disableassertions)\n");
  UsageMessage(stream, "  -esa\n");
  UsageMessage(stream, "  -dsa\n");
  UsageMessage(stream, "   (-enablesystemassertions, -disablesystemassertions)\n");
  UsageMessage(stream, "  -Xverify:{none,remote,all}\n");
  UsageMessage(stream, "  -Xrs\n");
  UsageMessage(stream, "  -Xint:portable, -Xint:fast, -Xint:jit\n");
  UsageMessage(stream, "  -Xdexopt:{none,verified,all,full}\n");
  UsageMessage(stream, "  -Xnoquithandler\n");
  UsageMessage(stream, "  -Xjniopts:{warnonly,forcecopy}\n");
  UsageMessage(stream, "  -Xjnigreflimit:integervalue\n");
  UsageMessage(stream, "  -Xgc:[no]precise\n");
  UsageMessage(stream, "  -Xgc:[no]verifycardtable\n");
  UsageMessage(stream, "  -X[no]genregmap\n");
  UsageMessage(stream, "  -Xverifyopt:[no]checkmon\n");
  UsageMessage(stream, "  -Xcheckdexsum\n");
  UsageMessage(stream, "  -Xincludeselectedop\n");
  UsageMessage(stream, "  -Xjitop:hexopvalue[-endvalue][,hexopvalue[-endvalue]]*\n");
  UsageMessage(stream, "  -Xincludeselectedmethod\n");
  UsageMessage(stream, "  -Xjitthreshold:integervalue\n");
  UsageMessage(stream, "  -Xjitcodecachesize:decimalvalueofkbytes\n");
  UsageMessage(stream, "  -Xjitblocking\n");
  UsageMessage(stream, "  -Xjitmethod:signature[,signature]* (eg Ljava/lang/String\\;replace)\n");
  UsageMessage(stream, "  -Xjitclass:classname[,classname]*\n");
  UsageMessage(stream, "  -Xjitoffset:offset[,offset]\n");
  UsageMessage(stream, "  -Xjitconfig:filename\n");
  UsageMessage(stream, "  -Xjitcheckcg\n");
  UsageMessage(stream, "  -Xjitverbose\n");
  UsageMessage(stream, "  -Xjitprofile\n");
  UsageMessage(stream, "  -Xjitdisableopt\n");
  UsageMessage(stream, "  -Xjitsuspendpoll\n");
  UsageMessage(stream, "  -XX:mainThreadStackSize=N\n");
  UsageMessage(stream, "\n");

  Exit((error) ? 1 : 0);
}

}  // namespace art
