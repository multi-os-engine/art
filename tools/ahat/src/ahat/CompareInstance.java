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

package ahat;

import java.util.Comparator;

import com.android.tools.perflib.heap.*;

class CompareInstance {
  public static class ById implements Comparator<Instance> {
    public int compare(Instance a, Instance b) {
      return Long.compare(a.getId(), b.getId());
    }
  }

  public static class ByTotalRetainedSize implements Comparator<Instance> {
    private Comparator<Instance> mFallback;

    public ByTotalRetainedSize(Comparator<Instance> fallback) {
      mFallback = fallback;
    }

    public int compare(Instance a, Instance b) {
      int res = Long.compare(b.getTotalRetainedSize(), a.getTotalRetainedSize());
      return res == 0 ? mFallback.compare(a, b) : res;
    }
  }

  public static class ByHeapRetainedSize implements Comparator<Instance> {
    private int mIndex;
    private Comparator<Instance> mFallback;

    /**
     * Sort instances in descending order of the retained size for the given
     * heap index.
     */
    public ByHeapRetainedSize(int heapIndex, Comparator<Instance> fallback) {
      mIndex = heapIndex;
      mFallback = fallback;
    }

    public int compare(Instance a, Instance b) {
      int res = Long.compare(b.getRetainedSize(mIndex), a.getRetainedSize(mIndex));
      return res == 0 ? mFallback.compare(a, b) : res;
    }
  }

  public static Comparator<Instance> getDefaultCompare(AhatSnapshot snapshot) {
    Comparator<Instance> compare = new ById();
    compare = new ByTotalRetainedSize(compare);

    // Priority goes to the app heap, if we can find one.
    int appHeapIdx = snapshot.getHeapIndex(snapshot.getHeap("app"));
    if (appHeapIdx >= 0) {
      compare = new ByHeapRetainedSize(appHeapIdx, compare);
    }
    return compare;
  }
}

