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

class Main {
  public static void main(String[] args) {
    testIntShiftRight();
    testIntShiftLeft();
    testIntUnsignedShiftRight();
    testLongShiftRight();
    testLongShiftLeft();
    testLongUnsignedShiftRight();
  }

  public static void testIntShiftLeft() {
    int a = myField;
    int b = myOtherField << a;
    if (b != -2147483648) {
      throw new Error("Expected -2147483648, got " + b);
    }
    if (a != 0xFFF) {
      throw new Error("Expected 0xFFF, got " + a);
    }
  }

  public static void testIntShiftRight() {
    int a = myField;
    int b = myOtherField >> a;
    if (b != 0) {
      throw new Error("Expected 0, got " + b);
    }
    if (a != 0xFFF) {
      throw new Error("Expected 0xFFF, got " + a);
    }
  }
  
  public static void testIntUnsignedShiftRight() {
    int a = myField;
    int b = myOtherField >>> a;
    if (b != 0) {
      throw new Error("Expected 0, got " + b);
    }
    if (a != 0xFFF) {
      throw new Error("Expected 0xFFF, got " + a);
    }
  }

  public static void testLongShiftLeft() {
    long a = myLongField;
    long b = myOtherLongField << a;
    if (b != -9223372036854775808L) {
      throw new Error("Expected -9223372036854775808, got " + b);
    }
    // The int conversion will be GVN'ed with the one required
    // by Java specification of long shift left.
    if ((int)a != 0xFFF) {
      throw new Error("Expected 0xFFF, got " + a);
    }
  }

  public static void testLongShiftRight() {
    long a = myLongField;
    long b = myOtherLongField >> a;
    if (b != 0) {
      throw new Error("Expected 0, got " + b);
    }
    // The int conversion will be GVN'ed with the one required
    // by Java specification of long shift right.
    if ((int)a != 0xFFF) {
      throw new Error("Expected 0xFFF, got " + a);
    }
  }

  public static void testLongUnsignedShiftRight() {
    long a = myLongField;
    long b = myOtherLongField >>> a;
    if (b != 0) {
      throw new Error("Expected 0, got " + b);
    }
    // The int conversion will be GVN'ed with the one required
    // by Java specification of long shift right.
    if ((int)a != 0xFFF) {
      throw new Error("Expected 0xFFF, got " + a);
    }
  }

  static int myField = 0xFFF;
  static int myOtherField = 0x1;

  static long myLongField = 0xFFF;
  static long myOtherLongField = 0x1;
}
