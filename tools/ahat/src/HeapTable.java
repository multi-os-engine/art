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
import java.util.ArrayList;
import java.util.List;

/**
 * Class for rendering a table that includes sizes of some kind for each heap.
 */
class HeapTable {
  /**
   * Configuration for a value column of a heap table.
   */
  public static interface ValueConfig<T> {
    public String getDescription();
    public void render(HtmlWriter html, T element);
  }

  /**
   * Configuration for the HeapTable.
   */
  public static interface TableConfig<T> {
    public String getHeapsDescription();
    public long getSize(T element, Heap heap);
    public List<ValueConfig<T>> getValueConfigs();
  }

  public static <T> void render(HtmlWriter html, TableConfig<T> config,
      AhatSnapshot snapshot, List<T> elements) {
    // Only show the heaps that have non-zero entries.
    List<Heap> heaps = new ArrayList<Heap>();
    for (Heap heap : snapshot.getHeaps()) {
      if (snapshot.getHeapSize(heap) > 0) {
        for (T element : elements) {
          if (config.getSize(element, heap) > 0) {
            heaps.add(heap);
            break;
          }
        }
      }
    }

    List<ValueConfig<T>> values = config.getValueConfigs();

    html.println("%R", "<table>");

    // Print the heap and values descriptions.
    boolean showTotal = heaps.size() > 1;
    int numHeapCols = heaps.size() + (showTotal ? 1 : 0);
    html.println("<tr><th colspan=\"%d\">%s</th>", numHeapCols, config.getHeapsDescription());
    for (ValueConfig value : values) {
      html.println("<th rowspan=\"2\">%s</th>", value.getDescription());
    }
    html.println("%R", "</tr>");

    // Print the heap names.
    html.println("%R", "<tr>");
    for (Heap heap : heaps) {
      html.println("<th>%s</th>", heap.getName());
    }
    if (showTotal) {
      html.println("%R", "<th>Total</th>");
    }
    html.println("%R", "</tr>");

    // Print the entries.
    for (T elem : elements) {
      html.println("%R", "<tr>");
      long total = 0;
      for (Heap heap : heaps) {
        long size = config.getSize(elem, heap);
        total += size;
        html.print("<td>%,14d</td>", size);
      }
      if (showTotal) {
        html.println("<td>%,14d</td>", total);
      }

      for (ValueConfig<T> value : values) {
        html.print("%R", "<td>");
        value.render(html, elem);
        html.print("%R", "</td>");
      }
      html.println("%R", "</tr>");
    }
    html.println("%R", "</table>");
  }
}

