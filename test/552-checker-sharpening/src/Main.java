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

  public static void assertStringEquals(String expected, String result) {
    if (expected != null ? !expected.equals(result) : result != null) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static boolean doThrow = false;

  private static int $noinline$foo(int x) {
    if (doThrow) { throw new Error(); }
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
    return $noinline$foo(x);
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
      return $noinline$foo(-x);
    } else {
      return $noinline$foo(x);
    }
  }

  /// CHECK-START-X86: int Main.testLoop(int[], int) pc_relative_fixups_x86 (before)
  /// CHECK-NOT:            X86ComputeBaseMethodAddress

  /// CHECK-START-X86: int Main.testLoop(int[], int) pc_relative_fixups_x86 (after)
  /// CHECK:                X86ComputeBaseMethodAddress
  /// CHECK-NOT:            X86ComputeBaseMethodAddress

  /// CHECK-START-X86: int Main.testLoop(int[], int) pc_relative_fixups_x86 (after)
  /// CHECK:                InvokeStaticOrDirect
  /// CHECK-NOT:            InvokeStaticOrDirect

  /// CHECK-START-X86: int Main.testLoop(int[], int) pc_relative_fixups_x86 (after)
  /// CHECK:                ArrayLength
  /// CHECK-NEXT:           X86ComputeBaseMethodAddress
  /// CHECK-NEXT:           Goto
  /// CHECK:                begin_block
  /// CHECK:                InvokeStaticOrDirect method_load_kind:dex_cache_pc_relative

  /// CHECK-START-ARM: int Main.testLoop(int[], int) dex_cache_array_fixups_arm (before)
  /// CHECK-NOT:            ArmDexCacheArraysBase

  /// CHECK-START-ARM: int Main.testLoop(int[], int) dex_cache_array_fixups_arm (after)
  /// CHECK:                ArmDexCacheArraysBase
  /// CHECK-NOT:            ArmDexCacheArraysBase

  /// CHECK-START-ARM: int Main.testLoop(int[], int) dex_cache_array_fixups_arm (after)
  /// CHECK:                InvokeStaticOrDirect
  /// CHECK-NOT:            InvokeStaticOrDirect

  /// CHECK-START-ARM: int Main.testLoop(int[], int) dex_cache_array_fixups_arm (after)
  /// CHECK:                ArrayLength
  /// CHECK-NEXT:           ArmDexCacheArraysBase
  /// CHECK-NEXT:           Goto
  /// CHECK:                begin_block
  /// CHECK:                InvokeStaticOrDirect method_load_kind:dex_cache_pc_relative

  public static int testLoop(int[] array, int x) {
    // PC-relative bases used by X86 and ARM should be pulled before the loop.
    for (int i : array) {
      x += $noinline$foo(i);
    }
    return x;
  }

  /// CHECK-START-X86: int Main.testLoopWithDiamond(int[], boolean, int) pc_relative_fixups_x86 (before)
  /// CHECK-NOT:            X86ComputeBaseMethodAddress

  /// CHECK-START-X86: int Main.testLoopWithDiamond(int[], boolean, int) pc_relative_fixups_x86 (after)
  /// CHECK:                If
  /// CHECK:                begin_block
  /// CHECK:                ArrayLength
  /// CHECK-NEXT:           X86ComputeBaseMethodAddress
  /// CHECK-NEXT:           Goto

  /// CHECK-START-ARM: int Main.testLoopWithDiamond(int[], boolean, int) dex_cache_array_fixups_arm (before)
  /// CHECK-NOT:            ArmDexCacheArraysBase

  /// CHECK-START-ARM: int Main.testLoopWithDiamond(int[], boolean, int) dex_cache_array_fixups_arm (after)
  /// CHECK:                If
  /// CHECK:                begin_block
  /// CHECK:                ArrayLength
  /// CHECK-NEXT:           ArmDexCacheArraysBase
  /// CHECK-NEXT:           Goto

  public static int testLoopWithDiamond(int[] array, boolean negate, int x) {
    // PC-relative bases used by X86 and ARM should be pulled before the loop
    // but not outside the if.
    if (array != null) {
      for (int i : array) {
        if (negate) {
          x += $noinline$foo(-i);
        } else {
          x += $noinline$foo(i);
        }
      }
    }
    return x;
  }

  /// CHECK-START: java.lang.String Main.$noinline$getBootImageString() sharpening (before)
  /// CHECK:                LoadString load_kind:DexCacheViaMethod

  /// CHECK-START-X86: java.lang.String Main.$noinline$getBootImageString() sharpening (after)
  // Note: load kind depends on PIC/non-PIC
  // TODO: Remove DexCacheViaMethod when read barrier config supports BootImageAddress.
  /// CHECK:                LoadString load_kind:{{BootImageAddress|DexCachePcRelative|DexCacheViaMethod}}

  /// CHECK-START-X86_64: java.lang.String Main.$noinline$getBootImageString() sharpening (after)
  // Note: load kind depends on PIC/non-PIC
  // TODO: Remove DexCacheViaMethod when read barrier config supports BootImageAddress.
  /// CHECK:                LoadString load_kind:{{BootImageAddress|DexCachePcRelative|DexCacheViaMethod}}

  /// CHECK-START-ARM: java.lang.String Main.$noinline$getBootImageString() sharpening (after)
  // Note: load kind depends on PIC/non-PIC
  // TODO: Remove DexCacheViaMethod when read barrier config supports BootImageAddress.
  /// CHECK:                LoadString load_kind:{{BootImageAddress|DexCachePcRelative|DexCacheViaMethod}}

  /// CHECK-START-ARM64: java.lang.String Main.$noinline$getBootImageString() sharpening (after)
  // Note: load kind depends on PIC/non-PIC
  // TODO: Remove DexCacheViaMethod when read barrier config supports BootImageAddress.
  /// CHECK:                LoadString load_kind:{{BootImageAddress|DexCachePcRelative|DexCacheViaMethod}}

  public static String $noinline$getBootImageString() {
    // Prevent inlining to avoid the string comparison being optimized away.
    if (doThrow) { throw new Error(); }
    // Empty string is known to be in the boot image.
    return "";
  }

  /// CHECK-START: java.lang.String Main.$noinline$getNonBootImageString() sharpening (before)
  /// CHECK:                LoadString load_kind:DexCacheViaMethod

  /// CHECK-START-X86: java.lang.String Main.$noinline$getNonBootImageString() sharpening (after)
  /// CHECK:                LoadString load_kind:DexCachePcRelative

  /// CHECK-START-X86: java.lang.String Main.$noinline$getNonBootImageString() pc_relative_fixups_x86 (after)
  /// CHECK-DAG:            X86ComputeBaseMethodAddress
  /// CHECK-DAG:            LoadString load_kind:DexCachePcRelative

  /// CHECK-START-X86_64: java.lang.String Main.$noinline$getNonBootImageString() sharpening (after)
  /// CHECK:                LoadString load_kind:DexCachePcRelative

  /// CHECK-START-ARM: java.lang.String Main.$noinline$getNonBootImageString() sharpening (after)
  /// CHECK:                LoadString load_kind:DexCachePcRelative

  /// CHECK-START-ARM: java.lang.String Main.$noinline$getNonBootImageString() dex_cache_array_fixups_arm (after)
  /// CHECK-DAG:            ArmDexCacheArraysBase
  /// CHECK-DAG:            LoadString load_kind:DexCachePcRelative

  /// CHECK-START-ARM64: java.lang.String Main.$noinline$getNonBootImageString() sharpening (after)
  /// CHECK:                LoadString load_kind:DexCachePcRelative

  public static String $noinline$getNonBootImageString() {
    // Prevent inlining to avoid the string comparison being optimized away.
    if (doThrow) { throw new Error(); }
    // This string is not in the boot image.
    return "non-boot-image-string";
  }

  /// CHECK-START: int Main.testVirtualFinalExactBase() sharpening (before)
  /// CHECK-DAG:            InvokeVirtual method_name:Base.$opt$noinline$getIntFinal

  /// CHECK-START: int Main.testVirtualFinalExactBase() sharpening (after)
  /// CHECK-DAG:            InvokeStaticOrDirect method_name:Base.$opt$noinline$getIntFinal

  /// CHECK-START: int Main.testVirtualFinalExactBase() sharpening (after)
  /// CHECK-NOT:            InvokeVirtual method_name:Base.$opt$noinline$getIntFinal

  public static int testVirtualFinalExactBase() {
    Base b = new Base();
    return b.$opt$noinline$getIntFinal();
  }

  /// CHECK-START: int Main.testVirtualNonFinalExactBase() sharpening (before)
  /// CHECK-DAG:            InvokeVirtual method_name:Base.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalExactBase() sharpening (after)
  /// CHECK-DAG:            InvokeStaticOrDirect method_name:Base.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalExactBase() sharpening (after)
  /// CHECK-NOT:            InvokeVirtual method_name:Base.$opt$noinline$getInt

  public static int testVirtualNonFinalExactBase() {
    Base b = new Base();
    return b.$opt$noinline$getInt();
  }

  /// CHECK-START: int Main.testVirtualNonFinalExactDerived1() sharpening (before)
  /// CHECK-DAG:            InvokeVirtual method_name:Base.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalExactDerived1() sharpening (after)
  /// CHECK-DAG:            InvokeStaticOrDirect method_name:Derived1.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalExactDerived1() sharpening (after)
  /// CHECK-NOT:            InvokeStaticOrDirect method_name:Base.$opt$noinline$getInt
  /// CHECK-NOT:            InvokeVirtual method_name:Base.$opt$noinline$getInt

  public static int testVirtualNonFinalExactDerived1() {
    Base b = new Derived1();
    return b.$opt$noinline$getInt();
  }

  /// CHECK-START: int Main.testVirtualNonFinalExactDerived2() sharpening (before)
  /// CHECK-DAG:            InvokeVirtual method_name:Base.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalExactDerived2() sharpening (after)
  /// CHECK-DAG:            InvokeStaticOrDirect method_name:Derived2.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalExactDerived2() sharpening (after)
  /// CHECK-NOT:            InvokeStaticOrDirect method_name:Base.$opt$noinline$getInt
  /// CHECK-NOT:            InvokeVirtual method_name:Base.$opt$noinline$getInt

  public static int testVirtualNonFinalExactDerived2() {
    Base b = new Derived2();
    return b.$opt$noinline$getInt();
  }

  /// CHECK-START: int Main.testVirtualNonFinalExactDerived3() sharpening (before)
  /// CHECK-DAG:            InvokeVirtual method_name:Base.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalExactDerived3() sharpening (after)
  /// CHECK-DAG:            InvokeStaticOrDirect method_name:Base.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalExactDerived3() sharpening (after)
  /// CHECK-NOT:            InvokeStaticOrDirect method_name:Derived3.$opt$noinline$getInt
  /// CHECK-NOT:            InvokeVirtual method_name:Base.$opt$noinline$getInt

  public static int testVirtualNonFinalExactDerived3() {
    Base b = new Derived3();
    return b.$opt$noinline$getInt();
  }

  /// CHECK-START: int Main.testVirtualNonFinalExactDerived4() sharpening (before)
  /// CHECK-DAG:            InvokeVirtual method_name:Base.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalExactDerived4() sharpening (after)
  /// CHECK-DAG:            InvokeStaticOrDirect method_name:Derived4.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalExactDerived4() sharpening (after)
  /// CHECK-NOT:            InvokeStaticOrDirect method_name:Base.$opt$noinline$getInt
  /// CHECK-NOT:            InvokeVirtual method_name:Base.$opt$noinline$getInt

  public static int testVirtualNonFinalExactDerived4() {
    Base b = new Derived4();
    return b.$opt$noinline$getInt();
  }

  /// CHECK-START: int Main.testVirtualFinalNonExactBase(Base) sharpening (before)
  /// CHECK-DAG:            InvokeVirtual method_name:Base.$opt$noinline$getIntFinal

  /// CHECK-START: int Main.testVirtualFinalNonExactBase(Base) sharpening (after)
  /// CHECK-DAG:            InvokeStaticOrDirect method_name:Base.$opt$noinline$getIntFinal

  /// CHECK-START: int Main.testVirtualFinalNonExactBase(Base) sharpening (after)
  /// CHECK-NOT:            InvokeVirtual method_name:Base.$opt$noinline$getIntFinal

  public static int testVirtualFinalNonExactBase(Base b) {
    return b.$opt$noinline$getIntFinal();
  }

  /// CHECK-START: int Main.testVirtualNonFinalNonExactBase(Base) sharpening (before)
  /// CHECK-DAG:            InvokeVirtual method_name:Base.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalNonExactBase(Base) sharpening (after)
  /// CHECK-DAG:            InvokeVirtual method_name:Base.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalNonExactBase(Base) sharpening (after)
  /// CHECK-NOT:            InvokeStaticOrDirect method_name:Base.$opt$noinline$getInt

  public static int testVirtualNonFinalNonExactBase(Base b) {
    return b.$opt$noinline$getInt();
  }

  /// CHECK-START: int Main.testVirtualNonFinalNonExactDerived1(Derived1) sharpening (before)
  /// CHECK-DAG:            InvokeVirtual method_name:Base.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalNonExactDerived1(Derived1) sharpening (after)
  /// CHECK-DAG:            InvokeStaticOrDirect method_name:Derived1.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalNonExactDerived1(Derived1) sharpening (after)
  /// CHECK-NOT:            InvokeVirtual method_name:Base.$opt$noinline$getInt
  /// CHECK-NOT:            InvokeStaticOrDirect method_name:Base.$opt$noinline$getInt

  public static int testVirtualNonFinalNonExactDerived1(Derived1 d) {
    Base b = d;
    return b.$opt$noinline$getInt();
  }

  /// CHECK-START: int Main.testVirtualNonFinalNonExactDerived2(Derived2) sharpening (before)
  /// CHECK-DAG:            InvokeVirtual method_name:Base.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalNonExactDerived2(Derived2) sharpening (after)
  /// CHECK-DAG:            InvokeStaticOrDirect method_name:Derived2.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalNonExactDerived2(Derived2) sharpening (after)
  /// CHECK-NOT:            InvokeVirtual method_name:Base.$opt$noinline$getInt
  /// CHECK-NOT:            InvokeStaticOrDirect method_name:Base.$opt$noinline$getInt

  public static int testVirtualNonFinalNonExactDerived2(Derived2 d) {
    Base b = d;
    return b.$opt$noinline$getInt();
  }

  /// CHECK-START: int Main.testVirtualNonFinalNonExactDerived3(Derived3) sharpening (before)
  /// CHECK-DAG:            InvokeVirtual method_name:Base.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalNonExactDerived3(Derived3) sharpening (after)
  /// CHECK-DAG:            InvokeStaticOrDirect method_name:Base.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalNonExactDerived3(Derived3) sharpening (after)
  /// CHECK-NOT:            InvokeVirtual method_name:Base.$opt$noinline$getInt

  public static int testVirtualNonFinalNonExactDerived3(Derived3 d) {
    Base b = d;
    return b.$opt$noinline$getInt();
  }

  /// CHECK-START: int Main.testVirtualNonFinalNonExactDerived4(Derived4) sharpening (before)
  /// CHECK-DAG:            InvokeVirtual method_name:Base.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalNonExactDerived4(Derived4) sharpening (after)
  /// CHECK-DAG:            InvokeVirtual method_name:Base.$opt$noinline$getInt

  /// CHECK-START: int Main.testVirtualNonFinalNonExactDerived4(Derived4) sharpening (after)
  /// CHECK-NOT:            InvokeStaticOrDirect method_name:Base.$opt$noinline$getInt
  /// CHECK-NOT:            InvokeStaticOrDirect method_name:Derived4.$opt$noinline$getInt

  public static int testVirtualNonFinalNonExactDerived4(Derived4 d) {
    Base b = d;
    return b.$opt$noinline$getInt();
  }

  public static void main(String[] args) {
    assertIntEquals(1, testSimple(1));
    assertIntEquals(1, testDiamond(false, 1));
    assertIntEquals(-1, testDiamond(true, 1));
    assertIntEquals(3, testLoop(new int[]{ 2 }, 1));
    assertIntEquals(8, testLoop(new int[]{ 3, 4 }, 1));
    assertIntEquals(1, testLoopWithDiamond(null, false, 1));
    assertIntEquals(3, testLoopWithDiamond(new int[]{ 2 }, false, 1));
    assertIntEquals(-6, testLoopWithDiamond(new int[]{ 3, 4 }, true, 1));
    assertStringEquals("", $noinline$getBootImageString());
    assertStringEquals("non-boot-image-string", $noinline$getNonBootImageString());

    assertIntEquals(-1, testVirtualFinalExactBase());
    assertIntEquals(1, testVirtualNonFinalExactBase());
    assertIntEquals(11, testVirtualNonFinalExactDerived1());
    assertIntEquals(21, testVirtualNonFinalExactDerived2());
    assertIntEquals(1, testVirtualNonFinalExactDerived3());
    assertIntEquals(41, testVirtualNonFinalExactDerived4());
    assertIntEquals(-1, testVirtualFinalNonExactBase(new Base()));
    assertIntEquals(1, testVirtualNonFinalNonExactBase(new Base()));
    assertIntEquals(11, testVirtualNonFinalNonExactDerived1(new Derived1()));
    assertIntEquals(21, testVirtualNonFinalNonExactDerived2(new Derived2()));
    assertIntEquals(1, testVirtualNonFinalNonExactDerived3(new Derived3()));
    assertIntEquals(41, testVirtualNonFinalNonExactDerived4(new Derived4()));
  }
}

class Base {
  public final int $opt$noinline$getIntFinal() {
    if (doThrow) { throw new Error(); }
    return -1;
  }

  public int $opt$noinline$getInt() {
    if (doThrow) { throw new Error(); }
    return 1;
  }

  public static boolean doThrow = false;
}

class Derived1 extends Base {
  public final int $opt$noinline$getInt() {
    if (doThrow) { throw new Error(); }
    return 11;
  }
}

final class Derived2 extends Base {
  public int $opt$noinline$getInt() {
    if (doThrow) { throw new Error(); }
    return 21;
  }
}

final class Derived3 extends Base {
}

class Derived4 extends Base {
  public int $opt$noinline$getInt() {
    if (doThrow) { throw new Error(); }
    return 41;
  }
}
