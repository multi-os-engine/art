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

package com.android.ahat;

import com.android.tools.perflib.heap.Heap;
import com.android.tools.perflib.heap.Instance;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Class for rendering a list of instances dominated by a single instance in a
 * pretty way.
 */
class DominatedList {
  private static final int kIncrAmount = 100;
  private static final int kDefaultShown = 100;

  private Query mQuery;
  private List<Instance> mInstances;
  private HtmlWriter mHtmlWriter;
  private AhatSnapshot mSnapshot;

  public DominatedList(AhatSnapshot snapshot, Collection<Instance> instances, Query query) {
    mQuery = query;
    mInstances = new ArrayList<Instance>(instances);
    mHtmlWriter = null;
    mSnapshot = snapshot;
  }

  /**
   * Render a table to the given HtmlWriter showing a pretty list of
   * instances.
   *
   * Rather than show all of the instances (which may be very many), we use
   * the query parameter "dominated" to specify a limited number of
   * instances to show. The 'uri' parameter should be the current page URI, so
   * that we can add links to "show more" and "show less" objects that go to
   * the same page with only the number of objects adjusted.
   */
  public void render(HtmlWriter html) {
    mHtmlWriter = html;
    List<Instance> insts = mInstances;
    Collections.sort(insts, Sort.defaultInstanceCompare(mSnapshot));

    int numInstancesToShow = getNumInstancesToShow();
    List<Instance> shown = new ArrayList<Instance>(insts.subList(0, numInstancesToShow));
    List<Instance> hidden = insts.subList(numInstancesToShow, insts.size());

    final Map<Heap, Long> hiddenSizes = new HashMap<Heap, Long>();
    for (Heap heap : mSnapshot.getHeaps()) {
      hiddenSizes.put(heap, 0l);
    }

    if (!hidden.isEmpty()) {
      for (Instance inst : hidden) {
        for (Heap heap : mSnapshot.getHeaps()) {
          int index = mSnapshot.getHeapIndex(heap);
          long size = inst.getRetainedSize(index);
          hiddenSizes.put(heap, hiddenSizes.get(heap) + size);
        }
      }

      // Add 'null' as a marker for "all the rest of the objects".
      shown.add(null);
    }

    HeapTable.TableConfig<Instance> table = new HeapTable.TableConfig<Instance>() {
      public String getHeapsDescription() {
        return "Bytes Retained by Heap";
      }

      public long getSize(Instance element, Heap heap) {
        if (element == null) {
          return hiddenSizes.get(heap);
        }
        int index = mSnapshot.getHeapIndex(heap);
        return element.getRetainedSize(index);
      }

      public List<HeapTable.ValueConfig<Instance>> getValueConfigs() {
        HeapTable.ValueConfig<Instance> value = new HeapTable.ValueConfig<Instance>() {
          public String getDescription() {
            return "Object";
          }

          public void render(HtmlWriter html, Instance element) {
            if (element == null) {
              html.println("%R", "...");
            } else {
              html.println("%I", element);
            }
          }
        };
        return Collections.singletonList(value);
      }
    };
    HeapTable.render(html, table, mSnapshot, shown);

    if (insts.size() > kDefaultShown) {
      printMenu(numInstancesToShow, insts.size());
    }
  }

  // Figure out how many objects to show based on the query parameter.
  // The resulting value is guaranteed to be at least zero, and no greater
  // than the number of total objects.
  private int getNumInstancesToShow() {
    String value = mQuery.get("dominated", null);
    try {
      int count = Math.min(mInstances.size(), Integer.parseInt(value));
      return Math.max(0, count);
    } catch (NumberFormatException e) {
      // We can't parse the value as a number. Ignore it.
    }
    return Math.min(kDefaultShown, mInstances.size());
  }

  // Print a menu line after the table to control how many objects are shown.
  // It has the form:
  //  (showing X of Y objects - show none - show less - show more - show all)
  private void printMenu(int shown, int all) {
    HtmlWriter html = mHtmlWriter;
    html.print("(%d of %d objects shown - ", shown, all);
    if (shown > 0) {
      int less = Math.max(0, shown - kIncrAmount);
      html.print("<a href=\"%s\">show none</a> - ", mQuery.with("dominated", 0));
      html.print("<a href=\"%s\">show less</a> - ", mQuery.with("dominated", less));
    } else {
      html.print("%R", "show none - show less - ");
    }
    if (shown < all) {
      int more = Math.min(shown + kIncrAmount, all);
      html.print("<a href=\"%s\">show more</a> - ", mQuery.with("dominated", more));
      html.print("<a href=\"%s\">show all</a>)", mQuery.with("dominated", all));
    } else {
      html.print("%R", "show more - show more)");
    }
  }
}

