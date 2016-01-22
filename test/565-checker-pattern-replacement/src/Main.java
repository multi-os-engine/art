/*
 * Copyright (C) 2016 The Android Open Source Project
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
    /// CHECK-START: void Main.nop() inliner (before)
    /// CHECK:                          InvokeStaticOrDirect

    /// CHECK-START: void Main.nop() inliner (after)
    /// CHECK-NOT:                      InvokeStaticOrDirect

    public static void nop() {
      Second.staticNop(11);
    }

    /// CHECK-START: void Main.nop(Second) inliner (before)
    /// CHECK:                          InvokeVirtual

    /// CHECK-START: void Main.nop(Second) inliner (after)
    /// CHECK-NOT:                      InvokeVirtual

    public static void nop(Second s) {
      s.nop();
    }

    /// CHECK-START: java.lang.Object Main.staticReturnArg2(java.lang.String) inliner (before)
    /// CHECK-DAG:  <<Value:l\d+>>      ParameterValue
    /// CHECK-DAG:  <<Ignored:i\d+>>    IntConstant 77
    /// CHECK-DAG:  <<ClinitCk:l\d+>>   ClinitCheck
    /// CHECK:      <<Invoke:l\d+>>     InvokeStaticOrDirect [<<Ignored>>,<<Value>>,<<ClinitCk>>]
    /// CHECK:                          Return [<<Invoke>>]

    /// CHECK-START: java.lang.Object Main.staticReturnArg2(java.lang.String) inliner (after)
    /// CHECK-DAG:  <<Value:l\d+>>      ParameterValue
    /// CHECK:                          Return [<<Value>>]

    /// CHECK-START: java.lang.Object Main.staticReturnArg2(java.lang.String) inliner (after)
    /// CHECK-NOT:                      InvokeStaticOrDirect

    public static Object staticReturnArg2(String value) {
      return Second.staticReturnArg2(77, value);
    }

    /// CHECK-START: long Main.returnArg1(Second, long) inliner (before)
    /// CHECK-DAG:  <<Second:l\d+>>     ParameterValue
    /// CHECK-DAG:  <<Value:j\d+>>      ParameterValue
    /// CHECK:      <<NullCk:l\d+>>     NullCheck [<<Second>>]
    /// CHECK:      <<Invoke:j\d+>>     InvokeVirtual [<<NullCk>>,<<Value>>]
    /// CHECK:                          Return [<<Invoke>>]

    /// CHECK-START: long Main.returnArg1(Second, long) inliner (after)
    /// CHECK-DAG:  <<Value:j\d+>>      ParameterValue
    /// CHECK:                          Return [<<Value>>]

    /// CHECK-START: long Main.returnArg1(Second, long) inliner (after)
    /// CHECK-NOT:                      InvokeVirtual

    public static long returnArg1(Second s, long value) {
      return s.returnArg1(value);
    }

    /// CHECK-START: int Main.staticReturn9() inliner (before)
    /// CHECK:      <<Invoke:i\d+>>     InvokeStaticOrDirect

    /// CHECK-START: int Main.staticReturn9() inliner (before)
    /// CHECK-NOT:                      IntConstant 9

    /// CHECK-START: int Main.staticReturn9() inliner (after)
    /// CHECK:      <<Const9:i\d+>>     IntConstant 9
    /// CHECK:                          Return [<<Const9>>]

    /// CHECK-START: int Main.staticReturn9() inliner (after)
    /// CHECK-NOT:                      InvokeStaticOrDirect

    public static int staticReturn9() {
      return Second.staticReturn9();
    }

    /// CHECK-START: int Main.return7(Second) inliner (before)
    /// CHECK:      <<Invoke:i\d+>>     InvokeVirtual

    /// CHECK-START: int Main.return7(Second) inliner (before)
    /// CHECK-NOT:                      IntConstant 7

    /// CHECK-START: int Main.return7(Second) inliner (after)
    /// CHECK:      <<Const7:i\d+>>     IntConstant 7
    /// CHECK:                          Return [<<Const7>>]

    /// CHECK-START: int Main.return7(Second) inliner (after)
    /// CHECK-NOT:                      InvokeVirtual

    public static int return7(Second s) {
      return s.return7(null);
    }

    /// CHECK-START: java.lang.String Main.staticReturnNull() inliner (before)
    /// CHECK:      <<Invoke:l\d+>>     InvokeStaticOrDirect

    /// CHECK-START: java.lang.String Main.staticReturnNull() inliner (before)
    /// CHECK-NOT:                      NullConstant

    /// CHECK-START: java.lang.String Main.staticReturnNull() inliner (after)
    /// CHECK:      <<Null:l\d+>>       NullConstant
    /// CHECK:                          Return [<<Null>>]

    /// CHECK-START: java.lang.String Main.staticReturnNull() inliner (after)
    /// CHECK-NOT:                      InvokeStaticOrDirect

    public static String staticReturnNull() {
      return Second.staticReturnNull();
    }

    /// CHECK-START: java.lang.Object Main.returnNull(Second) inliner (before)
    /// CHECK:      <<Invoke:l\d+>>     InvokeVirtual

    /// CHECK-START: java.lang.Object Main.returnNull(Second) inliner (before)
    /// CHECK-NOT:                      NullConstant

    /// CHECK-START: java.lang.Object Main.returnNull(Second) inliner (after)
    /// CHECK:      <<Null:l\d+>>       NullConstant
    /// CHECK:                          Return [<<Null>>]

    /// CHECK-START: java.lang.Object Main.returnNull(Second) inliner (after)
    /// CHECK-NOT:                      InvokeVirtual

    public static Object returnNull(Second s) {
      return s.returnNull();
    }

    /// CHECK-START: int Main.getInt(Second) inliner (before)
    /// CHECK:      <<Invoke:i\d+>>     InvokeVirtual

    /// CHECK-START: int Main.getInt(Second) inliner (after)
    /// CHECK:      <<IGet:i\d+>>       InstanceFieldGet

    /// CHECK-START: int Main.getInt(Second) inliner (after)
    /// CHECK-NOT:                      InvokeVirtual

    public static int getInt(Second s) {
      return s.getInstanceIntField();
    }

    /// CHECK-START: double Main.getDouble(Second) inliner (before)
    /// CHECK:      <<Invoke:d\d+>>     InvokeVirtual

    /// CHECK-START: double Main.getDouble(Second) inliner (after)
    /// CHECK:      <<IGet:d\d+>>       InstanceFieldGet

    /// CHECK-START: double Main.getDouble(Second) inliner (after)
    /// CHECK-NOT:                      InvokeVirtual

    public static double getDouble(Second s) {
      return s.getInstanceDoubleField(22);
    }

    /// CHECK-START: java.lang.Object Main.getObject(Second) inliner (before)
    /// CHECK:      <<Invoke:l\d+>>     InvokeVirtual

    /// CHECK-START: java.lang.Object Main.getObject(Second) inliner (after)
    /// CHECK:      <<IGet:l\d+>>       InstanceFieldGet

    /// CHECK-START: java.lang.Object Main.getObject(Second) inliner (after)
    /// CHECK-NOT:                      InvokeVirtual

    public static Object getObject(Second s) {
      return s.getInstanceObjectField(-1L);
    }

    /// CHECK-START: java.lang.String Main.getString(Second) inliner (before)
    /// CHECK:      <<Invoke:l\d+>>     InvokeVirtual

    /// CHECK-START: java.lang.String Main.getString(Second) inliner (after)
    /// CHECK:      <<IGet:l\d+>>       InstanceFieldGet

    /// CHECK-START: java.lang.String Main.getString(Second) inliner (after)
    /// CHECK-NOT:                      InvokeVirtual

    public static String getString(Second s) {
      return s.getInstanceStringField(null, "whatever", 1234L);
    }

    /// CHECK-START: int Main.staticGetInt(Second) inliner (before)
    /// CHECK:      <<Invoke:i\d+>>     InvokeStaticOrDirect

    /// CHECK-START: int Main.staticGetInt(Second) inliner (after)
    /// CHECK:      <<Invoke:i\d+>>     InvokeStaticOrDirect

    /// CHECK-START: int Main.staticGetInt(Second) inliner (after)
    /// CHECK-NOT:                      InstanceFieldGet

    public static int staticGetInt(Second s) {
      return Second.staticGetInstanceIntField(s);
    }

    /// CHECK-START: double Main.getDoubleFromParam(Second) inliner (before)
    /// CHECK:      <<Invoke:d\d+>>     InvokeVirtual

    /// CHECK-START: double Main.getDoubleFromParam(Second) inliner (after)
    /// CHECK:      <<Invoke:d\d+>>     InvokeVirtual

    /// CHECK-START: double Main.getDoubleFromParam(Second) inliner (after)
    /// CHECK-NOT:                      InstanceFieldGet

    public static double getDoubleFromParam(Second s) {
      return s.getInstanceDoubleFieldFromParam(s);
    }

    /// CHECK-START: int Main.getStaticInt(Second) inliner (before)
    /// CHECK:      <<Invoke:i\d+>>     InvokeVirtual

    /// CHECK-START: int Main.getStaticInt(Second) inliner (after)
    /// CHECK:      <<Invoke:i\d+>>     InvokeVirtual

    /// CHECK-START: int Main.getStaticInt(Second) inliner (after)
    /// CHECK-NOT:                      InstanceFieldGet
    /// CHECK-NOT:                      StaticFieldGet

    public static int getStaticInt(Second s) {
      return s.getStaticIntField();
    }

    /// CHECK-START: long Main.setLong(Second, long) inliner (before)
    /// CHECK:                          InvokeVirtual

    /// CHECK-START: long Main.setLong(Second, long) inliner (after)
    /// CHECK:                          InstanceFieldSet

    /// CHECK-START: long Main.setLong(Second, long) inliner (after)
    /// CHECK-NOT:                      InvokeVirtual

    public static long setLong(Second s, long value) {
      s.setInstanceLongField(-1, value);
      return s.instanceLongField;
    }

    /// CHECK-START: long Main.staticSetLong(Second, long) inliner (before)
    /// CHECK:                          InvokeStaticOrDirect

    /// CHECK-START: long Main.staticSetLong(Second, long) inliner (after)
    /// CHECK:                          InvokeStaticOrDirect

    /// CHECK-START: long Main.staticSetLong(Second, long) inliner (after)
    /// CHECK-NOT:                      InstanceFieldSet

    public static long staticSetLong(Second s, long value) {
      Second.staticSetInstanceLongField(s, value);
      return s.instanceLongField;
    }

    /// CHECK-START: long Main.setLongThroughParam(Second, long) inliner (before)
    /// CHECK:                          InvokeVirtual

    /// CHECK-START: long Main.setLongThroughParam(Second, long) inliner (after)
    /// CHECK:                          InvokeVirtual

    /// CHECK-START: long Main.setLongThroughParam(Second, long) inliner (after)
    /// CHECK-NOT:                      InstanceFieldSet

    public static long setLongThroughParam(Second s, long value) {
      s.setInstanceLongFieldThroughParam(s, value);
      return s.instanceLongField;
    }

    /// CHECK-START: float Main.setStaticFloat(Second, float) inliner (before)
    /// CHECK:                          InvokeVirtual

    /// CHECK-START: float Main.setStaticFloat(Second, float) inliner (after)
    /// CHECK:                          InvokeVirtual

    /// CHECK-START: float Main.setStaticFloat(Second, float) inliner (after)
    /// CHECK-NOT:                      InstanceFieldSet
    /// CHECK-NOT:                      StaticFieldSet

    public static float setStaticFloat(Second s, float value) {
      s.setStaticFloatField(value);
      return s.staticFloatField;
    }

    public static void main(String[] args) throws Exception {
      Second s = new Second();

      // Replaced NOP pattern.
      nop();
      nop(s);
      // Replaced "return arg" pattern.
      assertEquals("arbitrary string", staticReturnArg2("arbitrary string"));
      assertEquals(4321L, returnArg1(s, 4321L));
      // Replaced "return const" pattern.
      assertEquals(9, staticReturn9());
      assertEquals(7, return7(s));
      assertEquals(null, staticReturnNull());
      assertEquals(null, returnNull(s));
      // Replaced IGET pattern.
      assertEquals(42, getInt(s));
      assertEquals(-42.0, getDouble(s));
      assertEquals(null, getObject(s));
      assertEquals("dummy", getString(s));
      // Not replaced IGET pattern.
      assertEquals(42, staticGetInt(s));
      assertEquals(-42.0, getDoubleFromParam(s));
      // SGET.
      assertEquals(4242, getStaticInt(s));
      // Replaced IPUT pattern.
      assertEquals(111L, setLong(s, 111L));
      // Not replaced IPUT pattern.
      assertEquals(222L, staticSetLong(s, 222L));
      assertEquals(333L, setLongThroughParam(s, 333L));
      // SPUT.
      assertEquals(-11.5f, setStaticFloat(s, -11.5f));
    }

    private static void assertEquals(int expected, int actual) {
      if (expected != actual) {
        throw new AssertionError("Wrong result: " + expected + " != " + actual);
      }
    }

    private static void assertEquals(double expected, double actual) {
      if (expected != actual) {
        throw new AssertionError("Wrong result: " + expected + " != " + actual);
      }
    }

    private static void assertEquals(Object expected, Object actual) {
      if (expected != actual && (expected == null || !expected.equals(actual))) {
        throw new AssertionError("Wrong result: " + expected + " != " + actual);
      }
    }
}
