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
package com.intel.gameworkload;

import java.lang.reflect.Method;
import org.cocos2d.layers.CCScene;
import org.cocos2d.nodes.CCDirector;
import org.cocos2d.opengl.CCGLSurfaceView;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.Window;
import android.view.WindowManager;

public class MainActivity extends Activity {

    protected CCGLSurfaceView _glSurfaceView;
    private Handler handler;

    private class MyTask implements Runnable
    {
        private Activity mA;
        public MyTask(Activity a)
        {
            mA = a;
        }
        @Override
        public void run() {
            // TODO Auto-generated method stub
            float AFPS = CCDirector.sharedDirector().getAFPS();
            float Time = CCDirector.sharedDirector().gettotalTime();
            int nRocks = CCDirector.sharedDirector().getnRocks();
            int nSnows = CCDirector.sharedDirector().getnSnows();
            Log.d("GameWorkload", "final result: AFPS="+AFPS+",Time="+Time+",ROCK="+nRocks+",SNOW="+nSnows);
            Intent resultData = new Intent();
            resultData.putExtra("nrocks", nRocks);
            resultData.putExtra("nsnows", nSnows);
            resultData.putExtra("afps", AFPS);
            resultData.putExtra("time", Time);
            setResult(Activity.RESULT_OK, resultData);
            mA.finish();
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
            super.onCreate(savedInstanceState);

            requestWindowFeature(Window.FEATURE_NO_TITLE);
            getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
            getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON, WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

            _glSurfaceView = new CCGLSurfaceView(this);

            setContentView(_glSurfaceView);
    }

    private final static int defaultNRocks = 5;
    private final static int defaultNSnows = 10;
    private final static int defaultRockStep = 5;
    private final static int defaultSnowStep = 10;

    @Override
    public void onStart()
    {
            super.onStart();

            CCDirector.sharedDirector().attachInView(_glSurfaceView);

            CCDirector.sharedDirector().setDisplayFPS(true);

            CCDirector.sharedDirector().setAnimationInterval(1.0f / 60.0f);

            //portrait mode has some problem!
            CCDirector.sharedDirector().setLandscape(true);

            CCDirector.sharedDirector().setDisplayJank(false);

            int nRocks = defaultNRocks;
            int nSnows = defaultNSnows;
            int rockStep = defaultRockStep;
            int snowStep = defaultSnowStep;
            int execTime = 0;
            int mode = 0;

            Bundle extras = this.getIntent().getExtras();
            if(extras!=null && extras.containsKey("nrocks"))
            {
                nRocks = Integer.parseInt(extras.getString("nrocks"));
            }

            if(extras!=null && extras.containsKey("nsnows"))
            {
                nSnows = Integer.parseInt(extras.getString("nsnows"));
            }

            if(extras!=null && extras.containsKey("snowstep"))
            {
                snowStep = Integer.parseInt(extras.getString("snowstep"));
            }

            if(extras!=null && extras.containsKey("rockstep"))
            {
                rockStep = Integer.parseInt(extras.getString("rockstep"));
            }

            if(extras!=null && extras.containsKey("transition"))
            {
                float t = Integer.parseInt(extras.getString("transition"));
                CCDirector.sharedDirector().setmaxTransition(t);
            }

            if(extras!=null && extras.containsKey("execTime"))
            {
                execTime = Integer.parseInt(extras.getString("execTime"));
            }

            if(extras!=null && extras.containsKey("mode"))
            {
                String m = extras.getString("mode");
                if(m.equals("benchmark"))
                {
                    mode = 1;
                }
            }

            String abi64 = "x86_64";
            String abi32 = "x86";
            String artlib = "libart.so";
            String dvmlib = "libdvm.so";
            int runtime = 0;

            String abi=getSysProp("ro.product.cpu.abi");
            if(abi==null || abi.length()==0)
            {
                abi="<unk>";
            }else
            {
                if(abi.equals(abi64))
                    runtime |= 0x01;
            }

            String vm=getSysProp("persist.sys.dalvik.vm.lib");
            if(vm==null || vm.length()==0)
            {
                vm = getSysProp("persist.sys.dalvik.vm.lib.2");
                if(vm==null || vm.length() == 0)
                {
                    vm = "<unk>";
                }
            }
            if(vm.equals(artlib))
                runtime |= 0x02;

            Log.d("GameWorkload", "abi:"+abi+" vm:"+vm);
            Log.d("GameWorkload", "Configure nrocks = "+nRocks+" nSnows = "+nSnows);

            if(execTime>0)
            {
                handler = new Handler();
                handler.postDelayed(new MyTask(this), execTime);
            }

            CCScene scene = GameLayer.scene(this, this.getAssets(), nRocks, nSnows, rockStep, snowStep, runtime, mode);
            CCDirector.sharedDirector().runWithScene(scene);
    }

    @Override
    public void onPause()
    {
            super.onPause();
            CCDirector.sharedDirector().pause();
            Log.d("GameWorkload", "Game paused");
    }

    @Override
    public void onResume()
    {
            super.onResume();
            CCDirector.sharedDirector().resetAFPS();
            CCDirector.sharedDirector().resume();
    }

    @Override
    public void onStop()
    {
        super.onStop();
        CCDirector.sharedDirector().end();
        Log.d("GameWorkload", "Game stopped");
    }

    public String getSysProp(String key) throws IllegalArgumentException {

        String ret= "";
        try{

            ClassLoader cl = getClassLoader();
            @SuppressWarnings("rawtypes")
            Class SystemProperties = cl.loadClass("android.os.SystemProperties");

            //Parameters Types
            @SuppressWarnings("rawtypes")
                Class[] paramTypes= new Class[1];
            paramTypes[0]= String.class;

            Method get = SystemProperties.getMethod("get", paramTypes);

            //Parameters
            Object[] params= new Object[1];
            params[0]= new String(key);

            ret= (String) get.invoke(SystemProperties, params);

          }catch( IllegalArgumentException iAE ){
              throw iAE;
          }catch( Exception e ){
              ret= "<exception>";
              //TODO
          }
        return ret;
    }

}
