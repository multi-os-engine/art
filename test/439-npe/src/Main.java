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

  private volatile long x;

  private void f() { }

  public static void main(String[] args) {
    try {
      $opt$no_inline$directCall(null);
      throw new RuntimeException("Failed to present NullPointerException for directCall");
    } catch (NullPointerException npe) { }
    try {
      $opt$no_inline$virtualCall(null);
      throw new RuntimeException("Failed to present NullPointerException for virtualCall");
    } catch (NullPointerException npe) { }
    try {
      $opt$no_inline$fieldGet(null);
      throw new RuntimeException("Failed to present NullPointerException for fieldGet");
    } catch (NullPointerException npe) { }
    try {
      $opt$no_inline$fieldSet(null);
      throw new RuntimeException("Failed to present NullPointerException for fieldSet");
    } catch (NullPointerException npe) { }
    try {
      $opt$no_inline$arraySet(null);
      throw new RuntimeException("Failed to present NullPointerException for arraySet");
    } catch (NullPointerException npe) { }
    try {
      $opt$no_inline$arrayGet(null);
      throw new RuntimeException("Failed to present NullPointerException for arraySet");
    } catch (NullPointerException npe) { }
  }

  public static void $opt$no_inline$directCall(Main a) {
    a.f();
  }

  public static void $opt$no_inline$virtualCall(Main a) {
    a.toString();
  }

  public static long $opt$no_inline$fieldGet(Main a) {
    return a.x;
  }

  public static void $opt$no_inline$fieldSet(Main a) {
    a.x = 19;
  }

  public static void $opt$no_inline$arraySet(Main[] a) {
    a[Integer.MAX_VALUE] = null;
  }

  public static Object $opt$no_inline$arrayGet(Main[] a) {
    return a[Integer.MAX_VALUE];
  }
}
