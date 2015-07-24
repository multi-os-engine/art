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
  private void foo() { throw new RuntimeException(); }

  /// CHECK-START: void Main.simpleIfAlwaysTrue(boolean) instruction_simplifier_after_types (before)
  /// CHECK:         If

  /// CHECK-START: void Main.simpleIfAlwaysTrue(boolean) instruction_simplifier_after_types (after)
  /// CHECK:         If
  /// CHECK-NOT:     If
  /// CHECK:         InvokeStaticOrDirect
  /// CHECK-NOT:     If
  /// CHECK:         InvokeStaticOrDirect
  /// CHECK-NOT:     If
  /// CHECK:         InvokeStaticOrDirect
  /// CHECK-NOT:     If
  /// CHECK:         InvokeStaticOrDirect
  /// CHECK-NOT:     If
  /// CHECK:         InvokeStaticOrDirect
  /// CHECK-NOT:     If
  /// CHECK:         InvokeStaticOrDirect
  public void simpleIfAlwaysTrue(boolean b) {
    int a = b ? 0 : 10;

    if (a < 11) {
      foo();
    }
    if (a <= 10) {
      foo();
    }
    if (a >= 0) {
      foo();
    }
    if (a > -1) {
      foo();
    }
    if (a > -42) {
      foo();
    }
    if (a != 12) {
      foo();
    }
  }

  /// CHECK-START: void Main.simpleIfAlwaysFalse(boolean) instruction_simplifier_after_types (before)
  /// CHECK:         If

  /// CHECK-START: void Main.simpleIfAlwaysFalse(boolean) instruction_simplifier_after_types (after)
  /// CHECK:         If
  /// CHECK-NOT:     If
  /// CHECK-NOT:     InvokeStaticOrDirect
  public void simpleIfAlwaysFalse(boolean b) {
    int a = b ? 0 : 10;

    if (a > 11) {
      foo();
    }
    if (a < -1) {
      foo();
    }
    if (a < -42) {
      foo();
    }
    if (a == 12) {
      foo();
    }
  }

  /// CHECK-START: void Main.simpleIfKeep(boolean) instruction_simplifier_after_types (before)
  /// CHECK:         If

  /// CHECK-START: void Main.simpleIfKeep(boolean) instruction_simplifier_after_types (after)
  /// CHECK:         If
  /// CHECK:         If
  /// CHECK:         InvokeStaticOrDirect
  public void simpleIfKeep(boolean b) {
    int a = b ? 0 : 10;

    if (a < 5) {
      foo();
    }
  }

  // Make sure we don't modify loops since the ranges don't deal nicely with them yet
  /// CHECK-START: void Main.simpleWhileAlwaysTrue(boolean) instruction_simplifier_after_types (before)
  /// CHECK:         If

  /// CHECK-START: void Main.simpleWhileAlwaysTrue(boolean) instruction_simplifier_after_types (after)
  /// CHECK:         If
  /// CHECK:         InvokeStaticOrDirect
  public void simpleWhileAlwaysTrue(boolean b) {
    int a = b ? 0 : 10;

    while (a < 25) {
      foo();
    }
  }

  // Make sure we don't modify loops since the ranges don't deal nicely with them yet
  /// CHECK-START: void Main.simpleWhileAlwaysFalse(boolean) instruction_simplifier_after_types (before)
  /// CHECK:         InvokeStaticOrDirect

  /// CHECK-START: void Main.simpleWhileAlwaysFalse(boolean) instruction_simplifier_after_types (after)
  /// CHECK:         If
  /// CHECK:     InvokeStaticOrDirect
  public void simpleWhileAlwaysFalse(boolean b) {
    int a = b ? 0 : 10;

    while (a > 20) {
      foo();
    }
  }

  public Main() {}
  public static void main(String[] args) {}
}

