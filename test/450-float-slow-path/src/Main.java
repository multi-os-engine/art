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
  static double[] field = new double[]
      { 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0 };

  public static void main(String[] args) {
    testMultiple();
    testOne();
    testTwo();
  }

  public static void testOne() {
    double a = field[0];
    double b = field[1];
    double c = field[2];
    double d = field[3];
    double e = field[4];
    double f = field[5];
    double g = field[6];
    double h = field[7];
    float i = (float)field[8];

    System.err.println("Force slow path 1");

    System.err.println(a);
    System.err.println(b);
    System.err.println(c);
    System.err.println(d);
    System.err.println(e);
    System.err.println(f);
    System.err.println(g);
    System.err.println(h);
    System.err.println(i);
  }

  public static void testTwo() {
    double a = field[0];
    double b = field[1];
    double c = field[2];
    double d = field[3];
    double e = field[4];
    double f = field[5];
    double g = field[6];
    double h = field[7];
    float i = (float)field[8];
    float j = (float)field[9];

    System.err.println("Force slow path 2");

    System.err.println(a);
    System.err.println(b);
    System.err.println(c);
    System.err.println(d);
    System.err.println(e);
    System.err.println(f);
    System.err.println(g);
    System.err.println(h);
    System.err.println(i);
    System.err.println(j);
  }

  public static void testMultiple() {
    double a = field[0];
    double b = field[1];
    double c = field[2];
    double d = field[3];
    double e = field[4];
    double f = field[5];
    double g = field[6];
    double h = field[7];
    double i = field[8];
    double j = field[9];
    double k = field[10];

    System.err.println("Force slow path multiple");

    System.err.println(a);
    System.err.println(b);
    System.err.println(c);
    System.err.println(d);
    System.err.println(e);
    System.err.println(f);
    System.err.println(g);
    System.err.println(h);
    System.err.println(i);
    System.err.println(j);
    System.err.println(k);
  }
}
