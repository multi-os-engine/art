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

package com.android.ahat;

import com.android.tools.perflib.heap.ClassObj;
import com.android.tools.perflib.heap.Field;
import com.android.tools.perflib.heap.Instance;
import java.io.File;
import java.io.IOException;
import java.util.Map;

/**
 * The TestDump class is used to get an AhatSnapshot for the test-dump
 * program.
 */
public class TestDump {
  private static TestDump mSharedTestDump = null;

  private AhatSnapshot mSnapshot = null;

  public TestDump() throws IOException {
      String hprof = System.getProperty("ahat.test.dump.hprof");
      mSnapshot = AhatSnapshot.fromHprof(new File(hprof));
  }

  /**
   * Get the AhatSnapshot for the test dump program.
   */
  public AhatSnapshot getAhatSnapshot() {
    return mSnapshot;
  }

  /**
   * Return the value of a field in the DumpedStuff instance in the
   * snapshot for the test-dump program.
   */
  public Object getDumpedThing(String name) {
    ClassObj main = mSnapshot.findClass("Main");
    Instance stuff = null;
    for (Map.Entry<Field, Object> fields : main.getStaticFieldValues().entrySet()) {
      if ("stuff".equals(fields.getKey().getName())) {
        stuff = (Instance) fields.getValue();
      }
    }
    return InstanceUtils.getField(stuff, name);
  }

  // It can take on the order of a second to parse and process the
  // test-dump hprof. This function returns a shared instance of the TestDump
  // that can be used to avoid repeating this overhead for each test case. In
  // theory the test cases should not be able to modify the loaded snapshot in
  // a way that is visible to other test cases.
  //
  // Returns null if the shared test dump has not already been initialized
  // using initializeSharedTestDump.
  public static TestDump getSharedTestDump() {
    return mSharedTestDump;
  }

  // Initialize the shared test dump.
  public static void initializeSharedTestDump() throws IOException {
    mSharedTestDump = new TestDump();
  }
}
