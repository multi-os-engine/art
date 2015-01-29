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

    public enum ShiftType { SHL, SHR, USHR }

    public static void expectEquals(long expected, long result) {
        if (expected != result) {
            throw new Error("Expected: " + expected + ", found: " + result);
        }
    }

    public static void expectEquals(int expected, int result) {
        if (expected != result) {
            throw new Error("Expected: " + expected + ", found: " + result);
        }
    }

    public static int opshift_int(char op, ShiftType shift_type, int a, int b, int c) {
      int shift_res = 0;
      switch (shift_type) {
        case SHL:  shift_res = b << c;  break;
        case SHR:  shift_res = b >> c;  break;
        case USHR: shift_res = b >>> c; break;
      }
      int res = 0;
      switch (op) {
        case '+': res = a + shift_res; break;
        case '-': res = a - shift_res; break;
        case '&': res = a & shift_res; break;
        case '|': res = a | shift_res; break;
        case '^': res = a ^ shift_res; break;
      }
      return res;
    }

    static int[] int_vals = { 0x1, 0x12345678, 0x87654321, 0x1c1c1c1c, 0x31415926, 0x27182818};
    static long[] long_vals =
        { 0x1L, 0x123456789abcL, 0x87654321L, 0x1c1c1c1cL, 0x31415926535L, 0x2718281828L};

    public static void test_shift_int() {
      for (int index_a = 0; index_a < int_vals.length; index_a++) {
        int a  = int_vals[index_a];
        for (int index_b = 0; index_b < int_vals.length; index_b++) {
          int b  = int_vals[index_b];
          expectEquals(opshift_int('+', ShiftType.SHL,  a, b, -33), a + (b <<  -33));
          expectEquals(opshift_int('-', ShiftType.SHR,  a, b, -33), a - (b >>  -33));
          expectEquals(opshift_int('&', ShiftType.USHR, a, b, -33), a & (b >>> -33));
          expectEquals(opshift_int('|', ShiftType.SHL,  a, b, -33), a | (b <<  -33));
          expectEquals(opshift_int('^', ShiftType.SHR,  a, b, -33), a ^ (b >>  -33));
          expectEquals(opshift_int('+', ShiftType.USHR, a, b, -32), a + (b >>> -32));
          expectEquals(opshift_int('-', ShiftType.SHL,  a, b, -32), a - (b <<  -32));
          expectEquals(opshift_int('&', ShiftType.SHR,  a, b, -32), a & (b >>  -32));
          expectEquals(opshift_int('|', ShiftType.USHR, a, b, -32), a | (b >>> -32));
          expectEquals(opshift_int('^', ShiftType.SHL,  a, b, -32), a ^ (b <<  -32));
          expectEquals(opshift_int('+', ShiftType.SHR,  a, b, -31), a + (b >>  -31));
          expectEquals(opshift_int('-', ShiftType.USHR, a, b, -31), a - (b >>> -31));
          expectEquals(opshift_int('&', ShiftType.SHL,  a, b, -31), a & (b <<  -31));
          expectEquals(opshift_int('|', ShiftType.SHR,  a, b, -31), a | (b >>  -31));
          expectEquals(opshift_int('^', ShiftType.USHR, a, b, -31), a ^ (b >>> -31));
          expectEquals(opshift_int('+', ShiftType.SHL,  a, b, -10), a + (b <<  -10));
          expectEquals(opshift_int('-', ShiftType.SHR,  a, b, -10), a - (b >>  -10));
          expectEquals(opshift_int('&', ShiftType.USHR, a, b, -10), a & (b >>> -10));
          expectEquals(opshift_int('|', ShiftType.SHL,  a, b, -10), a | (b <<  -10));
          expectEquals(opshift_int('^', ShiftType.SHR,  a, b, -10), a ^ (b >>  -10));
          expectEquals(opshift_int('+', ShiftType.USHR, a, b,   0), a + (b >>>   0));
          expectEquals(opshift_int('-', ShiftType.SHL,  a, b,   0), a - (b <<    0));
          expectEquals(opshift_int('&', ShiftType.SHR,  a, b,   0), a & (b >>    0));
          expectEquals(opshift_int('|', ShiftType.USHR, a, b,   0), a | (b >>>   0));
          expectEquals(opshift_int('^', ShiftType.SHL,  a, b,   0), a ^ (b <<    0));
          expectEquals(opshift_int('+', ShiftType.SHR,  a, b,  10), a + (b >>   10));
          expectEquals(opshift_int('-', ShiftType.USHR, a, b,  10), a - (b >>>  10));
          expectEquals(opshift_int('&', ShiftType.SHL,  a, b,  10), a & (b <<   10));
          expectEquals(opshift_int('|', ShiftType.SHR,  a, b,  10), a | (b >>   10));
          expectEquals(opshift_int('^', ShiftType.USHR, a, b,  10), a ^ (b >>>  10));
          expectEquals(opshift_int('+', ShiftType.SHL,  a, b,  31), a + (b <<   31));
          expectEquals(opshift_int('-', ShiftType.SHR,  a, b,  31), a - (b >>   31));
          expectEquals(opshift_int('&', ShiftType.USHR, a, b,  31), a & (b >>>  31));
          expectEquals(opshift_int('|', ShiftType.SHL,  a, b,  31), a | (b <<   31));
          expectEquals(opshift_int('^', ShiftType.SHR,  a, b,  31), a ^ (b >>   31));
          expectEquals(opshift_int('+', ShiftType.USHR, a, b,  32), a + (b >>>  32));
          expectEquals(opshift_int('-', ShiftType.SHL,  a, b,  32), a - (b <<   32));
          expectEquals(opshift_int('&', ShiftType.SHR,  a, b,  32), a & (b >>   32));
          expectEquals(opshift_int('|', ShiftType.USHR, a, b,  32), a | (b >>>  32));
          expectEquals(opshift_int('^', ShiftType.SHL,  a, b,  32), a ^ (b <<   32));
          expectEquals(opshift_int('+', ShiftType.SHR,  a, b,  33), a + (b >>   33));
          expectEquals(opshift_int('-', ShiftType.USHR, a, b,  33), a - (b >>>  33));
          expectEquals(opshift_int('&', ShiftType.SHL,  a, b,  33), a & (b <<   33));
          expectEquals(opshift_int('|', ShiftType.SHR,  a, b,  33), a | (b >>   33));
          expectEquals(opshift_int('^', ShiftType.USHR, a, b,  33), a ^ (b >>>  33));
        }
      }
    }

    public static long opshift_long(char op, ShiftType shift_type, long a, long b, long c) {
      long shift_res = 0;
      switch (shift_type) {
        case SHL:  shift_res = b << c;  break;
        case SHR:  shift_res = b >> c;  break;
        case USHR: shift_res = b >>> c; break;
      }
      long res = 0;
      switch (op) {
        case '+': res = a + shift_res; break;
        case '-': res = a - shift_res; break;
        case '&': res = a & shift_res; break;
        case '|': res = a | shift_res; break;
        case '^': res = a ^ shift_res; break;
      }
      return res;
    }

    public static void test_shift_long() {
      for (int index_a = 0; index_a < long_vals.length; index_a++) {
        long a  = long_vals[index_a];
        for (int index_b = 0; index_b < long_vals.length; index_b++) {
          long b  = long_vals[index_b];
          expectEquals(opshift_long('+', ShiftType.SHL,  a, b, -65), a + (b <<  -65));
          expectEquals(opshift_long('-', ShiftType.SHR,  a, b, -65), a - (b >>  -65));
          expectEquals(opshift_long('&', ShiftType.USHR, a, b, -65), a & (b >>> -65));
          expectEquals(opshift_long('|', ShiftType.SHL,  a, b, -65), a | (b <<  -65));
          expectEquals(opshift_long('^', ShiftType.SHR,  a, b, -65), a ^ (b >>  -65));
          expectEquals(opshift_long('+', ShiftType.USHR, a, b, -64), a + (b >>> -64));
          expectEquals(opshift_long('-', ShiftType.SHL,  a, b, -64), a - (b <<  -64));
          expectEquals(opshift_long('&', ShiftType.SHR,  a, b, -64), a & (b >>  -64));
          expectEquals(opshift_long('|', ShiftType.USHR, a, b, -64), a | (b >>> -64));
          expectEquals(opshift_long('^', ShiftType.SHL,  a, b, -64), a ^ (b <<  -64));
          expectEquals(opshift_long('+', ShiftType.SHR,  a, b, -63), a + (b >>  -63));
          expectEquals(opshift_long('-', ShiftType.USHR, a, b, -63), a - (b >>> -63));
          expectEquals(opshift_long('&', ShiftType.SHL,  a, b, -63), a & (b <<  -63));
          expectEquals(opshift_long('|', ShiftType.SHR,  a, b, -63), a | (b >>  -63));
          expectEquals(opshift_long('^', ShiftType.USHR, a, b, -63), a ^ (b >>> -63));
          expectEquals(opshift_long('+', ShiftType.SHL,  a, b, -50), a + (b <<  -50));
          expectEquals(opshift_long('-', ShiftType.SHR,  a, b, -50), a - (b >>  -50));
          expectEquals(opshift_long('&', ShiftType.USHR, a, b, -50), a & (b >>> -50));
          expectEquals(opshift_long('|', ShiftType.SHL,  a, b, -50), a | (b <<  -50));
          expectEquals(opshift_long('^', ShiftType.SHR,  a, b, -50), a ^ (b >>  -50));
          expectEquals(opshift_long('+', ShiftType.USHR, a, b, -33), a + (b >>> -33));
          expectEquals(opshift_long('-', ShiftType.SHL,  a, b, -33), a - (b <<  -33));
          expectEquals(opshift_long('&', ShiftType.SHR,  a, b, -33), a & (b >>  -33));
          expectEquals(opshift_long('|', ShiftType.USHR, a, b, -33), a | (b >>> -33));
          expectEquals(opshift_long('^', ShiftType.SHL,  a, b, -33), a ^ (b <<  -33));
          expectEquals(opshift_long('+', ShiftType.SHR,  a, b, -32), a + (b >>  -32));
          expectEquals(opshift_long('-', ShiftType.USHR, a, b, -32), a - (b >>> -32));
          expectEquals(opshift_long('&', ShiftType.SHL,  a, b, -32), a & (b <<  -32));
          expectEquals(opshift_long('|', ShiftType.SHR,  a, b, -32), a | (b >>  -32));
          expectEquals(opshift_long('^', ShiftType.USHR, a, b, -32), a ^ (b >>> -32));
          expectEquals(opshift_long('+', ShiftType.SHL,  a, b, -31), a + (b <<  -31));
          expectEquals(opshift_long('-', ShiftType.SHR,  a, b, -31), a - (b >>  -31));
          expectEquals(opshift_long('&', ShiftType.USHR, a, b, -31), a & (b >>> -31));
          expectEquals(opshift_long('|', ShiftType.SHL,  a, b, -31), a | (b <<  -31));
          expectEquals(opshift_long('^', ShiftType.SHR,  a, b, -31), a ^ (b >>  -31));
          expectEquals(opshift_long('+', ShiftType.USHR, a, b, -10), a + (b >>> -10));
          expectEquals(opshift_long('-', ShiftType.SHL,  a, b, -10), a - (b <<  -10));
          expectEquals(opshift_long('&', ShiftType.SHR,  a, b, -10), a & (b >>  -10));
          expectEquals(opshift_long('|', ShiftType.USHR, a, b, -10), a | (b >>> -10));
          expectEquals(opshift_long('^', ShiftType.SHL,  a, b, -10), a ^ (b <<  -10));
          expectEquals(opshift_long('+', ShiftType.SHR,  a, b,   0), a + (b >>    0));
          expectEquals(opshift_long('-', ShiftType.USHR, a, b,   0), a - (b >>>   0));
          expectEquals(opshift_long('&', ShiftType.SHL,  a, b,   0), a & (b <<    0));
          expectEquals(opshift_long('|', ShiftType.SHR,  a, b,   0), a | (b >>    0));
          expectEquals(opshift_long('^', ShiftType.USHR, a, b,   0), a ^ (b >>>   0));
          expectEquals(opshift_long('+', ShiftType.SHL,  a, b,  10), a + (b <<   10));
          expectEquals(opshift_long('-', ShiftType.SHR,  a, b,  10), a - (b >>   10));
          expectEquals(opshift_long('&', ShiftType.USHR, a, b,  10), a & (b >>>  10));
          expectEquals(opshift_long('|', ShiftType.SHL,  a, b,  10), a | (b <<   10));
          expectEquals(opshift_long('^', ShiftType.SHR,  a, b,  10), a ^ (b >>   10));
          expectEquals(opshift_long('+', ShiftType.USHR, a, b,  31), a + (b >>>  31));
          expectEquals(opshift_long('-', ShiftType.SHL,  a, b,  31), a - (b <<   31));
          expectEquals(opshift_long('&', ShiftType.SHR,  a, b,  31), a & (b >>   31));
          expectEquals(opshift_long('|', ShiftType.USHR, a, b,  31), a | (b >>>  31));
          expectEquals(opshift_long('^', ShiftType.SHL,  a, b,  31), a ^ (b <<   31));
          expectEquals(opshift_long('+', ShiftType.SHR,  a, b,  32), a + (b >>   32));
          expectEquals(opshift_long('-', ShiftType.USHR, a, b,  32), a - (b >>>  32));
          expectEquals(opshift_long('&', ShiftType.SHL,  a, b,  32), a & (b <<   32));
          expectEquals(opshift_long('|', ShiftType.SHR,  a, b,  32), a | (b >>   32));
          expectEquals(opshift_long('^', ShiftType.USHR, a, b,  32), a ^ (b >>>  32));
          expectEquals(opshift_long('+', ShiftType.SHL,  a, b,  33), a + (b <<   33));
          expectEquals(opshift_long('-', ShiftType.SHR,  a, b,  33), a - (b >>   33));
          expectEquals(opshift_long('&', ShiftType.USHR, a, b,  33), a & (b >>>  33));
          expectEquals(opshift_long('|', ShiftType.SHL,  a, b,  33), a | (b <<   33));
          expectEquals(opshift_long('^', ShiftType.SHR,  a, b,  33), a ^ (b >>   33));
          expectEquals(opshift_long('+', ShiftType.USHR, a, b,  50), a + (b >>>  50));
          expectEquals(opshift_long('-', ShiftType.SHL,  a, b,  50), a - (b <<   50));
          expectEquals(opshift_long('&', ShiftType.SHR,  a, b,  50), a & (b >>   50));
          expectEquals(opshift_long('|', ShiftType.USHR, a, b,  50), a | (b >>>  50));
          expectEquals(opshift_long('^', ShiftType.SHL,  a, b,  50), a ^ (b <<   50));
          expectEquals(opshift_long('+', ShiftType.SHR,  a, b,  63), a + (b >>   63));
          expectEquals(opshift_long('-', ShiftType.USHR, a, b,  63), a - (b >>>  63));
          expectEquals(opshift_long('&', ShiftType.SHL,  a, b,  63), a & (b <<   63));
          expectEquals(opshift_long('|', ShiftType.SHR,  a, b,  63), a | (b >>   63));
          expectEquals(opshift_long('^', ShiftType.USHR, a, b,  63), a ^ (b >>>  63));
          expectEquals(opshift_long('+', ShiftType.SHL,  a, b,  64), a + (b <<   64));
          expectEquals(opshift_long('-', ShiftType.SHR,  a, b,  64), a - (b >>   64));
          expectEquals(opshift_long('&', ShiftType.USHR, a, b,  64), a & (b >>>  64));
          expectEquals(opshift_long('|', ShiftType.SHL,  a, b,  64), a | (b <<   64));
          expectEquals(opshift_long('^', ShiftType.SHR,  a, b,  64), a ^ (b >>   64));
          expectEquals(opshift_long('+', ShiftType.USHR, a, b,  65), a + (b >>>  65));
          expectEquals(opshift_long('-', ShiftType.SHL,  a, b,  65), a - (b <<   65));
          expectEquals(opshift_long('&', ShiftType.SHR,  a, b,  65), a & (b >>   65));
          expectEquals(opshift_long('|', ShiftType.USHR, a, b,  65), a | (b >>>  65));
          expectEquals(opshift_long('^', ShiftType.SHL,  a, b,  65), a ^ (b <<   65));
        }
      }
    }

    public static int   long_to_int   (long a)  { return   (int)a; }
    public static short long_to_short (long a)  { return (short)a; }
    public static char  long_to_char  (long a)  { return  (char)a; }
    public static byte  long_to_byte  (long a)  { return  (byte)a; }
    public static long  int_to_long   (int a)   { return  (long)a; }
    public static short int_to_short  (int a)   { return (short)a; }
    public static char  int_to_char   (int a)   { return  (char)a; }
    public static byte  int_to_byte   (int a)   { return  (byte)a; }
    public static long  short_to_long (short a) { return  (long)a; }
    public static int   short_to_int  (short a) { return   (int)a; }
    public static char  short_to_char (short a) { return  (char)a; }
    public static byte  short_to_byte (short a) { return  (byte)a; }
    public static long  char_to_long  (char a)  { return  (long)a; }
    public static int   char_to_int   (char a)  { return   (int)a; }
    public static short char_to_short (char a)  { return (short)a; }
    public static byte  char_to_byte  (char a)  { return  (byte)a; }

    public static void test_extend() {
      for (int index_a = 0; index_a < long_vals.length; index_a++) {
        long a  = long_vals[index_a];
        for (int index_b = 0; index_b < long_vals.length; index_b++) {
          long b_l  = long_vals[index_b];
          expectEquals(a + (int)  b_l, a + long_to_int  (b_l));
          expectEquals(a + (short)b_l, a + long_to_short(b_l));
          expectEquals(a + (char) b_l, a + long_to_char (b_l));
          expectEquals(a + (byte) b_l, a + long_to_byte (b_l));
          int b_i  = (int)long_vals[index_b];
          expectEquals(a + (long) b_i, a + int_to_long (b_i));
          expectEquals(a + (short)b_i, a + int_to_short(b_i));
          expectEquals(a + (char) b_i, a + int_to_char (b_i));
          expectEquals(a + (byte) b_i, a + int_to_byte (b_i));
          short b_s  = (short)long_vals[index_b];
          expectEquals(a + (long) b_s, a + short_to_long (b_s));
          expectEquals(a + (int)  b_s, a + short_to_int  (b_s));
          expectEquals(a + (char) b_s, a + short_to_char (b_s));
          expectEquals(a + (byte) b_s, a + short_to_byte (b_s));
          char b_c  = (char)long_vals[index_b];
          expectEquals(a + (long)  b_c, a + char_to_long (b_c));
          expectEquals(a + (int)   b_c, a + char_to_int  (b_c));
          expectEquals(a + (short) b_c, a + char_to_short (b_c));
          expectEquals(a + (byte)  b_c, a + char_to_byte  (b_c));
        }
      }
    }

    public static void main(String[] args) {
        test_shift_int();
        test_shift_long();
        test_extend();
        System.out.println("Done!");
    }

}

