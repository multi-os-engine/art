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

// Simple test for field accesses.
// TODO: float and double.
public class Main extends TestCase {
  public static void main(String[] args) {
    $opt$testInstanceFields();
    $opt$testStaticFields();
  }

  static void $opt$testInstanceFields() {
    AllFields fields = new AllFields();

    assertEquals(false, fields.iZ);
    assertEquals(0, fields.iB);
    assertEquals(0, fields.iC);
    assertEquals(0, fields.iI);
    assertEquals(0, fields.iJ);
    assertEquals(0, fields.iS);
    assertNull(fields.iObject);

    long longValue = -1122198787987987987L;
    fields.iZ = true;
    fields.iB = -2;
    fields.iC = 'c';
    fields.iI = 42;
    fields.iJ = longValue;
    fields.iS = 68;
    fields.iObject = fields;

    assertEquals(true, fields.iZ);
    assertEquals(-2, fields.iB);
    assertEquals('c', fields.iC);
    assertEquals(42, fields.iI);
    assertEquals(longValue, fields.iJ);
    assertEquals(68, fields.iS);
    assertEquals(fields, fields.iObject);
  }

  static void $opt$testStaticFields() {
    assertEquals(false, AllFields.sZ);
    assertEquals(0, AllFields.sB);
    assertEquals(0, AllFields.sC);
    assertEquals(0, AllFields.sI);
    assertEquals(0, AllFields.sJ);
    assertEquals(0, AllFields.sS);
    assertNull(AllFields.sObject);

    long longValue = -1122198787987987987L;
    Object fields = new AllFields();
    AllFields.sZ = true;
    AllFields.sB = -2;
    AllFields.sC = 'c';
    AllFields.sI = 42;
    AllFields.sJ = longValue;
    AllFields.sS = 68;
    AllFields.sObject = fields;

    assertEquals(true, AllFields.sZ);
    assertEquals(-2, AllFields.sB);
    assertEquals('c', AllFields.sC);
    assertEquals(42, AllFields.sI);
    assertEquals(longValue, AllFields.sJ);
    assertEquals(68, AllFields.sS);
    assertEquals(fields, AllFields.sObject);
  }

  static class AllFields {
    boolean iZ;
    byte iB;
    char iC;
    double iD;
    float iF;
    int iI;
    long iJ;
    short iS;
    Object iObject;

    static boolean sZ;
    static byte sB;
    static char sC;
    static double sD;
    static float sF;
    static int sI;
    static long sJ;
    static short sS;
    static Object sObject;
  }
}
