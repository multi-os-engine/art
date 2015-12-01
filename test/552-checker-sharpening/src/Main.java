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

public class Main {

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static int throw_if_0 = 42;

  private static int $opt$noinline$foo(int x) {
    if (throw_if_0 == 0) throw new Error();
    return x;
  }

  /// CHECK-START: int Main.testSimple(int) sharpening (before)
  /// CHECK:                InvokeStaticOrDirect method_load_kind:dex_cache_via_method

  /// CHECK-START-ARM: int Main.testSimple(int) sharpening (after)
  /// CHECK-NOT:            ArmDexCacheArraysBase
  /// CHECK:                InvokeStaticOrDirect method_load_kind:dex_cache_pc_relative

  /// CHECK-START-ARM64: int Main.testSimple(int) sharpening (after)
  /// CHECK:                InvokeStaticOrDirect method_load_kind:dex_cache_pc_relative

  /// CHECK-START-X86: int Main.testSimple(int) sharpening (after)
  /// CHECK-NOT:            X86ComputeBaseMethodAddress
  /// CHECK:                InvokeStaticOrDirect method_load_kind:dex_cache_pc_relative

  /// CHECK-START-X86_64: int Main.testSimple(int) sharpening (after)
  /// CHECK:                InvokeStaticOrDirect method_load_kind:dex_cache_pc_relative

  /// CHECK-START-ARM: int Main.testSimple(int) dex_cache_array_fixups_arm (after)
  /// CHECK:                ArmDexCacheArraysBase
  /// CHECK-NOT:            ArmDexCacheArraysBase

  /// CHECK-START-X86: int Main.testSimple(int) pc_relative_fixups_x86 (after)
  /// CHECK:                X86ComputeBaseMethodAddress
  /// CHECK-NOT:            X86ComputeBaseMethodAddress

  public static int testSimple(int x) {
    // This call should use PC-relative dex cache array load to retrieve the target method.
    return $opt$noinline$foo(x);
  }

  /// CHECK-START: int Main.testDiamond(boolean, int) sharpening (before)
  /// CHECK:                InvokeStaticOrDirect method_load_kind:dex_cache_via_method

  /// CHECK-START-ARM: int Main.testDiamond(boolean, int) sharpening (after)
  /// CHECK-NOT:            ArmDexCacheArraysBase
  /// CHECK:                InvokeStaticOrDirect method_load_kind:dex_cache_pc_relative
  /// CHECK:                InvokeStaticOrDirect method_load_kind:dex_cache_pc_relative

  /// CHECK-START-ARM64: int Main.testDiamond(boolean, int) sharpening (after)
  /// CHECK:                InvokeStaticOrDirect method_load_kind:dex_cache_pc_relative
  /// CHECK:                InvokeStaticOrDirect method_load_kind:dex_cache_pc_relative

  /// CHECK-START-X86: int Main.testDiamond(boolean, int) sharpening (after)
  /// CHECK-NOT:            X86ComputeBaseMethodAddress
  /// CHECK:                InvokeStaticOrDirect method_load_kind:dex_cache_pc_relative
  /// CHECK:                InvokeStaticOrDirect method_load_kind:dex_cache_pc_relative

  /// CHECK-START-X86_64: int Main.testDiamond(boolean, int) sharpening (after)
  /// CHECK:                InvokeStaticOrDirect method_load_kind:dex_cache_pc_relative
  /// CHECK:                InvokeStaticOrDirect method_load_kind:dex_cache_pc_relative

  /// CHECK-START-ARM: int Main.testDiamond(boolean, int) dex_cache_array_fixups_arm (after)
  /// CHECK:                ArmDexCacheArraysBase
  /// CHECK-NOT:            ArmDexCacheArraysBase

  /// CHECK-START-ARM: int Main.testDiamond(boolean, int) dex_cache_array_fixups_arm (after)
  /// CHECK:                ArmDexCacheArraysBase
  /// CHECK-NEXT:           If

  /// CHECK-START-X86: int Main.testDiamond(boolean, int) pc_relative_fixups_x86 (after)
  /// CHECK:                X86ComputeBaseMethodAddress
  /// CHECK-NOT:            X86ComputeBaseMethodAddress

  /// CHECK-START-X86: int Main.testDiamond(boolean, int) pc_relative_fixups_x86 (after)
  /// CHECK:                X86ComputeBaseMethodAddress
  /// CHECK-NEXT:           If

  public static int testDiamond(boolean negate, int x) {
    // These calls should use PC-relative dex cache array loads to retrieve the target method.
    // PC-relative bases used by X86 and ARM should be pulled before the If.
    if (negate) {
      return $opt$noinline$foo(-x);
    } else {
      return $opt$noinline$foo(x);
    }
  }

  public static void main(String[] args) {
    assertIntEquals(1, testSimple(1));
    assertIntEquals(1, testDiamond(false, 1));
    assertIntEquals(-1, testDiamond(true, 1));
  }
}
