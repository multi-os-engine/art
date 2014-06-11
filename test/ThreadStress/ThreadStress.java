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

import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

// Run on host with:
//   javac ThreadTest.java && java ThreadStress && rm *.class
class ThreadStress implements Runnable {

    public static final boolean DEBUG = false;

    private static abstract class Operation {
        /**
         * Perform the action represented by this operation. Returns true if the thread should
         * continue. 
         */
        public abstract boolean perform();
    }

    private final static class OOM extends Operation {
        @Override
        public boolean perform() {
            try {
                List<byte[]> l = new ArrayList<byte[]>();
                while (true) {
                    l.add(new byte[1024]);
                }
            } catch (OutOfMemoryError e) {
            }
            return true;
        }
    }

    private final static class SigQuit extends Operation {
        @Override
        public boolean perform() {
            try {
                Os.kill(Os.getpid(), OsConstants.SIGQUIT);
            } catch (ErrnoException ex) {
            }
            return true;
        }
    }

    private final static class Alloc extends Operation {
        @Override
        public boolean perform() {
            try {
                List<byte[]> l = new ArrayList<byte[]>();
                for (int i = 0; i < 1024; i++) {
                    l.add(new byte[1024]);
                }
            } catch (OutOfMemoryError e) {
            }
            return true;
        }
    }

    private final static class StackTrace extends Operation {
        @Override
        public boolean perform() {
            Thread.currentThread().getStackTrace();
            return true;
        }
    }

    private final static class Exit extends Operation {
        @Override
        public boolean perform() {
            return false;
        }
    }

    private final static class Sleep extends Operation {
        @Override
        public boolean perform() {
            try {
                Thread.sleep(100);
            } catch (InterruptedException ignored) {
            }
            return true;
        }
    }

    private final static class TimedWait extends Operation {
        private final Object lock;

        public TimedWait(Object lock) {
            this.lock = lock;
        }

        @Override
        public boolean perform() {
            synchronized (lock) {
                try {
                    lock.wait(100, 0);
                } catch (InterruptedException ignored) {
                }
            }
            return true;
        }
    }

    private final static class Wait extends Operation {
        private final Object lock;

        public Wait(Object lock) {
            this.lock = lock;
        }

        @Override
        public boolean perform() {
            synchronized (lock) {
                try {
                    lock.wait();
                } catch (InterruptedException ignored) {
                }
            }
            return true;
        }
    }

    private final static class SyncAndWork extends Operation {
        private final Object lock;

        public SyncAndWork(Object lock) {
            this.lock = lock;
        }

        @Override
        public boolean perform() {
            synchronized (lock) {
                try {
                    Thread.sleep((int)(Math.random()*10));
                } catch (InterruptedException ignored) {
                }
            }
            return true;
        }
    }

    private final static Map<Operation, Double> createDefaultFrequencyMap(Object lock) {
        Map<Operation, Double> frequencyMap = new HashMap<Operation, Double>();
        frequencyMap.put(new OOM(), 0.005);             //  1/200
        frequencyMap.put(new SigQuit(), 0.095);         // 19/200
        frequencyMap.put(new Alloc(), 0.3);             // 60/200
        frequencyMap.put(new StackTrace(), 0.1);        // 20/200
        frequencyMap.put(new Exit(), 0.25);             // 50/200
        frequencyMap.put(new Sleep(), 0.125);           // 25/200
        frequencyMap.put(new TimedWait(lock), 0.05);    // 10/200
        frequencyMap.put(new Wait(lock), 0.075);        // 15/200

        return frequencyMap;
    }

    private final static Map<Operation, Double> createLockFrequencyMap(Object lock) {
      Map<Operation, Double> frequencyMap = new HashMap<Operation, Double>();
      frequencyMap.put(new Sleep(), 0.2);
      frequencyMap.put(new TimedWait(lock), 0.2);
      frequencyMap.put(new Wait(lock), 0.2);
      frequencyMap.put(new SyncAndWork(lock), 0.4);

      return frequencyMap;
    }

    public static void main(String[] args) throws Exception {
      parseAndRun(args);

      // parseAndRun(new String[] { "--locks-only", "-n", "10", "-o", "10000" });
    }

    public static void parseAndRun(String[] args) throws Exception {
        int numberOfThreads = -1;
        int totalOperations = -1;
        int operationsPerThread = -1;
        Object lock = null;
        Map<Operation, Double> frequencyMap = null;

        if (args != null) {
            for (int i = 0; i < args.length; i++) {
                if (args[i].equals("-n")) {
                    i++;
                    numberOfThreads = Integer.parseInt(args[i]);
                } else if (args[i].equals("-o")) {
                    i++;
                    totalOperations = Integer.parseInt(args[i]);
                } else if (args[i].equals("-t")) {
                    i++;
                    operationsPerThread = Integer.parseInt(args[i]);
                } else if (args[i].equals("--locks-only")) {
                    lock = new Object();
                    frequencyMap = createLockFrequencyMap(lock);
                }
            }
        }

        if (numberOfThreads == -1) {
            numberOfThreads = 5;
        }

        if (totalOperations == -1) {
            totalOperations = 1000;
        }

        if (operationsPerThread == -1) {
            operationsPerThread = totalOperations/numberOfThreads;
        }

        if (lock == null) {
            lock = new Object();
        }

        if (frequencyMap == null) {
            frequencyMap = createDefaultFrequencyMap(lock);
        }

        runTest(numberOfThreads, operationsPerThread, lock, frequencyMap);
    }

    public static void runTest(final int numberOfThreads, final int operationsPerThread,
                               final Object lock, Map<Operation, Double> frequencyMap)
                                   throws Exception {
        // Each thread is going to do operationsPerThread
        // operations. The distribution of operations is determined by
        // the Operation.frequency values. We fill out an Operation[]
        // for each thread with the operations it is to perform. The
        // Operation[] is shuffled so that there is more random
        // interactions between the threads.

        // Fill in the Operation[] array for each thread by laying
        // down references to operation according to their desired
        // frequency.
        final ThreadStress[] threadStresses = new ThreadStress[numberOfThreads];
        for (int t = 0; t < threadStresses.length; t++) {
            Operation[] operations = new Operation[operationsPerThread];
            int o = 0;
            LOOP:
            while (true) {
                for (Operation op : frequencyMap.keySet()) {
                    int freq = (int)(frequencyMap.get(op) * operationsPerThread);
                    for (int f = 0; f < freq; f++) {
                        if (o == operations.length) {
                            break LOOP;
                        }
                        operations[o] = op;
                        o++;
                    }
                }
            }
            // Randomize the oepration order
            Collections.shuffle(Arrays.asList(operations));
            threadStresses[t] = new ThreadStress(lock, t, operations);
        }

        // Enable to dump operation counds per thread to make sure its
        // sane compared to Operation.frequency
        if (DEBUG) {
            for (int t = 0; t < threadStresses.length; t++) {
                Operation[] operations = threadStresses[t].operations;
                Map<Operation, Integer> distribution = new HashMap<Operation, Integer>();
                for (Operation operation : operations) {
                    Integer ops = distribution.get(operation);
                    if (ops == null) {
                        ops = 1;
                    } else {
                        ops++;
                    }
                    distribution.put(operation, ops);
                }
                System.out.println("Distribution for " + t);
                for (Operation op : frequencyMap.keySet()) {
                    System.out.println(op + " = " + distribution.get(op));
                }
            }
        }

        // Create the runners for each thread. The runner Thread
        // ensures that thread that exit due to Operation.EXIT will be
        // restarted until they reach their desired
        // operationsPerThread.
        Thread[] runners = new Thread[numberOfThreads];
        for (int r = 0; r < runners.length; r++) {
            final ThreadStress ts = threadStresses[r];
            runners[r] = new Thread("Runner thread " + r) {
                final ThreadStress threadStress = ts;
                public void run() {
                    int id = threadStress.id;
                    System.out.println("Starting worker for " + id);
                    while (threadStress.nextOperation < operationsPerThread) {
                        Thread thread = new Thread(ts, "Worker thread " + id);
                        thread.start();
                        try {
                            thread.join();
                        } catch (InterruptedException e) {
                        }
                        System.out.println("Thread exited for " + id + " with "
                                           + (operationsPerThread - threadStress.nextOperation)
                                           + " operations remaining.");
                    }
                    System.out.println("Finishing worker for " + id);
                }
            };
        }

        // The notifier thread is a daemon just loops forever to wake
        // up threads in Operation.WAIT
        if (lock != null) {
            Thread notifier = new Thread("Notifier") {
                public void run() {
                    while (true) {
                        synchronized (lock) {
                            lock.notifyAll();
                        }
                    }
                }
            };
            notifier.setDaemon(true);
            notifier.start();
        }

        for (int r = 0; r < runners.length; r++) {
            runners[r].start();
        }
        for (int r = 0; r < runners.length; r++) {
            runners[r].join();
        }
    }

    private final Operation[] operations;
    private final Object lock;
    private final int id;

    private int nextOperation;

    private ThreadStress(Object lock, int id, Operation[] operations) {
        this.lock = lock;
        this.id = id;
        this.operations = operations;
    }

    public void run() {
        try {
            if (DEBUG) {
                System.out.println("Starting ThreadStress " + id);
            }
            while (nextOperation < operations.length) {
                Operation operation = operations[nextOperation];
                if (DEBUG) {
                    System.out.println("ThreadStress " + id
                                       + " operation " + nextOperation
                                       + " is " + operation);
                }
                nextOperation++;
                if (!operation.perform()) {
                    return;
                }
            }
        } finally {
            if (DEBUG) {
                System.out.println("Finishing ThreadStress for " + id);
            }
        }
    }
}
