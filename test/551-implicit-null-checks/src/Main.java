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

  public static void main(String args[]) {
    try {
      $opt$noinline$testGetLong();
    } catch (NullPointerException ex) {
      // good
    }
    try {
      $opt$noinline$testPutLong();
    } catch (NullPointerException ex) {
      // good
    }
  }

  public static void $opt$noinline$testGetLong() {
    TestCase tc = new TestCase();
    long result = tc.get();
    System.out.println("Shouldn't see this! result = " + result);
  }

  public static void $opt$noinline$testPutLong() {
    TestCase tc = new TestCase();
    tc.put(778899112233L);
    System.out.println("Shouldn't see this!");
  }
}
