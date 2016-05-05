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
import java.util.IdentityHashMap;
import dalvik.system.VMRuntime;

public class Main {
  public static void main(String[] args) {
    IdentityHashMap<Object, Integer> all = new IdentityHashMap();
    for (int i = 0; i < 5000; ++i) {
      Object obj = new Object();
      synchronized(obj) {
        // Should force inflation.
        all.put(obj, obj.hashCode());
      }
    }
    System.out.println("Hopefully inflated 5000 monitors");
    // Encourage monitor deflation.
    // GC deflates only in JANK_IMPERCEPTIBLE state.
    // This is similar to code in ActivityThread.java.
    final int DALVIK_PROCESS_STATE_JANK_PERCEPTIBLE = 0;
    final int DALVIK_PROCESS_STATE_JANK_IMPERCEPTIBLE = 1;
    VMRuntime.getRuntime().updateProcessState(DALVIK_PROCESS_STATE_JANK_IMPERCEPTIBLE);
    System.gc();
    System.runFinalization();
    System.gc();
    VMRuntime.getRuntime().updateProcessState(DALVIK_PROCESS_STATE_JANK_PERCEPTIBLE);
    System.out.println("Finished GC");
    int j = 0;
    for (Object obj: all.keySet()) {
      ++j;
      if (obj.hashCode() != all.get(obj)) {
        throw new AssertionError("Failed hashcode test!");
      }
    }
    System.out.println("Finished first check");
    for (Object obj: all.keySet()) {
      ++j;
      synchronized(obj) {
        if (obj.hashCode() != all.get(obj)) {
          throw new AssertionError("Failed hashcode test!");
        }
      }
    }
    System.out.println("Finished second check");
    System.out.println("Total checks: " + j);
  }
}
