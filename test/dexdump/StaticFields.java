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
 *
 * To update staticfields.dex:
 *
 * $ out/host/linux-x86/bin/jack --classpath \
 *   out/target/common/obj/JAVA_LIBRARIES/core-libart_intermediates/classes.jack \
 *   art/test/dexdump/StaticFields.java --output-dex /tmp
 * $ cp /tmp/classes.dex art/test/dexdump/staticfields.dex
 */

public class StaticFields {
  public static final byte test00_public_static_final_byte_42 = 42;
  public static final short test01_public_static_final_short_43 = 43;
  public static final char test02_public_static_final_char_X = 'X';
  public static final int test03_public_static_final_int_44 = 44;
  public static final long test04_public_static_final_long_45 = 45;
  public static final float test05_public_static_final_float_46_47 = 46.47f;
  public static final double test06_public_static_final_double_48_49 = 48.49;
  public static final String test07_public_static_final_string =
      "abc \\><\"'&\t\r\n";
  public static final Object test08_public_static_final_object_null = null;
  public static final boolean test09_public_static_final_boolean_true = true;

  private static final int test10_private_static_final_int_50 = 50;
  // DEX doesn't have trailing zero values.
  public static final int test99_empty_value = 0;
}
