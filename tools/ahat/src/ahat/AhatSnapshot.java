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

import com.android.tools.perflib.heap.*;
import com.google.common.collect.Iterables;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A wrapper over the perflib snapshot that provides the behavior we use in
 * ahat.
 */
class AhatSnapshot {
  private Snapshot mSnapshot;
  private List<Heap> mHeaps;

  // Map from Instance to the list of Instances it immediately dominates.
  private Map<Instance, List<Instance>> mDominated;

  private Site mRootSite;
  private Map<String, Site> mSites;
  private Map<Long, Site> mSitesByObjectId;

  private Map<Heap, Long> mHeapSizes;

  public AhatSnapshot(Snapshot snapshot) {
    mSnapshot = snapshot;
    mHeaps = new ArrayList<Heap>(mSnapshot.getHeaps());
    mDominated = new HashMap<Instance, List<Instance>>();
    mRootSite = new Site(null, "ROOT");
    mSitesByObjectId = new HashMap<Long, Site>();
    mHeapSizes = new HashMap<Heap, Long>();

    long javaLangClassId = 0;
    ClassObj javaLangClass = mSnapshot.findClass("java.lang.Class");
    if (javaLangClass != null) {
      javaLangClassId = javaLangClass.getId();
    } else {
      System.err.println("Warning: Unable to find java.lang.Class object");
    }

    for (Heap heap : mHeaps) {
      long total = 0;
      for (Instance inst : Iterables.concat(heap.getClasses(), heap.getInstances())) {
        Instance dominator = inst.getImmediateDominator();
        if (dominator != null) {
          total += inst.getSize();

          // Properly label the class of a class object.
          if (inst.getClassObj() == null) {
            if (inst instanceof ClassObj) {
              inst.setClassId(javaLangClassId);
            } else {
              // TODO: give the instance a dummy 'Unknown' class so that
              // getClassObj will not return null.
              System.err.println("Warning: Unknown class for " + inst);
            }
          }

          // Update dominated instances.
          List<Instance> instances = mDominated.get(dominator);
          if (instances == null) {
            instances = new ArrayList<Instance>();
            mDominated.put(dominator, instances);
          }
          instances.add(inst);

          // Update sites.
          List<String> path = new ArrayList<String>();

          // TODO: Uncomment the following code when perflib is updated to
          // support calling Instance.getStack and StackFrame.getFrames()
          //StackTrace stack = inst.getStack();
          //if (stack != null && stack.getFrames().length > 0) {
          //  StackFrame[] frames = stack.getFrames();
          //  for (int i = 0; i < frames.length; i++) {
          //    path.add(frames[frames.length - i - 1].toString());
          //  }
          //}
          mSitesByObjectId.put(inst.getId(), mRootSite.add(path.iterator(), inst));
        }
      }
      mHeapSizes.put(heap, total);
    }
    mSites = new HashMap<String, Site>();
    mapSites(mRootSite);
  }

  // Add the given site and all of its children to the mSites map.
  private void mapSites(Site site) {
    mSites.put(site.getId(), site);
    for (Site child : site.getChildren()) {
      mapSites(child);
    }
  }

  public Instance findInstance(long id) {
    return mSnapshot.findInstance(id);
  }

  public int getHeapIndex(Heap heap) {
    return mSnapshot.getHeapIndex(heap);
  }

  public Heap getHeap(String name) {
    return mSnapshot.getHeap(name);
  }

  public Collection<RootObj> getGCRoots() {
    return mSnapshot.getGCRoots();
  }

  public List<Heap> getHeaps() {
    return mHeaps;
  }

  public Site getRootSite() {
    return mRootSite;
  }

  public Site getSiteById(String id) {
    return mSites.get(id);
  }

  /**
   * Look up the site at which the given object was allocated.
   */
  public Site getSiteByObjectId(long id) {
    return mSitesByObjectId.get(id);
  }

  /**
   * Return a list of those objects immediately dominated by the given
   * instance.
   */
  public List<Instance> getDominated(Instance inst) {
    return mDominated.get(inst);
  }

  /**
   * Return the total size of reachable objects allocated on the given heap.
   */
  public long getHeapSize(Heap heap) {
    return mHeapSizes.get(heap);
  }
}

