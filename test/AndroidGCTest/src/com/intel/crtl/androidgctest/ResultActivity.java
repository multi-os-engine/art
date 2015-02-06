
/*
 * Copyright (C) 2015, Intel Corporation, all rights reserved.
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

import org.achartengine.ChartFactory;
import org.achartengine.GraphicalView;
import org.achartengine.chart.PointStyle;
import org.achartengine.model.XYMultipleSeriesDataset;
import org.achartengine.model.XYSeries;
import org.achartengine.renderer.XYMultipleSeriesRenderer;
import org.achartengine.renderer.XYSeriesRenderer;

import android.util.Log;
import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.LinearLayout.LayoutParams;

public class ResultActivity extends Activity {
      private final String mTag = "AndroidGCTest";
      private long[] mThreadExeTime = null;
      private long mWorkloadExeTime = 0;
      private int[] mHeapFootprint = null;
      private long[] mHeapBytesAllocated = null;
      private float[] mGcPauseTime = null;
      private float[] mGcTotalTime = null;
      private String[] mGcCause = null;
      private boolean mVmType; //true for ART, false for Dalvik
      protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            setContentView(R.layout.activity_result);

            Bundle b = getIntent().getBundleExtra("result_data");
            if (b == null) {
                  Log.i(mTag, "no result_data bundle!");
            }

            if (b.containsKey("vm_type")) {
                  mVmType = b.getBoolean("vm_type");
            } else {
                  Log.i(mTag, "no vm_type");
            }

            if (b.containsKey("workload_complete_time")) {
                  mWorkloadExeTime = b.getLong("workload_complete_time");
                  if (mWorkloadExeTime == 0) {
                        Log.i(mTag, "cannot read workload_complete_time");
                  }
            } else {
                  Log.i(mTag, "no workload_complete_time");
            }

            if (b.containsKey("thread_complete_time")) {
                  mThreadExeTime = b.getLongArray("thread_complete_time");
                  if (mThreadExeTime == null) {
                        Log.i(mTag, "cannot read workload_complete_time");
                  }
            } else {
                  Log.i(mTag, "no thread_complete_time");
            }

            if (b.containsKey("heap_footprint")) {
                  mHeapFootprint = b.getIntArray("heap_footprint");
                  if (mHeapFootprint == null) {
                        Log.i(mTag, "cannot read heap_footprint");
                  }
            } else {
                  Log.i(mTag, "no heap_footprint");
            }

            if (b.containsKey("bytes_allocated")) {
                  mHeapBytesAllocated = b.getLongArray("bytes_allocated");
                  if (mHeapBytesAllocated == null) {
                        Log.i(mTag, "cannot read bytes_allocated");
                  }
            } else {
                  Log.i(mTag, "no bytes_allocated");
            }

            if (b.containsKey("gc_pause")) {
                  mGcPauseTime = b.getFloatArray("gc_pause");
                  if (mGcPauseTime == null) {
                        Log.i(mTag, "cannot read gc_pause");
                  }
            } else {
                  Log.i(mTag, "no gc_pause");
            }

            if (b.containsKey("gc_total")) {
                  mGcTotalTime = b.getFloatArray("gc_total");
                  if (mGcTotalTime == null) {
                        Log.i(mTag, "cannot read gc_total");
                  }
            } else {
                  Log.i(mTag, "no gc_total");
            }

            if (b.containsKey("gc_cause")) {
                  mGcCause = b.getStringArray("gc_cause");
                  if (mGcCause == null) {
                        Log.i(mTag, "cannot read gc_cause");
                  }
            } else {
                  Log.i(mTag, "no gc_cause");
            }

            TextView vm_type = (TextView)findViewById(R.id.vm_type);
            vm_type.setText("Current runtime engine is " + (mVmType? "ART" : "Dalvik")
                        + ", workload completes in " + mWorkloadExeTime + " ms");
            Button exeTimeButton = (Button)findViewById(R.id.button_exe_time);
            exeTimeButton.setClickable(true);
            exeTimeButton.setOnClickListener(new View.OnClickListener() {
                  @Override
                  public void onClick(View v) {
                        // TODO Auto-generated method stub
                        mCurrentDataset.clear();
                        mCurrentRenderer.removeAllRenderers();
                        buildExeTimeInfoChart();
                        mChartView.repaint();
                  }
            });

            Button heapButton = (Button)findViewById(R.id.button_heap);
            heapButton.setClickable(true);
            heapButton.setOnClickListener(new View.OnClickListener() {
                  @Override
                  public void onClick(View v) {
                        // TODO Auto-generated method stub
                        mCurrentDataset.clear();
                        mCurrentRenderer.removeAllRenderers();
                        buildHeapInfoChart();
                        mChartView.repaint();
                  }
            });

            Button gcButton = (Button)findViewById(R.id.button_gc);
            gcButton.setClickable(true);
            gcButton.setOnClickListener(new View.OnClickListener() {
                  @Override
                  public void onClick(View v) {
                        // TODO Auto-generated method stub
                        //int c = mCurrentDataset.getSeriesCount();
                        //for (int i = 0; i < c; i++)
                        //      mCurrentDataset.removeSeries(0);
                        mCurrentDataset.clear();
                        mCurrentRenderer.removeAllRenderers();
                        buildGcInfoChart();
                        mChartView.repaint();
                  }
            });
            mCurrentRenderer.setApplyBackgroundColor(true);
            mCurrentRenderer.setBackgroundColor(Color.argb(100, 50, 50, 50));
            mCurrentRenderer.setAxisTitleTextSize(16);
            mCurrentRenderer.setChartTitleTextSize(20);
            mCurrentRenderer.setLabelsTextSize(15);
            mCurrentRenderer.setLegendTextSize(15);
            mCurrentRenderer.setMargins(new int[] { 20, 30, 15, 0 });
            mCurrentRenderer.setZoomButtonsVisible(true);
            mCurrentRenderer.setPointSize(5);
      }
      private XYMultipleSeriesDataset mCurrentDataset = new XYMultipleSeriesDataset();
      private XYMultipleSeriesRenderer mCurrentRenderer = new XYMultipleSeriesRenderer();

      private GraphicalView mChartView;
      private void buildExeTimeInfoChart() {
            Log.i(mTag, "buildExeTimeInfoChart");
        //workload execution time
            XYSeries series = new XYSeries("Workload Execution Time");
            for (int i = 0; i < mThreadExeTime.length; i++) {
                  series.add(i, mThreadExeTime[i]);
            }
            mCurrentDataset.addSeries(series);
            XYSeriesRenderer renderer = new XYSeriesRenderer();
            // set some renderer properties
            renderer.setPointStyle(PointStyle.CIRCLE);
            renderer.setFillPoints(true);
            renderer.setDisplayChartValues(true);
            renderer.setDisplayChartValuesDistance(10);
            renderer.setLineWidth(2.0f);
            mCurrentRenderer.addSeriesRenderer(renderer);
            mCurrentRenderer.setChartTitle("Workload Execution Time (ms)");
            mCurrentRenderer.setYAxisMin(0);
            mCurrentRenderer.setYAxisMax(mWorkloadExeTime*1.05);
            mCurrentRenderer.setXAxisMin(0);
            mCurrentRenderer.setXAxisMax(mThreadExeTime.length);
      }

      private void buildHeapInfoChart() {
            Log.i(mTag, "buildHeapInfoChart");
        //Heap footprint and bytes_allocated
            double max = 0d;
            XYSeries series2 = new XYSeries("Heap Footprint");
            XYSeries series3 = new XYSeries("Bytes Allocated");
            for (int i = 0; i < mHeapFootprint.length; i++) {
                  series2.add(i, mHeapFootprint[i]);
                  series3.add(i, mHeapBytesAllocated[i]);
                  if (max < mHeapFootprint[i])
                        max = mHeapFootprint[i];
                  if (max < mHeapBytesAllocated[i])
                        max = mHeapBytesAllocated[i];
            }
            mCurrentDataset.addSeries(series2);
            mCurrentDataset.addSeries(series3);
            XYSeriesRenderer renderer = new XYSeriesRenderer();
            // set some renderer properties
            renderer.setPointStyle(PointStyle.CIRCLE);
            renderer.setFillPoints(true);
            renderer.setDisplayChartValues(false);
            renderer.setDisplayChartValuesDistance(10);
            renderer.setColor(Color.RED);
            renderer.setLineWidth(1.0f);
            mCurrentRenderer.addSeriesRenderer(renderer);
            renderer = new XYSeriesRenderer();
            // set some renderer properties
            renderer.setPointStyle(PointStyle.CIRCLE);
            renderer.setFillPoints(true);
            renderer.setDisplayChartValues(false);
            renderer.setDisplayChartValuesDistance(10);
            renderer.setColor(Color.BLUE);
            renderer.setLineWidth(1.0f);
            mCurrentRenderer.addSeriesRenderer(renderer);
            mCurrentRenderer.setChartTitle("Heap Footprint/Bytes allocated (kB)");
            mCurrentRenderer.setYAxisMin(0);
            mCurrentRenderer.setYAxisMax(max*1.05);
            mCurrentRenderer.setXAxisMin(0);
            mCurrentRenderer.setXAxisMax(mHeapFootprint.length);
      }

      private void buildGcInfoChart() {
            Log.i(mTag, "buildGcInfoChart");
            //GC status
            //XYSeries series2 = new XYSeries("GC total time");
            XYSeries series3 = new XYSeries("GC pause time");
            double max = 0d;
            for (int i = 0; i < mGcPauseTime.length; i++) {
                  //series2.add(i, mGcTotalTime[i]);
                  series3.add(i, mGcPauseTime[i]);
                  if (max < mGcPauseTime[i])
                      max = mGcPauseTime[i];
            }
            //mCurrentDataset.addSeries(series2);
            mCurrentDataset.addSeries(series3);
            XYSeriesRenderer renderer = new XYSeriesRenderer();
            // set some renderer properties
            renderer.setPointStyle(PointStyle.CIRCLE);
            renderer.setFillPoints(true);
            renderer.setDisplayChartValues(false);
            renderer.setDisplayChartValuesDistance(10);
            renderer.setColor(Color.BLUE);
            renderer.setLineWidth(1.5f);
            mCurrentRenderer.addSeriesRenderer(renderer);
            mCurrentRenderer.setChartTitle("Gc Pause Time (ms)");
            mCurrentRenderer.setYAxisMin(0);
            mCurrentRenderer.setYAxisMax(max*1.05);
            mCurrentRenderer.setXAxisMin(0);
            mCurrentRenderer.setXAxisMax(mGcPauseTime.length);
      }

      @Override
      protected void onResume() {
          super.onResume();
          if (mChartView == null) {
              buildExeTimeInfoChart();
              LinearLayout layout = (LinearLayout) findViewById(R.id.chart);
              mChartView = ChartFactory.getLineChartView(this, mCurrentDataset, mCurrentRenderer);
              layout.addView(mChartView, new LayoutParams(LayoutParams.WRAP_CONTENT,
              LayoutParams.WRAP_CONTENT));
          } else {
              mChartView.repaint();
          }
      }
      @Override
      public boolean onKeyUp(int keyCode, KeyEvent event) {
            if (keyCode == KeyEvent.KEYCODE_BACK) {
                mCurrentDataset = null;
                mCurrentRenderer = null;
                mThreadExeTime = null;
                mWorkloadExeTime = 0;
                mHeapFootprint = null;
                mHeapBytesAllocated = null;
                mGcPauseTime = null;
                mGcTotalTime = null;
                mGcCause = null;
                Runtime.getRuntime().runFinalization();
                Runtime.getRuntime().gc();
            }
            return super.onKeyUp(keyCode, event);
      }
}
