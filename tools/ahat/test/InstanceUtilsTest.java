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

import com.android.tools.perflib.heap.Instance;
import com.android.tools.perflib.heap.hprof.HprofClassDump;
import com.android.tools.perflib.heap.hprof.HprofConstant;
import com.android.tools.perflib.heap.hprof.HprofDumpRecord;
import com.android.tools.perflib.heap.hprof.HprofHeapDump;
import com.android.tools.perflib.heap.hprof.HprofInstanceDump;
import com.android.tools.perflib.heap.hprof.HprofInstanceField;
import com.android.tools.perflib.heap.hprof.HprofLoadClass;
import com.android.tools.perflib.heap.hprof.HprofPrimitiveArrayDump;
import com.android.tools.perflib.heap.hprof.HprofRecord;
import com.android.tools.perflib.heap.hprof.HprofStaticField;
import com.android.tools.perflib.heap.hprof.HprofStringBuilder;
import com.android.tools.perflib.heap.hprof.HprofType;
import com.google.common.io.ByteArrayDataOutput;
import com.google.common.io.ByteStreams;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import static org.junit.Assert.assertEquals;
import org.junit.Test;

public class InstanceUtilsTest {
  @Test
  public void asString_basic() throws IOException {
    AhatSnapshot snapshot = SnapshotBuilder.makeSnapshotWithString(0x42, "hello, world");
    Instance stringInstance = snapshot.findInstance(0x42);
    assertEquals("hello, world", InstanceUtils.asString(stringInstance));
  }

  @Test
  public void asString_embedded() throws IOException {
    // Set up a heap dump with an instance of java.lang.String of
    // "hello" with instance id 0x42 that is backed by a char array that is
    // bigger.
    HprofStringBuilder strings = new HprofStringBuilder(0);
    List<HprofRecord> records = new ArrayList<HprofRecord>();
    List<HprofDumpRecord> dump = new ArrayList<HprofDumpRecord>();

    final int stringClassObjectId = 1;
    records.add(new HprofLoadClass(0, 0, stringClassObjectId, 0,
          strings.get("java.lang.String")));
    dump.add(new HprofClassDump(stringClassObjectId, 0, 0, 0, 0, 0, 0, 0, 0,
          new HprofConstant[0], new HprofStaticField[0],
          new HprofInstanceField[]{
            new HprofInstanceField(strings.get("count"), HprofType.TYPE_INT),
            new HprofInstanceField(strings.get("hashCode"), HprofType.TYPE_INT),
            new HprofInstanceField(strings.get("offset"), HprofType.TYPE_INT),
            new HprofInstanceField(strings.get("value"), HprofType.TYPE_OBJECT)}));

    dump.add(new HprofPrimitiveArrayDump(0x41, 0, HprofType.TYPE_CHAR,
          new long[]{'n', 'o', 't', ' ', 'h', 'e', 'l', 'l', 'o', 'o', 'p'}));

    ByteArrayDataOutput values = ByteStreams.newDataOutput();
    values.writeInt(5);     // count
    values.writeInt(0);     // hashCode
    values.writeInt(4);     // offset
    values.writeInt(0x41);  // value
    dump.add(new HprofInstanceDump(0x42, 0, stringClassObjectId, values.toByteArray()));

    records.add(new HprofHeapDump(0, dump.toArray(new HprofDumpRecord[0])));
    AhatSnapshot snapshot = SnapshotBuilder.makeSnapshot(strings, records);

    Instance stringInstance = snapshot.findInstance(0x42);
    assertEquals("hello", InstanceUtils.asString(stringInstance));
  }
}
