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
  // Note: To avoid re-parsing the test-dump hprof file over and over again,
  // we save the result the first time and re-use that. In theory this could
  // be bad, because different test cases could interact through the shared
  // snapshot. In practice, if any of the code is modifying the snapshot in a
  // visible way, it's probably a bug that should be fixed anyway.
  private static AhatSnapshot mSnapshot = null;

  /**
   * Get an AhatSnapshot for the test dump program.
   * Throws an IOException if it is unable to read the test dump hprof file.
   */
  public static AhatSnapshot getAhatSnapshot() throws IOException {
    if (mSnapshot == null) {
      String hprof = System.getProperty("ahat.test.dump.hprof");
      mSnapshot = AhatSnapshot.fromHprof(new File(hprof));
    }
    return mSnapshot;
  }

  /**
   * Return the value of a field in the DumpedStuff instance in the given
   * snapshot, assuming the snapshot is for an hprof from the test-dump
   * program.
   */
  public static Object getDumpedThing(AhatSnapshot snapshot, String name) {
    ClassObj main = snapshot.findClass("Main");
    Instance stuff = null;
    for (Map.Entry<Field, Object> fields : main.getStaticFieldValues().entrySet()) {
      if ("stuff".equals(fields.getKey().getName())) {
        stuff = (Instance) fields.getValue();
      }
    }
    return InstanceUtils.getField(stuff, name);
  }
}
