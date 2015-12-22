/*
 * Copyright (C) 2013 The Android Open Source Project
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

import java.lang.reflect.*;
import java.lang.Runtime;

public class Main {
    static Object runtime;
    static Method register_native_allocation;
    static Method register_native_free;
    static long maxMem = 0;

    private static void affectsTotalMemoryTest() throws Exception {
        long start = Runtime.getRuntime().totalMemory();
        int size = (int)Math.min((maxMem - start)/32, Integer.MAX_VALUE/4);
        for (int i = 0; i < 4; i++) {
            register_native_allocation.invoke(runtime, size);
        }
        long change = Runtime.getRuntime().totalMemory() - start;
        long expected = 4*size;
        if (Math.abs(change - expected) > 0.2*expected) {
            System.out.println(
                String.format("Expected totalMemory increased by around %,d, but found an increase of %,d", expected, change));
        }
        for (int i = 0; i < 4; i++) {
            register_native_free.invoke(runtime, size);
        }
    }

    private static void leadsToOutOfMemoryTest() throws Exception {
        System.gc();
        long start = Runtime.getRuntime().totalMemory();
        int size = (int)Math.min((maxMem - start)/32, Integer.MAX_VALUE/4);

        long expected = (maxMem - start)/size;
        int count = 0;

        // Fill up memory with native allocations that there should be
        // sufficient space for.
        for (int i = 0; i < expected-1; i++) {
            register_native_allocation.invoke(runtime, size);
            count++;
        }

        // Continue filling up memory with the chance of OutOfMemoryError.
        // We may not get OutOfMemoryError right away because GC might run to
        // free up more space.
        try {
            for (int i = 0; i < expected; i++) {
                register_native_allocation.invoke(runtime, size);
                count++;
            }
            System.out.println("Expected OutOfMemoryError, but none occurred");
            System.out.println("TotalMemory: " + Runtime.getRuntime().totalMemory());
            System.out.println("MaxMemory: " + maxMem);
            System.out.println("RegisteredNative Total: " + (count * size));
        } catch (InvocationTargetException e) {
            Throwable cause = e.getCause();
            if (!(cause instanceof OutOfMemoryError)) {
                throw e;
            }
        }

        while (count > 0) {
            register_native_free.invoke(runtime, size);
            count--;
        }
    }

    public static void main(String[] args) throws Exception {
        Class<?> vm_runtime = Class.forName("dalvik.system.VMRuntime");
        Method get_runtime = vm_runtime.getDeclaredMethod("getRuntime");
        runtime = get_runtime.invoke(null);
        register_native_allocation = vm_runtime.getDeclaredMethod("registerNativeAllocation", Integer.TYPE);
        register_native_free = vm_runtime.getDeclaredMethod("registerNativeFree", Integer.TYPE);
        maxMem = Runtime.getRuntime().maxMemory();

        affectsTotalMemoryTest();
        leadsToOutOfMemoryTest();
        System.out.println("Test complete");
    }
}

