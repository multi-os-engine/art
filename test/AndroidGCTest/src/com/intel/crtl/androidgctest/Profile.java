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

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import android.app.Activity;
import android.util.Log;
import android.util.Xml;

public class Profile {
      private final String mTag = "AndroidGCTest";
      public class ProfileData {
          private int mId;
          private String mName;
          private int mTotalSize;
          private int mBucketSize;
          private int mLosThreshold;
          private float[] mSizeDist;
          private float[] mLosElementDist;
          private float[][] mLifetime;
          private boolean mThreadMode;
          private int mThreadNum;
          private int mIterationNum;

          public ProfileData(int id, String name) {
              mId = id;
              mName = name;
          }
          public String getName() { return mName; }
          public int getTotalSize() { return mTotalSize; }
          public int getBucketSize() { return mBucketSize; }
          public int getLosThreshold() { return mLosThreshold; }
          public float[] getSizeDistribution() { return mSizeDist; }
          public float[] getLosElementDist() { return mLosElementDist; }
          public float[][] getLifetime() { return mLifetime; }
          public boolean getThreadMode() { return mThreadMode; }
          public int getThreadNum() { return mThreadMode? 1 : mThreadNum; }
          public int getIterationNum() { return mIterationNum; }

          public void setName(String name) { mName = name; }
          public void setTotalSize(int total_size) { mTotalSize = total_size; }
          public void setBucketSize(int bucket_size) { mBucketSize = bucket_size; }
          public void setLosThreshold(int los_threshold) { mLosThreshold = los_threshold; }
          public void setSizeDistribution(float[] size_dist) { mSizeDist = size_dist; }
          public void setLosElementDist(float[] los_element_dist) {
              mLosElementDist = los_element_dist;
          }
          public void setLifetime(float[][] lifetime) { mLifetime = lifetime; }
          public void setThreadMode(boolean singleThread) { mThreadMode = singleThread; }
          public void setThreadNum(int thread_num) {
              mThreadNum = mThreadMode? 1 : thread_num;
          }
          public void setIterationNum(int iteration_num) { mIterationNum = iteration_num; }
      }

    public boolean mInit = false;
    int mCurrentProfileId;
    public ArrayList<ProfileData> mData = null;

    private static Profile instance_;
    private Profile() {
        mCurrentProfileId = -1;
        mInit = false;
    }
    public static Profile getInstance() {
          if (instance_ == null)
                instance_ = new Profile();
          return instance_;
    }

    private boolean initProfileData(InputStream input) {
        ProfileData newProfile = null;
        XmlPullParser parser=Xml.newPullParser();
        try {
            parser.setInput(new InputStreamReader(input));
            int eventType = parser.getEventType();
            float[] floatValues = null;
            String itemName, name;
            int idx0 = -1, idx1 = 0;
            while (eventType != XmlPullParser.END_DOCUMENT) {
                switch(eventType) {
                case XmlPullParser.START_DOCUMENT:
                    mData = new ArrayList<ProfileData>();
                    break;
                case XmlPullParser.START_TAG:
                    name = parser.getName();
                    if (name.matches("profile")) {
                        newProfile = new ProfileData(
                                Integer.parseInt(parser.getAttributeValue(0)),
                                parser.getAttributeValue(1));
                    } else if (name.matches("item")) {
                        itemName = parser.getAttributeValue(0);
                        if (itemName.matches("total_size")) {
                            newProfile.mTotalSize = Integer.parseInt(parser.nextText());
                        } else if (itemName.matches("bucket_size")) {
                            newProfile.mBucketSize = Integer.parseInt(parser.nextText());
                        } else if (itemName.matches("los_threshold")) {
                            newProfile.mLosThreshold = Integer.parseInt(parser.nextText());
                        } else if (itemName.matches("thread-mode")) {
                            newProfile.mThreadMode = Integer.parseInt(parser.nextText()) == 1;
                        } else if (itemName.matches("thread-num")) {
                            newProfile.mThreadNum = Integer.parseInt(parser.nextText());
                        } else if (itemName.matches("iteration-times")) {
                            newProfile.mIterationNum = Integer.parseInt(parser.nextText());
                        }
                    } else if (name.matches("float-array-2d")) {
                        if ("lifetime".equals(parser.getAttributeValue(0))) {
                            newProfile.mLifetime = new float[Integer.parseInt(
                                                            parser.getAttributeValue(1))][];
                            idx0 = 0;
                        }
                    } else if(name.matches("float-array")) {
                        itemName = parser.getAttributeValue(0);
                        if (itemName.matches("size_dist")) {
                            newProfile.mSizeDist = new float[Integer.parseInt(
                                                            parser.getAttributeValue(1))];
                            floatValues = newProfile.mSizeDist;
                        } else if (itemName.matches("los_element_dist")) {
                            newProfile.mLosElementDist = new float[Integer.parseInt(
                                                            parser.getAttributeValue(1))];
                            floatValues = newProfile.mLosElementDist;
                        } else if (idx0 >= 0) {
                            newProfile.mLifetime[idx0] = new float[Integer.parseInt(
                                                                parser.getAttributeValue(1))];
                            floatValues = newProfile.mLifetime[idx0];
                            idx0++;
                        }
                        idx1 = 0;
                    } else if(name.matches("value")) {
                        if (floatValues == null) {
                            Log.i(mTag, "error in parsing profile.xml");
                            return false;
                        }
                        floatValues[idx1++] = Float.parseFloat(parser.nextText());
                    }
                    break;
                case XmlPullParser.END_TAG:
                    name = parser.getName();
                    if ("profile".equals(name))
                        mData.add(newProfile);
                    else if ("float-array-2d".equals(name))
                        idx0 = -1;
                    break;
                default:
                    break;
                }
                eventType = parser.next();
            }
        } catch (XmlPullParserException e) {
            e.printStackTrace();
            return false;
        } catch (NumberFormatException e) {
            e.printStackTrace();
            return false;
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        }
        return true;
    }

    public ProfileData getProfile(String name) {
        if (!mInit)
            return null;
        for (int i = 0; i < mData.size(); i++) {
            ProfileData data = mData.get(i);
            if (data.mName.matches(name)) {
                mCurrentProfileId = data.mId;
                return data;
            }
        }
        return null;
    }

    public boolean parseProfileData(InputStream rawProfileFile) {
          mInit = initProfileData(rawProfileFile);
          return mInit;
    }

    public boolean initialized() {
          return mInit;
    }

    public void setCurrentProfileData(String name) {
        if (!mInit)
            return;
        for (int i = 0; i < mData.size(); i++) {
            ProfileData data = mData.get(i);
            if (data.mName.matches(name)) {
                mCurrentProfileId = data.mId;
                break;
            }
        }
    }

    public ProfileData getCurrentProfileData() {
          if (!mInit)
                return null;
          for (int i = 0; i < mData.size(); i++)
                if (mData.get(i).mId == mCurrentProfileId)
                      return mData.get(i);
          return null;
    }
}
