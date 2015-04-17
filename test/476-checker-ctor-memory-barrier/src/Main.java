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


class ClassWithoutFinals {
  // CHECK-START: void ClassWithoutFinals.<init>() register (after)
  // CHECK-NOT: MemoryBarrier {{StoreStore}}
  public ClassWithoutFinals() {}
}

class ClassWithFinals {
  public final int x;

  // CHECK-START: void ClassWithFinals.<init>(boolean) register (after)
  // CHECK:     MemoryBarrier {{StoreStore}}
  // CHECK-NOT: {{.*}}
  // CHECK:     ReturnVoid
  public ClassWithFinals(boolean cond) {
    x = 0;
    if (cond) {
      // avoid inlining
      throw new RuntimeException();
    }
  }

  // CHECK-START: void ClassWithFinals.<init>() register (after)
  // CHECK:     MemoryBarrier {{StoreStore}}
  // CHECK-NOT: {{.*}}
  // CHECK:     ReturnVoid
  public ClassWithFinals() {
    x = 0;
  }
}

class InheritFromClassWithFinals extends ClassWithFinals {
  // CHECK-START: void InheritFromClassWithFinals.<init>() register (after)
  // CHECK:     MemoryBarrier {{StoreStore}}
  // CHECK-NOT: {{.*}}
  // CHECK:     ReturnVoid

  // CHECK-START: void InheritFromClassWithFinals.<init>() register (after)
  // CHECK-NOT: InvokeStaticOrDirect
  public InheritFromClassWithFinals() {
    // Should inline the super constructor.
  }

  // CHECK-START: void InheritFromClassWithFinals.<init>(boolean) register (after)
  // CHECK:     InvokeStaticOrDirect

  // CHECK-START: void InheritFromClassWithFinals.<init>(boolean) register (after)
  // CHECK-NOT: MemoryBarrier {{StoreStore}}
  public InheritFromClassWithFinals(boolean cond) {
    super(cond);
    // should not inline the super constructor
  }
}

public class Main {

  // CHECK-START: ClassWithFinals Main.noInlineNoConstructorBarrier() register (after)
  // CHECK:     InvokeStaticOrDirect

  // CHECK-START: ClassWithFinals Main.noInlineNoConstructorBarrier() register (after)
  // CHECK-NOT: MemoryBarrier {{StoreStore}}
  public static ClassWithFinals noInlineNoConstructorBarrier() {
    return new ClassWithFinals(false);
  }

  // CHECK-START: ClassWithFinals Main.inlineConstructorBarrier() register (after)
  // CHECK:     MemoryBarrier {{StoreStore}}
  // CHECK-NOT: {{.*}}
  // CHECK:     Return

  // CHECK-START: ClassWithFinals Main.inlineConstructorBarrier() register (after)
  // CHECK-NOT: InvokeStaticOrDirect
  public static ClassWithFinals inlineConstructorBarrier() {
    return new ClassWithFinals();
  }

  // CHECK-START: InheritFromClassWithFinals Main.doubleInlineConstructorBarrier() register (after)
  // CHECK:     MemoryBarrier {{StoreStore}}
  // CHECK-NOT: {{.*}}
  // CHECK:     Return

  // CHECK-START: InheritFromClassWithFinals Main.doubleInlineConstructorBarrier() register (after)
  // CHECK-NOT: InvokeStaticOrDirect
  public static InheritFromClassWithFinals doubleInlineConstructorBarrier() {
    return new InheritFromClassWithFinals();
  }

  public static void main(String[] args) {  }
}
