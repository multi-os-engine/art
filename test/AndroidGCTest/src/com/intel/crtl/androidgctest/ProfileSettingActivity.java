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

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;

public class ProfileSettingActivity extends Activity {
      private final String mTag = "AndroidGCTest";
      private Button mConfirmButton = null;

      @Override
      protected void onCreate(Bundle savedInstanceState) {
          super.onCreate(savedInstanceState);
          setContentView(R.layout.activity_profile_config);
          mConfirmButton = (Button)findViewById(R.id.button_confirm);
          mConfirmButton.setOnClickListener(settingProfile);
          Profile profile = Profile.getInstance();
          showProfileData(profile.getCurrentProfileData());
      }
      private View.OnClickListener settingProfile = new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                  // TODO Auto-generated method stub
                  int heapSize, bucketSize, largeObjectSize;
            float[] sizeDist = null;
            float[][] lifetimeDist = null;
            float[] largeSizeDist = null;
            boolean singleThread = false;
            int thread_num = 0, exe_time = 1;
            heapSize = Integer.parseInt(((EditText)findViewById(
                                                    R.id.total_size_v)).getText().toString());
            bucketSize = Integer.parseInt(((EditText)findViewById(
                                                    R.id.bucket_size_v)).getText().toString());
            largeObjectSize = Integer.parseInt(((EditText)findViewById(
                                                    R.id.los_threshold_v)).getText().toString());
            exe_time = Integer.parseInt(((EditText)findViewById(
                                                    R.id.exe_time_v)).getText().toString());
            thread_num = Integer.parseInt(((EditText)findViewById(
                                                    R.id.thread_num)).getText().toString());

            sizeDist = new float[AndroidGCTestMain.OBJECT_SIZE_TYPE];
            sizeDist[0] = Float.parseFloat(((EditText)findViewById(
                                                    R.id.size_dist_16_v)).getText().toString());
            sizeDist[1] = Float.parseFloat(((EditText)findViewById(
                                                    R.id.size_dist_32_v)).getText().toString());
            sizeDist[2] = Float.parseFloat(((EditText)findViewById(
                                                    R.id.size_dist_64_v)).getText().toString());
            sizeDist[3] = Float.parseFloat(((EditText)findViewById(
                                                    R.id.size_dist_128_v)).getText().toString());
            sizeDist[4] = Float.parseFloat(((EditText)findViewById(
                                                    R.id.size_dist_256_v)).getText().toString());
            sizeDist[5] = Float.parseFloat(((EditText)findViewById(
                                                    R.id.size_dist_512_v)).getText().toString());
            sizeDist[6] = Float.parseFloat(((EditText)findViewById(
                                                    R.id.size_dist_los_v)).getText().toString());

            String[][] lifetimeStr = new String[AndroidGCTestMain.OBJECT_SIZE_TYPE][];
            lifetimeDist = new float[AndroidGCTestMain.OBJECT_SIZE_TYPE][];
            lifetimeStr[AndroidGCTestMain.OBJECT_16_BYTE] = ((EditText)findViewById(
                                                                    R.id.lifetime_16_v))
                .getText().toString().replaceAll(", ", ",").split(",");
            lifetimeStr[AndroidGCTestMain.OBJECT_32_BYTE] = ((EditText)findViewById(
                                                                    R.id.lifetime_32_v))
                .getText().toString().replaceAll(", ", ",").split(",");
            lifetimeStr[AndroidGCTestMain.OBJECT_64_BYTE] = ((EditText)findViewById(
                                                                    R.id.lifetime_64_v))
                .getText().toString().replaceAll(", ", ",").split(",");
            lifetimeStr[AndroidGCTestMain.OBJECT_128_BYTE] = ((EditText)findViewById(
                                                                    R.id.lifetime_128_v))
                .getText().toString().replaceAll(", ", ",").split(",");
            lifetimeStr[AndroidGCTestMain.OBJECT_256_BYTE] = ((EditText)findViewById(
                                                                    R.id.lifetime_256_v))
                .getText().toString().replaceAll(", ", ",").split(",");
            lifetimeStr[AndroidGCTestMain.OBJECT_512_BYTE] = ((EditText)findViewById(
                                                                    R.id.lifetime_512_v))
                .getText().toString().replaceAll(", ", ",").split(",");
            lifetimeStr[AndroidGCTestMain.OBJECT_LARGE_BYTE] = ((EditText)findViewById(
                                                                    R.id.lifetime_los_v))
                .getText().toString().replaceAll(", ", ",").split(",");
            for (int i = 0; i < AndroidGCTestMain.OBJECT_SIZE_TYPE; i++) {
                int len = lifetimeStr[i].length;
                lifetimeDist[i] = new float[len];
                for (int j = 0; j < len; j++) {
                    lifetimeDist[i][j] = Float.parseFloat(lifetimeStr[i][j]);
                }
            }

            largeSizeDist = new float[4];
            largeSizeDist[0] = Float.parseFloat(((EditText)findViewById(
                                                            R.id.los_dist_byte_v))
                                                .getText().toString());
            largeSizeDist[1] = Float.parseFloat(((EditText)findViewById(
                                                            R.id.los_dist_char_v))
                                                .getText().toString());
            largeSizeDist[2] = Float.parseFloat(((EditText)findViewById(
                                                            R.id.los_dist_int_v))
                                                .getText().toString());
            largeSizeDist[3] = Float.parseFloat(((EditText)findViewById(
                                                            R.id.los_dist_long_v))
                                                .getText().toString());

            singleThread = ((CheckBox)findViewById(R.id.single_thread)).isChecked();
            Profile profile = Profile.getInstance();
            Profile.ProfileData profileData = profile.getCurrentProfileData();
            profileData.setTotalSize(heapSize);
            profileData.setBucketSize(bucketSize);
            profileData.setLosThreshold(largeObjectSize);
            profileData.setSizeDistribution(sizeDist);
            profileData.setLifetime(lifetimeDist);
            profileData.setLosElementDist(largeSizeDist);
            profileData.setThreadMode(singleThread);
            profileData.setThreadNum(thread_num);
            profileData.setIterationNum(exe_time);
            Intent intent = new Intent(getApplicationContext(), MainActivity.class);
            intent.addFlags(Intent.FLAG_ACTIVITY_BROUGHT_TO_FRONT);
            startActivity(intent);
            }
      };

      private void showProfileData(Profile.ProfileData data) {
        ((EditText)findViewById(R.id.total_size_v))
            .setText(String.valueOf(data.getTotalSize()));
        ((EditText)findViewById(R.id.bucket_size_v))
            .setText(String.valueOf(data.getBucketSize()));
        ((EditText)findViewById(R.id.los_threshold_v))
            .setText(String.valueOf(data.getLosThreshold()));

        int i = 0;
        float[] size_dist = data.getSizeDistribution();
        ((EditText)findViewById(R.id.size_dist_16_v)).setText(String.valueOf(size_dist[i++]));
        ((EditText)findViewById(R.id.size_dist_32_v)).setText(String.valueOf(size_dist[i++]));
        ((EditText)findViewById(R.id.size_dist_64_v)).setText(String.valueOf(size_dist[i++]));
        ((EditText)findViewById(R.id.size_dist_128_v)).setText(String.valueOf(size_dist[i++]));
        ((EditText)findViewById(R.id.size_dist_256_v)).setText(String.valueOf(size_dist[i++]));
        ((EditText)findViewById(R.id.size_dist_512_v)).setText(String.valueOf(size_dist[i++]));
        ((EditText)findViewById(R.id.size_dist_los_v)).setText(String.valueOf(size_dist[i++]));

        i = 0;
        float[] los_element_dist = data.getLosElementDist();
        ((EditText)findViewById(R.id.los_dist_byte_v))
            .setText(String.valueOf(los_element_dist[i++]));
        ((EditText)findViewById(R.id.los_dist_char_v))
            .setText(String.valueOf(los_element_dist[i++]));
        ((EditText)findViewById(R.id.los_dist_int_v))
            .setText(String.valueOf(los_element_dist[i++]));
        ((EditText)findViewById(R.id.los_dist_long_v))
            .setText(String.valueOf(los_element_dist[i++]));

        String[] t = new String[]{"", "", "", "", "", "", ""};
        float[][] lifetime = data.getLifetime();
        for (i = 0; i < AndroidGCTestMain.OBJECT_SIZE_TYPE; i++) {
            for (int j = 0; j < lifetime[i].length; j++) {
                if (j != 0)
                    t[i] += ",";
                t[i] += String.valueOf(lifetime[i][j]);
            }
        }
        i = 0;
        ((EditText)findViewById(R.id.lifetime_16_v)).setText(t[i++]);
        ((EditText)findViewById(R.id.lifetime_32_v)).setText(t[i++]);
        ((EditText)findViewById(R.id.lifetime_64_v)).setText(t[i++]);
        ((EditText)findViewById(R.id.lifetime_128_v)).setText(t[i++]);
        ((EditText)findViewById(R.id.lifetime_256_v)).setText(t[i++]);
        ((EditText)findViewById(R.id.lifetime_512_v)).setText(t[i++]);
        ((EditText)findViewById(R.id.lifetime_los_v)).setText(t[i++]);

        ((CheckBox)findViewById(R.id.single_thread)).setChecked(data.getThreadMode());

        EditText thread_num_view = (EditText)findViewById(R.id.thread_num);
        thread_num_view.setText(String.valueOf(data.getThreadNum()));
        ((EditText)findViewById(R.id.exe_time_v))
            .setText(String.valueOf(data.getIterationNum()));
    }
}
