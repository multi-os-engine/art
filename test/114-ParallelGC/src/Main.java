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

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

public class Main implements Runnable {

    // Timeout in minutes. Make it larger than the run-test timeout to get a native thread dump by
    // ART on timeout when running on the host.
    public final static long TIMEOUT_VALUE = 7;

    public final static long MAX_SIZE = 1000;  // Maximum size of array-list to allocate.

    public final static int THREAD_COUNT = 16;
    public final static AtomicInteger counter = new AtomicInteger();
    public final static Object gate = new Object();

    public static void main(String[] args) throws Exception {
        Thread[] threads = new Thread[THREAD_COUNT];

        // We use two barriers. The first one ensures that the threads can start their allocation
        // run around approximately the same time. The second one ensures that they all wait before
        // ending.
        // Note: We could reuse the first barrier, but it's simpler to just have two.

        for (int i = 0; i < threads.length; i++) {
            threads[i] = new Thread(new Main());
        }
        for (Thread thread : threads) {
            thread.start();
        }
        // Sleep a little bit to give the threads time to gate.wait.
        try {
            Thread.sleep(250);
        } catch (Exception exc) {
            // Any exception is bad.
            exc.printStackTrace(System.err);
            System.exit(1);
        }
        synchronized (gate) {
            gate.notifyAll();
        }

        // Wait for the threads to finish.
        for (Thread thread : threads) {
            thread.join();
        }

        // Allocate objects to definitely run GC before quitting.
        try {
            ArrayList<Object> l = new ArrayList<Object>();
            for (int i = 0; i < 100000; i++) {
                l.add(new ArrayList<Object>(i));
            }
        } catch (OutOfMemoryError oom) {
        }
        new ArrayList<Object>(50);
    }

    private Main() {
    }

    private ArrayList<Object> store;

    public void run() {
        ArrayList<Object> l = new ArrayList<Object>();
        store = l;  // Keep it alive.

        // Wait for the start signal.
        synchronized (gate) {
            try {
                gate.wait(TIMEOUT_VALUE * 1000 * 60);
            } catch (Exception exc) {
                // Any exception is bad.
                exc.printStackTrace(System.err);
                System.exit(1);
            }
        }

        // Allocate.
        try {
            for (int i = 0; i < MAX_SIZE; i++) {
                l.add(new ArrayList<Object>(i));
            }
        } catch (OutOfMemoryError oome) {
            // Fine, we're done.
        } catch (Exception exc) {
            // Any other exception is bad.
            exc.printStackTrace(System.err);
            System.exit(1);
        }

        // Atomically increment the counter and check whether we were last.
        int number = counter.incrementAndGet();

        if (number < THREAD_COUNT) {
            // Not last, wait.
            synchronized (gate) {
                try {
                    gate.wait(TIMEOUT_VALUE * 1000 * 60);
                } catch (Exception exc) {
                    // Any exception is bad.
                    exc.printStackTrace(System.err);
                    System.exit(1);
                }
            }
        } else {
            // Last. Sleep for a bit so the other threads can get to the gate.wait...
            try {
                Thread.sleep(250);
            } catch (Exception exc) {
             // Any exception is bad.
                exc.printStackTrace(System.err);
                System.exit(1);
            }
            // Now wake up everyone.
            synchronized (gate) {
                gate.notifyAll();
            }
        }

        store = null;  // Allow GC to reclaim it.
    }
}
