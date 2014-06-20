/*
 * Copyright (C) 2014 The Android Open Source Project
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

    static final int ITER_NUM = 2100;
    static final int THREAD_NUM = 120;
    static final int ARR_SIZE = 1000000;

    static volatile Object obj;

    volatile int startedNum = 0;

    Object monitorLock = new Object();

    public static void main(String[] args) {
        new Main().test();
        System.out.println("passed");
    }

    public synchronized void incNum() {
        startedNum++;
    }

    private void test() {
        Thread[] t = new Thread[THREAD_NUM];
        for (int i = 0; i < THREAD_NUM; i++) {
            t[i] = new TestThread();
            t[i].setDaemon(true);
            t[i].start();
        }
        while(startedNum < THREAD_NUM) {
            Thread.yield();
        }
        for (int i = 0; i < ITER_NUM; i++) {
            obj = new Integer[ARR_SIZE];
            for (int j = 0; j < THREAD_NUM; j++) {
                t[j].interrupt();
            }
            System.gc();
        }
    }

    class TestThread extends Thread {

        @Override
        public void run() {
            incNum();
            while (true) {
                synchronized (monitorLock) {
                    try {
                        monitorLock.wait();
                    } catch (InterruptedException ie) {
                    }
                }
            }
        }
    }
}
