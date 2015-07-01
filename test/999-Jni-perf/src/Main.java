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
  public Main() {
  }

  private static final long N = 0x1ffffff;
  private static final double nanoRatio = 1000000000.0;
  private static final String MSG = "ABCDE";

  native int perfJniEmptyCall();
  native int perfSOACall();
  native int perfSOAUncheckedCall();

  int runPerfTest() {
    long start = System.nanoTime();
    for (long i = 0; i < N; i++) {
      char c = MSG.charAt(2);
    }
    long elapse = System.nanoTime() - start;
    System.out.println("Fast JNI: " + elapse/nanoRatio);

    start = System.nanoTime();
    for (long i = 0; i < N; i++) {
      perfJniEmptyCall();
    }
    elapse = System.nanoTime() - start;
    System.out.println("Empty call: " + elapse/nanoRatio);

    start = System.nanoTime();
    for (long i = 0; i < N; i++) {
      perfSOACall();
    }
    elapse = System.nanoTime() - start;
    System.out.println("SOA call: " + elapse/nanoRatio);

    start = System.nanoTime();
    for (long i = 0; i < N; i++) {
      perfSOAUncheckedCall();
    }
    elapse = System.nanoTime() - start;
    System.out.println("SOA unchecked call: " + elapse/nanoRatio);

    return 0;
  }

  static {
    System.loadLibrary("arttest");
  }

  public static void main(String[] args) {
    Main m = new Main();
    m.runPerfTest();
  }
}
