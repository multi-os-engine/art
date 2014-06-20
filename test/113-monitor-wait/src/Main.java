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

    static final int iterNum = 2100;
    static final int threadNum = 120;

    static volatile Object obj;
    
    static int startedNum = 0;
    
    static Object monitorLock = new Object();

    public static void main(String[] args) {
        new Main().test();
        System.out.println("passed");
    }

    public static synchronized int updateNum() {
        return startedNum++;
    }

    private void test() {
        Thread[] t = new Thread[threadNum];
        for (int i = 0; i < threadNum; i++) {
            t[i] = new TestThread();
            t[i].setDaemon(true);
            t[i].start();
        }
        while(startedNum < threadNum) {
            Thread.yield();
        }
        for (int i = 0; i < iterNum; i++) {
            obj = new Integer[1000000];
            for (int j = 0; j < threadNum; j++) {
                t[j].interrupt();
            }
            System.gc();
        }
    }
}

class TestThread extends Thread {
    
    @Override
    public void run() {
        Main.updateNum();
        while (true) {
            synchronized (Main.monitorLock) {
                try {
                    Main.monitorLock.wait();
                } catch (InterruptedException ie) {
                }
            }
        }
    }
}
