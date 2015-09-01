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

import com.android.tools.perflib.heap.Snapshot;
import com.android.tools.perflib.heap.hprof.Hprof;
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
import com.android.tools.perflib.heap.io.InMemoryBuffer;
import com.google.common.io.ByteArrayDataOutput;
import com.google.common.io.ByteStreams;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/**
 * Class with utilities to help constructing snapshots for tests.
 */
public class SnapshotBuilder {

  // Construct a snapshot that contains a string instance for the given string
  // with the given instance id, and underlying char array with instance id+1.
  public static AhatSnapshot makeSnapshotWithString(int id, String str) throws IOException {
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
            new HprofInstanceField(strings.get("value"), HprofType.TYPE_OBJECT)}));

    long[] chars = new long[str.length()];
    for (int i = 0; i < str.length(); i++) {
      chars[i] = str.charAt(i);
    }
    dump.add(new HprofPrimitiveArrayDump(id + 1, 0, HprofType.TYPE_CHAR, chars));

    ByteArrayDataOutput values = ByteStreams.newDataOutput();
    values.writeInt(str.length());      // count
    values.writeInt(0);                 // hashCode
    values.writeInt(id + 1);            // value
    dump.add(new HprofInstanceDump(id, 0, stringClassObjectId, values.toByteArray()));

    records.add(new HprofHeapDump(0, dump.toArray(new HprofDumpRecord[0])));
    return SnapshotBuilder.makeSnapshot(strings, records);
  }

  // Helper function to make a snapshot with id size 4 given an
  // HprofStringBuilder and list of HprofRecords
  public static AhatSnapshot makeSnapshot(HprofStringBuilder strings, List<HprofRecord> records)
    throws IOException {
    // TODO: When perflib can handle the case where strings are referred to
    // before they are defined, just add the string records to the records
    // list.
    List<HprofRecord> actualRecords = new ArrayList<HprofRecord>();
    actualRecords.addAll(strings.getStringRecords());
    actualRecords.addAll(records);

    Hprof hprof = new Hprof("JAVA PROFILE 1.0.3", 4, new Date(), actualRecords);
    ByteArrayOutputStream os = new ByteArrayOutputStream();
    hprof.write(os);
    InMemoryBuffer buffer = new InMemoryBuffer(os.toByteArray());
    return new AhatSnapshot(Snapshot.createSnapshot(buffer));
  }
}
