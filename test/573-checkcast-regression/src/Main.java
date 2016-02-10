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

  public static void main(String[] args) {
    Main main1 = new Main(new int[]{ 3 },
                          new Object[]{ new Integer(1), new Integer(2), new Integer(3) });
    int[] vector1d = { 1 };
    Integer result1 = main1.test(vector1d);
    System.out.println(result1.intValue());

    Main main2 = new Main(new int[]{ 2, 2 },
                         new Object[][]{ { new Integer(11), new Integer(12) },
                                         { new Integer(21), new Integer(22) } });
    int[] vector2d = { 1, 0 };
    Integer result2 = main2.test(vector2d);
    System.out.println(result2.intValue());

    Main main3 = new Main(new int[]{ 2, 2, 2 },
                          new Object[][][]{ { { new Integer(111), new Integer(112) },
                                              { new Integer(121), new Integer(122) } },
                                            { { new Integer(211), new Integer(212) },
                                              { new Integer(221), new Integer(222) } } });
    int[] vector3d = { 1, 0, 1 };
    Integer result3 = main3.test(vector3d);
    System.out.println(result3.intValue());
  }

  public Integer test(int[] vector) {
    Object[] lastDimension = (Object[]) array;
    for (int i = 0; i < sizes.length - 1; ++i) {
      lastDimension = (Object[]) lastDimension[vector[i]];
    }
    Integer lastValue = (Integer) lastDimension[vector[sizes.length - 1]];
    return lastValue;
  }

  public Main(int[] sizes, Object array) {
    this.sizes = sizes;
    this.array = array;
  }

  protected int[] sizes;
  private Object array;

}
