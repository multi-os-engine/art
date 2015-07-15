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
import java.util.Collections;
import java.util.List;

import com.android.tools.perflib.heap.*;

class SitePrinter {
  public static void printSite(HtmlWriter html, AhatSnapshot snapshot, Site site) {
    List<Site> path = new ArrayList<Site>();
    for (Site parent = site; parent != null; parent = parent.getParent()) {
      path.add(parent);
    }
    Collections.reverse(path);

    final HeapTable.ValueConfig<Site> value = new HeapTable.ValueConfig<Site>() {
      public String getDescription() {
        return "Stack Frame";
      }

      public void render(HtmlWriter html, Site element) {
        if (element.getParent() == null) {
          html.println("<a href=\"site?id=%s\">%s</a>", element.getId(), element.getName());
        } else {
          html.println("&#x2192;&nbsp<a href=\"site?id=%s\">%s</a>", element.getId(), element.getName());
        }
      }
    };

    HeapTable.TableConfig<Site> table = new HeapTable.TableConfig<Site>() {
      public String getHeapsDescription() {
        return "Reachable Bytes Allocated on Heap";
      }

      public long getSize(Site element, Heap heap) {
        return element.getSize(heap.getName());
      }

      public List<HeapTable.ValueConfig<Site>> getValueConfigs() {
        List<HeapTable.ValueConfig<Site>> values = new ArrayList<HeapTable.ValueConfig<Site>>();
        values.add(value);
        return values;
      }
    };
    HeapTable.render(html, table, snapshot, path); 
  }
}
