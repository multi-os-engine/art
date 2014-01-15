/*
 * Copyright (C) 2011 The Android Open Source Project
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
  public static void main(String[] args) {
    System.out.println("Hello, world!");
    String hwuc = "HELLOWORLD";
    String hwlc = "helloworld";

    if (!hwuc.toLowerCase().equals(hwlc)) {
      System.out.println(" FAIL 1");
    }
    if ("\u03c3" != "\u03a3".toLowerCase()) {
      System.out.println(" FAIL 2: \u03c3 " + "\u03a3".toLowerCase());
    }
    if ("a\u03c2" != "a\u03a3".toLowerCase()) {
      System.out.println(" FAIL 3: a\u03c2 " + "a\u03a3".toLowerCase());
    }
    if ("\uD801\uDC44" != "\uD801\uDC1C".toLowerCase()) {
      System.out.println(" FAIL 4: \uD801\uDC44 " + "\uD801\uDC1C".toLowerCase());
    }
  }
}
