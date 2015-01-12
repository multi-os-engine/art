/*
 * Copyright (C) 2014 The Android Open Source Project
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

  private volatile Object objectField;
  private volatile int intField;
  private volatile float floatField;
  private volatile long longField;
  private volatile double doubleField;
  private volatile byte byteField;
  private volatile boolean booleanField;
  private volatile char charField;
  private volatile short shortField;

  public static void $opt$no_inline$setObjectField(Main m) {
    m.objectField = null;
  }

  public static void $opt$no_inline$setIntField(Main m) {
    m.intField = 0;
  }

  public static void $opt$no_inline$setFloatField(Main m) {
    m.floatField = 0;
  }

  public static void $opt$no_inline$setLongField(Main m) {
    m.longField = 0;
  }

  public static void $opt$no_inline$setDoubleField(Main m) {
    m.doubleField = 0;
  }

  public static void $opt$no_inline$setByteField(Main m) {
    m.byteField = 0;
  }

  public static void $opt$no_inline$setBooleanField(Main m) {
    m.booleanField = false;
  }

  public static void $opt$no_inline$setCharField(Main m) {
    m.charField = 0;
  }

  public static void $opt$no_inline$setShortField(Main m) {
    m.shortField = 0;
  }

  public static Object $opt$no_inline$getObjectField(Main m) {
    return m.objectField;
  }

  public static int $opt$no_inline$getIntField(Main m) {
    return m.intField;
  }

  public static float $opt$no_inline$getFloatField(Main m) {
    return m.floatField;
  }

  public static long $opt$no_inline$getLongField(Main m) {
    return m.longField;
  }

  public static double $opt$no_inline$getDoubleField(Main m) {
    return m.doubleField;
  }

  public static byte $opt$no_inline$getByteField(Main m) {
    return m.byteField;
  }

  public static boolean $opt$no_inline$getBooleanField(Main m) {
    return m.booleanField;
  }

  public static char $opt$no_inline$getCharField(Main m) {
    return m.charField;
  }

  public static short $opt$no_inline$getShortField(Main m) {
    return m.shortField;
  }

  public static void main(String[] args) {
    int methodLine = 30;
    int thisLine = 103  ;
    try {
      $opt$no_inline$setObjectField(null);
      throw new RuntimeException("Failed to throw NullPointerException.");
    } catch (NullPointerException npe) {
      check(npe, thisLine += 2, methodLine, "$opt$no_inline$setObjectField");
    }
    try {
      $opt$no_inline$setIntField(null);
      throw new RuntimeException("Failed to throw NullPointerException.");
    } catch (NullPointerException npe) {
      check(npe, thisLine += 6, methodLine += 4, "$opt$no_inline$setIntField");
    }
    try {
      $opt$no_inline$setFloatField(null);
      throw new RuntimeException("Failed to throw NullPointerException.");
    } catch (NullPointerException npe) {
      check(npe, thisLine += 6, methodLine += 4, "$opt$no_inline$setFloatField");
    }
    try {
      $opt$no_inline$setLongField(null);
      throw new RuntimeException("Failed to throw NullPointerException.");
    } catch (NullPointerException npe) {
      check(npe, thisLine += 6, methodLine += 4, "$opt$no_inline$setLongField");
    }
    try {
      $opt$no_inline$setDoubleField(null);
      throw new RuntimeException("Failed to throw NullPointerException.");
    } catch (NullPointerException npe) {
      check(npe, thisLine += 6, methodLine += 4, "$opt$no_inline$setDoubleField");
    }
    try {
      $opt$no_inline$setByteField(null);
      throw new RuntimeException("Failed to throw NullPointerException.");
    } catch (NullPointerException npe) {
      check(npe, thisLine += 6, methodLine += 4, "$opt$no_inline$setByteField");
    }
    try {
      $opt$no_inline$setBooleanField(null);
      throw new RuntimeException("Failed to throw NullPointerException.");
    } catch (NullPointerException npe) {
      check(npe, thisLine += 6, methodLine += 4, "$opt$no_inline$setBooleanField");
    }
    try {
      $opt$no_inline$setCharField(null);
      throw new RuntimeException("Failed to throw NullPointerException.");
    } catch (NullPointerException npe) {
      check(npe, thisLine += 6, methodLine += 4, "$opt$no_inline$setCharField");
    }
    try {
      $opt$no_inline$setShortField(null);
      throw new RuntimeException("Failed to throw NullPointerException.");
    } catch (NullPointerException npe) {
      check(npe, thisLine += 6, methodLine += 4, "$opt$no_inline$setShortField");
    }
    try {
      $opt$no_inline$getObjectField(null);
      throw new RuntimeException("Failed to throw NullPointerException.");
    } catch (NullPointerException npe) {
      check(npe, thisLine += 6, methodLine += 4, "$opt$no_inline$getObjectField");
    }
    try {
      $opt$no_inline$getIntField(null);
      throw new RuntimeException("Failed to throw NullPointerException.");
    } catch (NullPointerException npe) {
      check(npe, thisLine += 6, methodLine += 4, "$opt$no_inline$getIntField");
    }
    try {
      $opt$no_inline$getFloatField(null);
      throw new RuntimeException("Failed to throw NullPointerException.");
    } catch (NullPointerException npe) {
      check(npe, thisLine += 6, methodLine += 4, "$opt$no_inline$getFloatField");
    }
    try {
      $opt$no_inline$getLongField(null);
      throw new RuntimeException("Failed to throw NullPointerException.");
    } catch (NullPointerException npe) {
      check(npe, thisLine += 6, methodLine += 4, "$opt$no_inline$getLongField");
    }
    try {
      $opt$no_inline$getDoubleField(null);
      throw new RuntimeException("Failed to throw NullPointerException.");
    } catch (NullPointerException npe) {
      check(npe, thisLine += 6, methodLine += 4, "$opt$no_inline$getDoubleField");
    }
    try {
      $opt$no_inline$getByteField(null);
      throw new RuntimeException("Failed to throw NullPointerException.");
    } catch (NullPointerException npe) {
      check(npe, thisLine += 6, methodLine += 4, "$opt$no_inline$getByteField");
    }
    try {
      $opt$no_inline$getBooleanField(null);
      throw new RuntimeException("Failed to throw NullPointerException.");
    } catch (NullPointerException npe) {
      check(npe, thisLine += 6, methodLine += 4, "$opt$no_inline$getBooleanField");
    }
    try {
      $opt$no_inline$getCharField(null);
      throw new RuntimeException("Failed to throw NullPointerException.");
    } catch (NullPointerException npe) {
      check(npe, thisLine += 6, methodLine += 4, "$opt$no_inline$getCharField");
    }
    try {
      $opt$no_inline$getShortField(null);
      throw new RuntimeException("Failed to throw NullPointerException.");
    } catch (NullPointerException npe) {
      check(npe, thisLine += 6, methodLine += 4, "$opt$no_inline$getShortField");
    }
  }

  static void check(NullPointerException npe, int mainLine, int medthodLine, String methodName) {
    System.out.println(methodName);
    StackTraceElement[] trace = npe.getStackTrace();
    checkElement(trace[0], "Main", methodName, "Main.java", medthodLine);
    checkElement(trace[1], "Main", "main", "Main.java", mainLine);
  }

  static void checkElement(StackTraceElement element,
                           String declaringClass, String methodName,
                           String fileName, int lineNumber) {
    assertEquals(declaringClass, element.getClassName());
    assertEquals(methodName, element.getMethodName());
    assertEquals(fileName, element.getFileName());
    assertEquals(lineNumber, element.getLineNumber());
  }

  static void assertEquals(Object expected, Object actual) {
    if (!expected.equals(actual)) {
      String msg = "Expected \"" + expected + "\" but got \"" + actual + "\"";
      throw new AssertionError(msg);
    }
  }

  static void assertEquals(int expected, int actual) {
    if (expected != actual) {
      throw new AssertionError("Expected " + expected + " got " + actual);
    }
  }

}
