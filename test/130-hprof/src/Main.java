/*
 * Copyright (C) 2009 The Android Open Source Project
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

import java.io.File;
import java.lang.ref.WeakReference;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

public class Main {
    private static final int TEST_TIME = 10;
    private static final String OUTPUT_FILE = "gc-thrash.hprof";

    private static boolean makeArray(int i) {
        return i % 10 == 0;
    }

    private static void fillArray(Object global[], Object local[], int i) {
        // Very stupid linking.
        local[0] = global;
        for (int j = 1; j < local.length; j++) {
            local[j] = global[j];
        }
    }

    public static void main(String[] args) {
        // Create some data.
        Object data[] = new Object[100];
        for (int i = 0; i < 100; i++) {
            if (makeArray(i)) {
                data[i] = new Object[100];
            } else {
                data[i] = String.valueOf(i);
            }
        }
        for (int i = 0; i < 100; i++) {
            if (makeArray(i)) {
                Object data2[] = (Object[])data[i];
                fillArray(data, data2, i);
            }
        }
        System.out.println("Generated data.");

        File dumpFile = null;
        File convFile = null;

        try {
            // Now dump the heap.
            dumpFile = createDump();

            // Run hprof-conv on it.
            convFile = getConvFileName();
            try {
                ProcessBuilder pb = new ProcessBuilder("hprof-conv",
                        dumpFile.getAbsoluteFile().toString(),
                        convFile.getAbsoluteFile().toString());
                pb.redirectErrorStream(true);
                Process process = pb.start();
                int ret = process.waitFor();
                if (ret != 0) {
                    throw new RuntimeException("Exited abnormally with " + ret);
                }
            } catch (Exception exc) {
                throw new RuntimeException(exc);
            }
        } finally {
            // Delete the files.
            if (dumpFile != null) {
                dumpFile.delete();
            }
            if (convFile != null) {
                convFile.delete();
            }
        }
    }

    private static File createDump() {
        java.lang.reflect.Method dumpHprofDataMethod = getDumpHprofDataMethod();
        if (dumpHprofDataMethod != null) {
            File f = getDumpFileName();
            try {
                dumpHprofDataMethod.invoke(null, f.getAbsoluteFile().toString());
                return f;
            } catch (Exception exc) {
                exc.printStackTrace(System.out);
            }
        } else {
            System.out.println("Could not find dump method!");
        }
        return null;
    }

    /**
     * Finds VMDebug.dumpHprofData() through reflection.  In the reference
     * implementation this will not be available.
     *
     * @return the reflection object, or null if the method can't be found
     */
    private static Method getDumpHprofDataMethod() {
        ClassLoader myLoader = Main.class.getClassLoader();
        Class vmdClass;
        try {
            vmdClass = myLoader.loadClass("dalvik.system.VMDebug");
        } catch (ClassNotFoundException cnfe) {
            return null;
        }

        Method meth;
        try {
            meth = vmdClass.getMethod("dumpHprofData",
                    new Class[] { String.class });
        } catch (NoSuchMethodException nsme) {
            System.err.println("Found VMDebug but not dumpHprofData method");
            return null;
        }

        return meth;
    }

    private static File getDumpFileName() {
        try {
            return File.createTempFile("test-130-hprof", "dump");
        } catch (Exception exc) {
            return null;
        }
    }

    private static File getConvFileName() {
        try {
            return File.createTempFile("test-130-hprof", "conv");
        } catch (Exception exc) {
            return null;
        }
    }
}
