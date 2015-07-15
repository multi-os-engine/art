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

import java.util.ArrayList;
import java.util.Collection;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

import com.android.tools.perflib.heap.*;
import com.google.common.collect.Iterables;

class Site {
  private Site mParent;
  private String mName;
  private Map<String, Long> mSizesByHeap;
  private Map<String, Site> mChildren;
  private List<Instance> mObjects;
  private List<ObjectsInfo> mObjectsInfos;
  private Map<Heap, Map<ClassObj, ObjectsInfo>> mObjectsInfoMap;

  public static class ObjectsInfo {
    public Heap heap;
    public ClassObj classObj;
    public long numInstances;
    public long numBytes;
  }

  public Site(Site parent, String name) {
    mParent = parent;
    mName = name;
    mSizesByHeap = new HashMap<String, Long>();
    mChildren = new HashMap<String, Site>();
    mObjects = new ArrayList<Instance>();
    mObjectsInfos = new ArrayList<ObjectsInfo>();
    mObjectsInfoMap = new HashMap<Heap, Map<ClassObj, ObjectsInfo>>();
  }

  /**
   * Returns the site at which the instance was allocated.
   */
  public Site add(Iterator<String> path, Instance inst) {
    mObjects.add(inst);

    String heap = inst.getHeap().getName();
    mSizesByHeap.put(heap, getSize(heap) + inst.getSize());

    Map<ClassObj, ObjectsInfo> classToObjectsInfo = mObjectsInfoMap.get(inst.getHeap());
    if (classToObjectsInfo == null) {
      classToObjectsInfo = new HashMap<ClassObj, ObjectsInfo>();
      mObjectsInfoMap.put(inst.getHeap(), classToObjectsInfo);
    }

    ObjectsInfo info = classToObjectsInfo.get(inst.getClassObj());
    if (info == null) {
      info = new ObjectsInfo();
      info.heap = inst.getHeap();
      info.classObj = inst.getClassObj();
      info.numInstances = 0;
      info.numBytes = 0;
      mObjectsInfos.add(info);
      classToObjectsInfo.put(inst.getClassObj(), info);
    }

    info.numInstances++;
    info.numBytes += inst.getSize();

    if (path.hasNext()) {
      String next = path.next();
      Site child = mChildren.get(next);
      if (child == null) {
        child = new Site(this, next);
        mChildren.put(next, child);
      }
      return child.add(path, inst);
    } else {
      return this;
    }
  }

  // Get the size of a site for a specific heap.
  public long getSize(String heap) {
    Long val = mSizesByHeap.get(heap);
    if (val == null) {
      return 0;
    }
    return val;
  }

  /**
   * Get the list of objects allocated under this site. Includes objects
   * allocated in children sites.
   */
  public Collection<Instance> getObjects() {
    return mObjects;
  }

  public List<ObjectsInfo> getObjectsInfos() {
    return mObjectsInfos;
  }

  // Get the combined size of the site for all heaps.
  public long getTotalSize() {
    long size = 0;
    for (Long val : mSizesByHeap.values()) {
      size += val;
    }
    return size;
  }

  /**
   * Return the site this site was called from.
   * Returns null for the root site.
   */
  public Site getParent() {
    return mParent;
  }

  public String getName() {
    return mName;
  }

  public String getId() {
    return String.format("%h", this);
  }

  List<Site> getChildren() {
    return new ArrayList<Site>(mChildren.values());
  }

  public static class CompareBySize implements Comparator<Site> {
    String mHeap;

    public CompareBySize(String heap) {
      mHeap = heap;
    }

    public int compare(Site a, Site b) {
      return Long.compare(b.getSize(mHeap), a.getSize(mHeap));
    }
  }
}

