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
package com.intel.crtl.androidgctest;

import java.io.BufferedReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Random;
import java.lang.reflect.Method;

import android.os.Build;
import android.os.Bundle;
import android.os.Debug;
import android.os.Message;
import android.os.Parcelable;
import android.os.SystemClock;
import android.util.Log;

// 6 kinds of small objects
class TreeNode { // object size = 16
    TreeNode left, right;
    TreeNode (TreeNode l, TreeNode r) { left = l; right = r; }
    TreeNode () { }
    int count() {
        int c = 1;
        if (left != null)
            c += left.count();
        if (right != null)
            c += right.count();
        return c;
    }
}

class TreeNode32 extends TreeNode { // object size = 24
    private long payload1;
    TreeNode32(TreeNode32 l, TreeNode32 r) { left = l; right = r; }
    TreeNode32() { }
}

class TreeNode64 extends TreeNode { // object size = 40
    private long payload1, payload2, payload3;
    TreeNode64(TreeNode64 l, TreeNode64 r) { left = l; right = r; }
    TreeNode64() { }
}

class TreeNode128 extends TreeNode { // object size = 96
    private long payload1, payload2, payload3, payload4, payload5;
    private long payload6, payload7, payload8, payload9, payload10;
    TreeNode128(TreeNode128 l, TreeNode128 r) { left = l; right = r; }
    TreeNode128() { }
}

class TreeNode256 extends TreeNode { // object size = 192
    private long payload1, payload2, payload3, payload4, payload5;
    private long payload6, payload7, payload8, payload9, payload10;
    private long payload11, payload12, payload13, payload14, payload15;
    private long payload16, payload17, payload18, payload19, payload20;
    private long payload21, payload22;
    TreeNode256(TreeNode256 l, TreeNode256 r) { left = l; right = r; }
    TreeNode256() { }
}

class TreeNode512 extends TreeNode { // object size = 336
    private long payload1, payload2, payload3, payload4, payload5;
    private long payload6, payload7, payload8, payload9, payload10;
    private long payload11, payload12, payload13, payload14, payload15;
    private long payload16, payload17, payload18, payload19, payload20;
    private long payload101, payload102, payload103, payload104, payload105;
    private long payload106, payload107, payload108, payload109, payload100;
    private long payload111, payload112, payload113, payload114, payload115;
    private long payload116, payload117, payload118, payload119, payload120;
    TreeNode512(TreeNode512 l, TreeNode512 r) { left = l; right = r; }
    TreeNode512() { }
}

class LinkNode {
    LinkNode next;
    TreeNode treeNode;
    LinkNode(TreeNode n) { treeNode = n; }
    LinkNode() { }
}

class LivedLink {
    public LinkNode treeHead;
    LivedLink() { treeHead = null;}
    void insertNode(TreeNode newTree) {
        if (treeHead == null) {
            treeHead = new LinkNode(newTree);
            treeHead.next = null;
        } else {
            LinkNode newNode = new LinkNode(newTree);
            newNode.next = treeHead;
            treeHead = newNode;
        }
    }
}

public class AndroidGCTestMain {
    private final String mTag = "AndroidGCTest";
    private MainActivity mActivity = null;

    // size of small objects
    public static final int OBJECT_16_BYTE = 0;
    public static final int OBJECT_32_BYTE = 1;
    public static final int OBJECT_64_BYTE = 2;
    public static final int OBJECT_128_BYTE = 3;
    public static final int OBJECT_256_BYTE = 4;
    public static final int OBJECT_512_BYTE = 5;
    public static final int OBJECT_LARGE_BYTE = 6;
    public static final int OBJECT_SIZE_TYPE = 7;
    private final int[] mObjectSize = new int[]{16, 24, 40, 96, 192, 336};

    // size of large object, default is 12KByte
    private int mLargeObjectSize = 0;
    // array element type distribution for large object
    // {1-byte array, 2-byte array, 4-byte array, 8-byte array}
    private float[] mLargeObjectDistribution = null;

    // allocation configuration
    // total allocate size
    private int mTotalAllocateSize = 0;
    // total allocate size by one thread
    private int mTotalAllocSizePerThread = 0;

    // unit for tree allocation/deletion operation.
    // mBucketSize is also the unit to measure object lifetime.
    // Benchmark allocates mBucketSize of data -> deletes some data
    // -> allocates mBucketSize of Data
    private int mBucketSize = 0;

    // allocated object size distribution
    // {[1-16], [17-32], [33-64], [65, 128], [129, 256], [257, 512], >12K}
    private float[] mSizeDistribution = null;

    // allocated object lifetime:
    // mLifetime[X][0] is long lived object percentage of object size X,
    // mLifetime[X][1] is percentage of object die immediately,
    // mLifetime[X][2] is percentage of object die in next period after
    // creation period ...
    private float[][] mLifetime =  null;

    private int mLongLiveSmallObjectSize;

    // a big array to store random values to decide object size of next allocated object
    private byte[][] mObjectSizeRandom = null;
    // a big array to store random values to decide lifetime of next allocated object
    private byte[][] mLifetimeRandom = null;
    private int mShortLiveTreeCount;
    private int mTotalNodeCount;
    private final int mTreeCountParallel = 4;

    private int[] mLargeArrayLength = null;
    private int[] mLargeArrayNum = null;
    private int[] mLargeArrayInter = null;
    private int mLongLiveLargeObjectSize;
    private int[][] mDieNumLargeObject = null;

    // thread mode configuration
    private int mThreadNum;
    private boolean mSingleThread = false;

    private long[] mElapseTime = null;
    private int[] mHeapFootprint = null;
    private long[] mHeapBytesAllocated = null;

    private long mTotalGcCount;
    private long mTotalGcTime;
    private long mTotalBlockingGcCount;
    private long mTotalBlockingGcTime;
    public final String[] GC_CAUSE_ART = new String[]{"Alloc", "Background", "Explicit",
        "NativeAlloc", "CollectorTransition", "DisableMovingGc", "HomogeneousSpaceCompact",
        "HeapTrim"};
    public final String[] GC_CAUSE_DALVIK = new String[]{"GC_FOR_ALLOC","GC_CONCURRENT",
        "GC_EXPLICIT", "GC_BEFORE_OOM"};
    private int[] mGcCauseCount = null;
    private float[] mGcPauseTime = null;
    private float[] mGcTotalTime = null;
    private String[] mGcCause = null;

    private boolean isArt;
    private boolean mWorkloadComplete;
    private int mIterNum = 100;

    private FileWriter fileOutput;
    private BenchThread[] mTestThreads;
    private boolean mOutOfMemory;

    // initialize benchmark setting
    private void init() {
        // thread number;
        if (mSingleThread) {
            mThreadNum = 1;
        } else if (mThreadNum == 0) {
            mThreadNum = Runtime.getRuntime().availableProcessors();
        }
        if (mThreadNum == 0) {
            mThreadNum = 1;
        }

        if (mSizeDistribution == null) {
            mSizeDistribution = new float[]{0.0436f,0.5465f,0.2103f,0.1499f,
                0.0275f,0.0125f,0.0097f};
        }
        if (mLifetime == null) {
            mLifetime = new float[][] {
                {0.0865f,0.5404f,0.2887f,0.0865f},
                {0.0469f,0.7724f,0.1460f,0.0346f},
                {0.1154f,0.5982f,0.1880f,0.0984f},
                {0.0662f,0.7851f,0.1077f,0.0411f},
                {0.0520f,0.8778f,0.0503f,0.0198f},
                {0.1628f,0.7137f,0.0821f,0.0414f},
                {0.0923f,0.7117f,0.1769f,0.0192f} // large object lifetime
            };
        }

        if (mLargeObjectDistribution == null) {
            mLargeObjectDistribution = new float[]{0.9f, 0.07f, 0.02f, 0.01f};
        }

        if (mTotalAllocateSize == 0) {
            mTotalAllocateSize = 100 * 1024 * 1024;
        }
        mTotalAllocSizePerThread = mTotalAllocateSize / mThreadNum;

        if (mBucketSize == 0) {
            mBucketSize = 1 * 1024 * 1024;
        }

        if (mLargeObjectSize == 0) {
            mLargeObjectSize = 12 * 1024;
        }

        Log.i(mTag, "Allocate Size: " + (mTotalAllocateSize / 1024) + " kB");
        Log.i(mTag, "BucketSize: " + (mBucketSize / 1024) + " kB");
        Log.i(mTag, "Large object size: " + (mLargeObjectSize / 1024) + " kB");
        Log.i(mTag, "Single Thread: " + mSingleThread);
        Log.i(mTag, "Thread number: " + mThreadNum);
        Log.i(mTag, "Stress test part iterates " + mIterNum + " times");

        String curDate = DateFormat.getDateTimeInstance().format(Calendar.getInstance().getTime());
        try {
            fileOutput = new FileWriter("/sdcard/AndroidGCTest-result.txt", true);
            fileOutput.append("\n\n" + curDate);

            fileOutput.append("\nWorkload configuration:");
            fileOutput.append("\n\tAllocate Size: " + (mTotalAllocateSize / 1024) + " kB");
            fileOutput.append("\n\tBucketSize: " + (mBucketSize / 1024) + " kB");
            fileOutput.append("\n\tLarge object size: " + (mLargeObjectSize / 1024) + " kB");
            fileOutput.append("\n\tThread mode: "
                              + (mSingleThread? "Single-Thread" : "Multi-Thread"));
            fileOutput.append("\n\tThread number: " + mThreadNum);
            fileOutput.append("\n\tStress test part iterates " + mIterNum + " times");
        } catch (IOException e) {
            Log.i(mTag, "Cannot open /sdcard/AndroidGCTest-result.txt" + e.getMessage());
        }

        int smallObjectSize, largeObjectSize;
        largeObjectSize = (int)(mTotalAllocSizePerThread * mSizeDistribution[OBJECT_LARGE_BYTE]);
        smallObjectSize = mTotalAllocSizePerThread - largeObjectSize;

        float[] countDistr = new float[OBJECT_LARGE_BYTE];
        float nodeSize = 0.0f;
        int[] sizeThreshold = new int[OBJECT_LARGE_BYTE];
        int lifetimeLen = mLifetime[0].length;
        int[][] lifetimeThreshold = new int[OBJECT_LARGE_BYTE][lifetimeLen];
        float sum = 0.0f;
        for (int i = 0; i < OBJECT_LARGE_BYTE; i++) {
            countDistr[i] = mSizeDistribution[i]
                / (1 - mSizeDistribution[OBJECT_LARGE_BYTE]) / mObjectSize[i];
            sum += countDistr[i];
        }

        mLongLiveSmallObjectSize = 0;
        float sum1 = 0.0f;
        for (int i = 0; i < OBJECT_LARGE_BYTE; i++) {
            sum1 += countDistr[i];
            sizeThreshold[i] = (int)(sum1 * 1000 / sum + 0.5);
            nodeSize += mObjectSize[i] * countDistr[i] / sum;

            float sum2 = 0.0f;
            for (int j = 1; j < lifetimeLen; j++) {
                sum2 += mLifetime[i][j];
                lifetimeThreshold[i][j-1] = (int)(sum2 * 1000 + 0.5);
            }
            lifetimeThreshold[i][lifetimeLen - 1] = 1000;

            mLongLiveSmallObjectSize += mTotalAllocSizePerThread * mSizeDistribution[i]
                * mLifetime[i][0];
        }
        sizeThreshold[OBJECT_512_BYTE] = 1000;


        mShortLiveTreeCount = (int)(smallObjectSize / mBucketSize + 0.5);
        mTotalNodeCount = (int)(mBucketSize / nodeSize * 1.11);

        mObjectSizeRandom = new byte[mThreadNum][mTotalNodeCount];
        mLifetimeRandom = new byte[mThreadNum][mTotalNodeCount];
        for (int i = 0; i < mThreadNum; i++) {
            Random r_size = new Random(i);
            Random r_lifetime = new Random(i + mThreadNum);
            for (int j = 0; j < mTotalNodeCount; j++) {
                int vs = r_size.nextInt(1000);
                int vl = r_lifetime.nextInt(1000);
                for (int k = 0; k < OBJECT_LARGE_BYTE; k++) {
                    if (vs < sizeThreshold[k]) {
                        mObjectSizeRandom[i][j] = (byte)(OBJECT_16_BYTE + k);
                        for (int l = 0; l < lifetimeLen; l++) {
                            if (vl < lifetimeThreshold[k][l]) {
                                mLifetimeRandom[i][j] = (byte)l;
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }

        mLongLiveLargeObjectSize = (int)(largeObjectSize * mLifetime[OBJECT_LARGE_BYTE][0]);
        mDieNumLargeObject = new int[4][lifetimeLen - 1];
        mLargeArrayInter = new int[4];
        mLargeArrayLength = new int[4];
        mLargeArrayNum = new int[4];
        for (int i = 0; i < 4; i++) {
            mLargeArrayLength[i] = (int)((largeObjectSize * mLargeObjectDistribution[i]
                    / mLargeObjectSize) + 0.5);
            if (mLargeArrayLength[i] == 0) {
                mLargeArrayInter[i] = mShortLiveTreeCount + 1;
                mLargeArrayNum[i] = 0;
                continue;
            }
            if (mShortLiveTreeCount > mLargeArrayLength[i]) {
                mLargeArrayInter[i] = mShortLiveTreeCount / mLargeArrayLength[i];
                mLargeArrayNum[i] = 1;
                mLargeArrayLength[i] = mShortLiveTreeCount/mLargeArrayInter[i] + 1;
            } else {
                mLargeArrayInter[i] = 1;
                mLargeArrayNum[i] = (int)(mLargeArrayLength[i]/(float)mShortLiveTreeCount + 0.5);
                mLargeArrayLength[i] = mLargeArrayNum[i] * mShortLiveTreeCount;
            }

            sum = 0.0f;
            for (int j = 1; j < lifetimeLen; j++) {
                sum += mLifetime[OBJECT_LARGE_BYTE][j];
                mDieNumLargeObject[i][j-1] = (int)(mLargeArrayNum[i] * sum);
            }
        }

        Log.i(mTag, "init done");
    }

    private void clearInitData() {
        Log.i(mTag, "clear auxiliary data to run workload");
        mLifetime = null;
        mLargeObjectDistribution = null;
        mSizeDistribution = null;
        mObjectSizeRandom = null;
        mLifetimeRandom = null;
        mLargeArrayInter = null;
        mLargeArrayLength = null;
        mLargeArrayNum = null;
        mDieNumLargeObject = null;
        Runtime.getRuntime().runFinalization();
        Runtime.getRuntime().gc();
    }

    AndroidGCTestMain() {
        init();
    }
    AndroidGCTestMain(MainActivity activity, int heapSize, int bucketSize, int largeObjectSize,
                       float[]sizeDistr, float[][] lifetimeDistr, float[] largeObjectSizeDist,
                       boolean singleThread, int thread_num, int exeTime) {
        mActivity = activity;
        mTotalAllocateSize = heapSize * 1024 * 1024;
        mBucketSize = bucketSize * 1024 * 1024;
        mLargeObjectSize = largeObjectSize * 1024;
        mSizeDistribution = sizeDistr;
        mLifetime = lifetimeDistr;
        mLargeObjectDistribution = largeObjectSizeDist;
        mSingleThread = singleThread;
        mThreadNum = mSingleThread? 1 : thread_num;
        mIterNum = exeTime > 0 ? exeTime : 100;
        init();
    }

    private void freeArrays(byte[][] byteArray, char[][] charArray, int[][] intArray,
                            long[][] longArray, int[] arrayIdx, int treeCount) {
        int len = mDieNumLargeObject[0].length;
        int died, release, phase;

        phase = treeCount % mLargeArrayInter[0];
        if (byteArray != null && phase <= len) {
            int curIdx = arrayIdx[0] - mLargeArrayNum[0];
            while (curIdx >= 0 && phase <= len) {
                if (phase == 0)
                    died = 0;
                else
                    died = mDieNumLargeObject[0][phase - 1];
                if (phase == len) {
                    if (byteArray[curIdx + died] != null) {
                        for (int i = died; i < mLargeArrayNum[0]; i++)
                            byteArray[curIdx + i] = null;
                    }
                    break;
                }
                release = mDieNumLargeObject[0][phase];
                for (int i = died; i < release; i++) {
                    if (i >= mLargeArrayNum[0])
                        break;
                    byteArray[curIdx + i] = null;
                }
                curIdx -= mLargeArrayNum[0];
                phase += mLargeArrayInter[0];
            }
        }

        phase = treeCount % mLargeArrayInter[1];
        if (charArray != null && phase < mLargeArrayNum[1]) {
            int curIdx = arrayIdx[1] - mLargeArrayNum[1];
            while (curIdx >= 0 && phase <= len) {
                if (phase == 0)
                    died = 0;
                else
                    died = mDieNumLargeObject[1][phase - 1];
                if (phase == len) {
                    if (charArray[curIdx + died] != null) {
                        for (int i = died; i < mLargeArrayNum[1]; i++)
                            charArray[curIdx + i] = null;
                    }
                    break;
                }
                release = mDieNumLargeObject[1][phase];
                for (int i = died; i < release; i++) {
                    if (i >= mLargeArrayNum[1])
                        break;
                    charArray[curIdx + i] = null;
                }
                curIdx -= mLargeArrayNum[1];
                phase += mLargeArrayInter[1];
            }
        }

        phase = treeCount % mLargeArrayInter[2];
        if (intArray != null && phase < mLargeArrayNum[2]) {
            int curIdx = arrayIdx[2] - mLargeArrayNum[2];
            while (curIdx >= 0 && phase <= len) {
                if (phase == 0)
                    died = 0;
                else
                    died = mDieNumLargeObject[2][phase - 1];
                if (phase == len) {
                    if (intArray[curIdx + died] != null) {
                        for (int i = died; i < mLargeArrayNum[2]; i++)
                            intArray[curIdx + i] = null;
                    }
                    break;
                }
                release = mDieNumLargeObject[2][phase];
                for (int i = died; i < release; i++) {
                    if (i >= mLargeArrayNum[2])
                        break;
                    intArray[curIdx + i] = null;
                }
                curIdx -= mLargeArrayNum[2];
                phase += mLargeArrayInter[2];
            }
        }

        phase = treeCount % mLargeArrayInter[3];
        if (longArray != null && phase < mLargeArrayNum[3]) {
            int curIdx = arrayIdx[3] - mLargeArrayNum[3];
            while (curIdx >= 0 && phase <= len) {
                if (phase == 0)
                    died = 0;
                else
                    died = mDieNumLargeObject[3][phase - 1];
                if (phase == len) {
                    if (longArray[curIdx + died] != null) {
                        for (int i = died; i < mLargeArrayNum[3]; i++)
                            longArray[curIdx + i] = null;
                    }
                    break;
                }
                release = mDieNumLargeObject[3][phase];
                for (int i = died; i < release; i++) {
                    if (i >= mLargeArrayNum[3])
                        break;
                    longArray[curIdx + i] = null;
                }
                curIdx -= mLargeArrayNum[3];
                phase += mLargeArrayInter[3];
            }
        }
    }

    private void makeTreesLongLive(TreeNode[] trees, int allocSize, int myId, int[] nodeCount) {
        while (true) {
            if (allocSize <= 0) {
                trees = null;
                return;
            }

            int len = trees.length;
            TreeNode[] nextTrees = new TreeNode[len*2];
            byte random;
            for (int i = 0; i < len; i++) {
                TreeNode node = null;
                random = mObjectSizeRandom[myId][nodeCount[0]];
                switch (random) {
                    case OBJECT_16_BYTE:
                        node = new TreeNode();
                        break;
                    case OBJECT_32_BYTE:
                        node = new TreeNode32();
                        break;
                    case OBJECT_64_BYTE:
                        node = new TreeNode64();
                        break;
                    case OBJECT_128_BYTE:
                        node = new TreeNode128();
                        break;
                    case OBJECT_256_BYTE:
                        node = new TreeNode256();
                        break;
                    case OBJECT_512_BYTE:
                        node = new TreeNode512();
                        break;
                }
                trees[i].left = node;
                nextTrees[2 * i] = node;
                allocSize -= mObjectSize[random];
                nodeCount[0]++;
                if (nodeCount[0] == mTotalNodeCount)
                    nodeCount[0] = 0;

                random = mObjectSizeRandom[myId][nodeCount[0]];
                switch (random) {
                    case OBJECT_16_BYTE:
                        node = new TreeNode();
                        break;
                    case OBJECT_32_BYTE:
                        node = new TreeNode32();
                        break;
                    case OBJECT_64_BYTE:
                        node = new TreeNode64();
                        break;
                    case OBJECT_128_BYTE:
                        node = new TreeNode128();
                        break;
                    case OBJECT_256_BYTE:
                        node = new TreeNode256();
                        break;
                    case OBJECT_512_BYTE:
                        node = new TreeNode512();
                        break;
                }
                trees[i].right = node;
                nextTrees[2*i + 1] = node;
                allocSize -= mObjectSize[random];
                nodeCount[0]++;
                if (nodeCount[0] == mTotalNodeCount)
                    nodeCount[0] = 0;

                if (allocSize <= 0) {
                    trees = null;
                    break;
                }
            }
            trees = null;
            trees = nextTrees;
        }
    }

    private void makeTreesShortLive(TreeNode[] trees, int allocSize, int myId, int[] nodeCount) {
        int lifetimeLen = mLifetime[0].length;
        int[] curTreeIdx = new int[lifetimeLen];
        int[] nextTreeIdx = new int[lifetimeLen];
        TreeNode[][] curTrees = new TreeNode[lifetimeLen][1];
        TreeNode[][] nextTrees = new TreeNode[lifetimeLen][];
        for (int i = 0; i < lifetimeLen; i++) {
            curTrees[i][0] = trees[i];
            curTreeIdx[i] = 0;
        }

        while (true) {
            if (allocSize <= 0) {
                trees = null;
                return;
            }

            byte rs = mObjectSizeRandom[myId][nodeCount[0]];
            TreeNode node = null;
            switch (rs) {
            case OBJECT_16_BYTE:
                node = new TreeNode();
                break;
            case OBJECT_32_BYTE:
                node = new TreeNode32();
                break;
            case OBJECT_64_BYTE:
                node = new TreeNode64();
                break;
            case OBJECT_128_BYTE:
                node = new TreeNode128();
                break;
            case OBJECT_256_BYTE:
                node = new TreeNode256();
                break;
            case OBJECT_512_BYTE:
                node = new TreeNode512();
                break;
            }

            byte rl = mLifetimeRandom[myId][nodeCount[0]];
            if (nextTrees[rl] == null) {
                nextTrees[rl] = new TreeNode[curTrees[rl].length << 1];
                nextTreeIdx[rl] = 0;
                nextTrees[rl][0] = node;
            } else {
                nextTrees[rl][++nextTreeIdx[rl]] = node;
            }
            TreeNode parent = curTrees[rl][curTreeIdx[rl]];
            if (parent.left == null) {
                parent.left = node;
            } else {
                parent.right = node;
                curTreeIdx[rl]++;
                if (curTreeIdx[rl] == curTrees[rl].length) {
                    curTrees[rl] = nextTrees[rl];
                    nextTrees[rl] = null;
                    curTreeIdx[rl] = 0;
                }
            }

            allocSize -= mObjectSize[rs];
            nodeCount[0]++;
            if (nodeCount[0] == mTotalNodeCount) nodeCount[0] = 0;
        }
    }

    public boolean allocTrace(int myId) throws OutOfMemoryError {
        // long-lived data
        Log.i(mTag, "Thread-" + myId + " ----- Build long lived trees -----");

        LivedLink longLiveTreeLink = new LivedLink();
        TreeNode[] trees = new TreeNode[mTreeCountParallel];

        int[] nodeCount = new int[]{0};
        for (int i = 0; i < mTreeCountParallel; i++) {
            TreeNode node = new TreeNode();
            longLiveTreeLink.insertNode(node);
            trees[i] = node;
        }

        makeTreesLongLive(trees, mLongLiveSmallObjectSize, myId, nodeCount);

        trees = null;

        Log.i(mTag, "Thread-" + myId + " ----- Build long lived byte array -----");
        int longLiveArrayCount = (int)(mLongLiveLargeObjectSize / mLargeObjectSize + 0.5);
        if (longLiveArrayCount <= 0)
            longLiveArrayCount = 1;
        byte[][] longLiveByteArrays = new byte[longLiveArrayCount][];
        for (int i = 0; i < longLiveArrayCount; i++) {
            longLiveByteArrays[i] = new byte[mLargeObjectSize];
            for (int j = 0; j < mLargeObjectSize; j+=100)
                longLiveByteArrays[i][j] = (byte) 0xff;
        }

        // stress test
        Log.i(mTag, "Thread-" + myId + " ----- Stress test -----");
        Debug.MemoryInfo memInfo = new Debug.MemoryInfo();

        int lifetimeLen = mLifetime[0].length;
        TreeNode[][] shortLiveTrees = new TreeNode[lifetimeLen][lifetimeLen];

        byte[][] shortLiveByteArray = null;
        char[][] shortLiveCharArray = null;
        int[][] shortLiveIntArray = null;
        long[][] shortLiveLongArray = null;

        nodeCount[0] = 0;
        int round = 0;

        for (int iter = 0; iter < mIterNum; iter++) {
            if (mOutOfMemory)
                break;
            int[] allocIdx = new int[]{0, 0, 0, 0};

            if (mLargeArrayLength[0] != 0)
                shortLiveByteArray = new byte[mLargeArrayLength[0]][];
            if (mLargeArrayLength[1] != 0)
                shortLiveCharArray = new char[mLargeArrayLength[1]][];
            if (mLargeArrayLength[2] != 0)
                shortLiveIntArray = new int[mLargeArrayLength[2]][];
            if (mLargeArrayLength[3] != 0)
                shortLiveLongArray = new long[mLargeArrayLength[3]][];

            int treeCount = 0;
            while (treeCount < mShortLiveTreeCount) {
                int treesIdx = round % lifetimeLen;
                round++;
                for (int i = 0; i < lifetimeLen; i++) {
                    TreeNode node = new TreeNode();
                    shortLiveTrees[treesIdx][i] = node;
                }
                makeTreesShortLive(shortLiveTrees[treesIdx], mBucketSize, myId, nodeCount);

                for (int i = 0; i < lifetimeLen; i++) {
                    int idx = (treesIdx - i + lifetimeLen) % lifetimeLen;
                    shortLiveTrees[idx][i] = null;
                }

                if ((mLargeArrayLength[0] > 0) && ((treeCount % mLargeArrayInter[0]) == 0)) {
                    for (int n = 0; n < mLargeArrayNum[0]; n++)
                        shortLiveByteArray[allocIdx[0] + n] = new byte[mLargeObjectSize];
                    allocIdx[0] += mLargeArrayNum[0];
                }

                if ((mLargeArrayLength[1] > 0) && ((treeCount % mLargeArrayInter[1]) == 0)) {
                    for (int n = 0; n < mLargeArrayNum[1]; n++)
                        shortLiveCharArray[allocIdx[1] + n] = new char[mLargeObjectSize/2];
                    allocIdx[1] += mLargeArrayNum[1];
                }

                if ((mLargeArrayLength[2] > 0) && ((treeCount % mLargeArrayInter[2]) == 0)) {
                    for (int n = 0; n < mLargeArrayNum[2]; n++)
                        shortLiveIntArray[allocIdx[2] + n] = new int[mLargeObjectSize/4];
                    allocIdx[2] += mLargeArrayNum[2];
                }

                if ((mLargeArrayLength[3] > 0) && ((treeCount % mLargeArrayInter[3]) == 0)) {
                    for (int n = 0; n < mLargeArrayNum[3]; n++)
                        shortLiveLongArray[allocIdx[3] + n] = new long[mLargeObjectSize/8];
                    allocIdx[3] += mLargeArrayNum[3];
                }

                freeArrays(shortLiveByteArray, shortLiveCharArray, shortLiveIntArray,
                           shortLiveLongArray, allocIdx, treeCount);

                treeCount++;
            }

            if (myId == 0) {
                Debug.getMemoryInfo(memInfo);
                mHeapFootprint[iter] = memInfo.dalvikPss;
                if (getRuntimeStatMethod != null)
                    mHeapBytesAllocated[iter] = (getRuntimeStat("art.gc.bytes-allocated")
                                                 - getRuntimeStat("art.gc.bytes-freed")) / 1024;
                else
                    mHeapBytesAllocated[iter] = (Runtime.getRuntime().totalMemory()
                                                 - Runtime.getRuntime().freeMemory()) / 1024;

                Message m = mActivity.getHandler().obtainMessage(MainActivity.BenchmarkProgress);
                m.arg1 = iter + 1;
                mActivity.getHandler().sendMessage(m);
            }
        }

        if (longLiveTreeLink.treeHead.treeNode != null
            && longLiveByteArrays[0][100] == (byte)0xff
            && longLiveByteArrays[longLiveArrayCount - 1][200] == (byte)0xff)
            return true;
        return false;
    }

    class BenchThread extends Thread {
        private  int myId;
        BenchThread (int id) {
            super();
            myId = id;
        }
        public void run() {
            mElapseTime[myId] = 0;
            long start = System.currentTimeMillis();
            try {
                if (!allocTrace(myId))
                    Log.i(mTag, "Error in thread-" + myId);
            } catch (OutOfMemoryError e) {
                mOutOfMemory = true;
                Log.i(mTag, "Thread-" + myId + " meets OutOfMemory");

                Message m = mActivity.getHandler().obtainMessage(
                                                        MainActivity.BenchmarkOutOfMemoryError);
                m.arg1 = myId;
                mActivity.getHandler().sendMessage(m);
            }
            mElapseTime[myId] = System.currentTimeMillis() - start;
        }
    }

    public void start() {
        if (getRuntimeStatMethod == null) {
            try {
                Class c = Class.forName("android.os.Debug");
                getRuntimeStatMethod = c.getDeclaredMethod("getRuntimeStat", String.class);
            } catch (ClassNotFoundException e) {
                Log.w(mTag, "Cannot find android.os.Debug class");
            } catch (NoSuchMethodException e) {
                Log.w(mTag, "No getRuntimeStat method in android.os.Debug");
            }
        }
        mElapseTime = new long[mThreadNum];
        mHeapFootprint = new int[mIterNum];
        mHeapBytesAllocated = new long[mIterNum];
        isArt = false;
        String vmVersion = System.getProperty("java.vm.version");
        isArt = vmVersion != null && vmVersion.startsWith("2");
        String androidVersion = Build.VERSION.RELEASE;
        String deviceName = Build.MODEL;
        String deviceDesc = deviceName + "/" + "android-" + androidVersion
            + ", runtime: " + (isArt? "ART" :"Dalvik");
        mTotalGcCount = getRuntimeStat("art.gc.gc-count");
        mTotalGcTime = getRuntimeStat("art.gc.gc-time");
        mTotalBlockingGcCount = getRuntimeStat("art.gc.blocking-gc-count");
        mTotalBlockingGcTime = getRuntimeStat("art.gc.blocking-gc-time");
        if (isArt)
            mGcCauseCount = new int[]{0, 0, 0, 0, 0, 0, 0, 0};
        else
            mGcCauseCount = new int[]{0, 0, 0, 0};

        mWorkloadComplete = false;
        clearLogcat();
        Thread logcat = new Thread(new Runnable() {
            @Override
            public void run() {
                readLogcat();
            }
        });
        logcat.start();

        mOutOfMemory = false;
        mTestThreads = new BenchThread[mThreadNum];
        for (int i = 1; i < mThreadNum; i++) {
            mTestThreads[i] = new BenchThread(i);
            mTestThreads[i].start();
        }

        mElapseTime[0] = 0;
        long start = System.currentTimeMillis();
        try {
            if (!allocTrace(0))
                Log.i(mTag, "Error in thread-0");
        } catch (OutOfMemoryError e) {
            mOutOfMemory = true;
                Log.i(mTag, "Thread-0 meets OutOfMemory");
            Message m = mActivity.getHandler().obtainMessage(
                                                      MainActivity.BenchmarkOutOfMemoryError);
            m.arg1 = 0;
            mActivity.getHandler().sendMessage(m);
        }
        mElapseTime[0] = System.currentTimeMillis() - start;

        for (int i = 1; i < mThreadNum; i++) {
            try {
                mTestThreads[i].join();
            } catch (InterruptedException e) {
                Log.i(mTag, "Waiting thread " + i + " finish interrupted by "
                        + e.getLocalizedMessage());
            }
        }
        SystemClock.sleep(1000);
        mWorkloadComplete = true;
        Runtime.getRuntime().runFinalization();
        Runtime.getRuntime().gc();
        try {
            logcat.join();
        } catch (InterruptedException e) {
            Log.i(mTag, "Waiting logcat finish interrupted by " + e.getLocalizedMessage());
        }
        if (mOutOfMemory) {
            try {
                if (fileOutput != null) {
                    fileOutput.append("\nOutOfMemory! Please config proifle or "
                                       + "Java Runtime and run again!\n");
                    fileOutput.flush();
                    fileOutput.close();
                }
            } catch (IOException e) {
                Log.i(mTag, "Cannot write to /sdcard/AndroidGCTest-result.txt"
                      + e.getMessage());
            }
        } else {
            mTotalGcCount = getRuntimeStat("art.gc.gc-count") - mTotalGcCount;
            mTotalGcTime = getRuntimeStat("art.gc.gc-time") - mTotalGcTime;
            mTotalBlockingGcCount = getRuntimeStat("art.gc.blocking-gc-count")
                - mTotalBlockingGcCount;
            mTotalBlockingGcTime = getRuntimeStat("art.gc.blocking-gc-time")
                - mTotalBlockingGcTime;
            long maxTime = 0;
            String completionTime = "";
            for (int i = 0; i < mThreadNum; i++) {
                if (maxTime < mElapseTime[i])
                    maxTime = mElapseTime[i];
                Log.i(mTag, "Thread-" + i + " completion time: "
                      + String.valueOf((mElapseTime[i])) + "ms");
                completionTime += "Thread-" + i + " completion time: "
                    + String.valueOf((mElapseTime[i])) + "ms\n";
            }
            String totalTime = "AndroidGCTest is done by " + mThreadNum
                + " threads. Completion time is " + maxTime + "ms";
            Log.i(mTag, totalTime);
            completionTime += totalTime + "\n";
            String gcDesc = "Total GC count: " + mTotalGcCount;
            gcDesc += "\nTotal GC time: " + mTotalGcTime + "ms";
            gcDesc += "\nTotal Blocking GC count: " + mTotalBlockingGcCount;
            gcDesc += "\nTotal Blocking GC time: " + mTotalBlockingGcTime + "ms";
            Log.i(mTag, "Total GC count: " + mTotalGcCount);
            Log.i(mTag, "Total GC time: " + mTotalGcTime + "ms");
            Log.i(mTag, "Total Blocking GC count: " + mTotalBlockingGcCount);
            Log.i(mTag, "Total Blocking GC time: " + mTotalBlockingGcTime + "ms");
            String pauseDesc = "";
            String[] causes;
            if (isArt)
                causes = GC_CAUSE_ART;
            else
                causes = GC_CAUSE_DALVIK;
            for (int i = 0; i < causes.length; i++) {
                Log.i(mTag, mGcCauseCount[i] + " GCs for " + causes[i] + " from logcat");
                gcDesc += "\n" + mGcCauseCount[i] + " GCs for " + causes[i] + " from logcat";
            }
            for (int i = 0; i < mGcPauseTime.length; i++) {
                String gc_time = "GC-" + i + ": " + mGcCause[i] + ", pause "
                    + mGcPauseTime[i] + "ms, total " + mGcTotalTime[i] + "ms";
                pauseDesc += "\n" + gc_time;
                Log.i(mTag, gc_time);
            }
            try {
                if (fileOutput != null) {
                    fileOutput.append("\n" + "Device config:\n\t" + deviceDesc);
                    fileOutput.append("\n" + completionTime);
                    fileOutput.append("\n" + "Heap status after each iteration "
                                      + "(footprint, bytes allocated):\n");
                    for (int j = 0; j < mIterNum; j++)
                        fileOutput.append("\t" + mHeapFootprint[j] + " kB, "
                                          + mHeapBytesAllocated[j] + " kB\n");
                    fileOutput.append(gcDesc);
                    fileOutput.append(pauseDesc);
                    fileOutput.flush();
                    fileOutput.close();
                }
            } catch (IOException e) {
                Log.i(mTag, "Cannot write to /sdcard/AndroidGCTest-result.txt"
                      + e.getMessage());
            }
            if (mActivity != null) {
                Message m = mActivity.getHandler().obtainMessage(
                                                          MainActivity.BenchmarkDone);
                Bundle b = new Bundle();
                b.putBoolean(ResultActivity.KEY_VM_TYPE, isArt);
                b.putString("device", deviceDesc);
                b.putLongArray(ResultActivity.KEY_THREAD_COMPLETE_TIME, mElapseTime);
                b.putLong(ResultActivity.KEY_WORKLOAD_COMPLETE_TIME, maxTime);
                b.putLongArray(ResultActivity.KEY_BYTES_LIVING, mHeapBytesAllocated);
                b.putIntArray(ResultActivity.KEY_HEAP_FOOTPRINT, mHeapFootprint);
                b.putFloatArray(ResultActivity.KEY_GC_PAUSE_TIME, mGcPauseTime);
                b.putFloatArray(ResultActivity.KEY_GC_COMPLETION_TIME, mGcTotalTime);
                b.putStringArray(ResultActivity.KEY_GC_CAUSE, mGcCause);
                m.setData(b);
                mActivity.getHandler().sendMessage(m);
            }
          }
        mElapseTime = null;
        mHeapFootprint = null;
        mHeapBytesAllocated = null;
        mGcPauseTime = null;
        mGcTotalTime = null;
        mGcCause = null;

        clearInitData();
    }

    public void stop() {
    }

    private void clearLogcat() {
        try {
            Process process = Runtime.getRuntime().exec("logcat -c");
            try {
                process.waitFor();
            } catch (InterruptedException e) {
                Log.e(mTag, "Clear logcat fails, interrupted by " + e.getLocalizedMessage());
            }
            Log.i(mTag, "Clear logcat before workload running");
        } catch (Exception e) {
            Log.e(mTag, "Clear logcat fails, " + e.getLocalizedMessage());
        }
    }

    private void readLogcat() {
        String cmd;
        if (isArt)
            cmd = "logcat -s art";
        else
            cmd = "logcat -s dalvikvm";
        ArrayList<Float> gcPauseTimeList = new ArrayList<Float>();
        ArrayList<Float> gcTotalTimeList = new ArrayList<Float>();
        ArrayList<String> gcCauseList = new ArrayList<String>();
        int loggerGcCount = 0;
        try {
            Process process = Runtime.getRuntime().exec(cmd);
            InputStream inStream = process.getInputStream();
            InputStream errorStream = process.getErrorStream();
            int error = errorStream.available();
            if (error > 0) {
                byte[] errorMsg = new byte[error];
                errorStream.read(errorMsg);
                Log.i(mTag, "executing logcat return error message: " + new String(errorMsg));
            }
            BufferedReader inReader = new BufferedReader(new InputStreamReader(inStream));
            String line;
            while (!mWorkloadComplete && (line = inReader.readLine()) != null) {
                if (mWorkloadComplete)
                    break;

                int idx = line.indexOf(": ");
                if (idx == -1)
                    continue;
                line = line.substring(idx+2);
                boolean isGCLog = false;
                String gcCause = "";
                if (isArt) {
                    for (int i = 0; i < GC_CAUSE_ART.length; i++) {
                        if ((line.contains("mark sweep") || line.contains("marksweep")
                             || line.contains("mark compact"))
                            && line.startsWith(GC_CAUSE_ART[i])) {
                            isGCLog = true;
                            mGcCauseCount[i]++;
                            idx = line.indexOf(" GC ");
                            if (idx > 0)
                                gcCause = line.substring(0, idx);
                            else {
                                Log.i(mTag, "Error: cannot find ' GC ' from this log, " + line);
                                continue;
                            }
                            break;
                        }
                    }
                } else {
                    for (int i = 0; i < GC_CAUSE_DALVIK.length; i++) {
                        if (line.startsWith(GC_CAUSE_DALVIK[i])) {
                            isGCLog = true;
                            mGcCauseCount[i]++;
                            gcCause = GC_CAUSE_DALVIK[i];
                            break;
                        }
                    }
                }
                if (!isGCLog)
                    continue;
                loggerGcCount++;

                int idx0 = line.indexOf("paused ");
                int idx1 = line.indexOf(" total ");
                if (idx0 == -1 || idx1 == -1) {
                    Log.i(mTag, "Cannot find pause or total completion time from the GC log "
                          + line);
                    continue;
                }

                String pauseTimeStr = line.substring(idx0+7, idx1);
                String totalTimeStr = line.substring(idx1+7);
                float completeTime = 0.0f;
                float pauseTime = 0.0f;
                idx = totalTimeStr.indexOf("ms");
                int timeToSec = 1000;
                if (idx == -1) {
                    idx = totalTimeStr.indexOf("us");
                    timeToSec = 1000000;
                }    
                if (idx == -1) {
                   idx = totalTimeStr.indexOf("s");
                   timeToSec = 1;
                }
                if (idx == -1) {
                    Log.i(mTag, "Cannot identify total complete time format, " + line);
                    continue;
                }
                completeTime = Float.parseFloat(totalTimeStr.substring(0, idx)) * 1000 / timeToSec;
                String[] pauseTimes = pauseTimeStr.split("\\+\\s*|,\\s*");
                for (int i = 0; i < pauseTimes.length; i++) {
                    String tmp = pauseTimes[i];
                    idx = tmp.indexOf("ms");
                    if (idx > 0) {
                        pauseTime += Float.parseFloat(tmp.substring(0, idx));
                    } else {
                        idx = tmp.indexOf("us");
                        if (idx > 0) {
                            pauseTime += Float.parseFloat(tmp.substring(0, idx))/1000;
                        } else {
                            Log.i(mTag, "Cannot identify pause time format, " + line);
                            continue;
                        }
                    }
                }
                gcPauseTimeList.add(pauseTime);
                gcTotalTimeList.add(completeTime);
                gcCauseList.add(gcCause);
            }
            errorStream.close();
            inReader.close();
            process.destroy();
            Log.i(mTag, "workload complete, stop reading logcat");
            mGcTotalTime = new float[loggerGcCount];
            mGcPauseTime = new float[loggerGcCount];
            mGcCause = new String[loggerGcCount];
            for (int i = 0; i < gcPauseTimeList.size(); i++) {
                mGcTotalTime[i] = gcTotalTimeList.get(i).floatValue();
                mGcPauseTime[i] = gcPauseTimeList.get(i).floatValue();
                mGcCause[i] = gcCauseList.get(i);
            }
        } catch (OutOfMemoryError e) {
              mOutOfMemory = true;
              Log.i(mTag, "logcat thread meets OutOfMemory");
              Message m = mActivity.getHandler().obtainMessage(
                                                        MainActivity.BenchmarkOutOfMemoryError);
              m.arg1 = -1;
              mActivity.getHandler().sendMessage(m);
          } catch (Exception e) {
            Log.i(mTag, "Cannot run logcat " + e.getMessage());
        }
        gcPauseTimeList.clear();
        gcPauseTimeList = null;
        gcTotalTimeList.clear();
        gcTotalTimeList = null;
        gcCauseList.clear();
        gcCauseList = null;
        Log.i(mTag, "logcat done");
    }

    private static Method getRuntimeStatMethod = null;
    private long getRuntimeStat(String statName) {
        if (getRuntimeStatMethod == null) {
            return 0;
        }
        String valueStr;
        try {
           valueStr = (String) getRuntimeStatMethod.invoke(null, statName);
        } catch (Exception e) {
            Log.w(mTag, "Failed to invoke getRuntimeStat");
            return 0;
        }
        if (valueStr != null)
            return Long.parseLong(valueStr);
        return 0;
    }
}

