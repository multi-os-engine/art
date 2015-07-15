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
import java.io.IOException;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

class SiteHandler extends AhatHandler {
  public SiteHandler(AhatSnapshot snapshot) {
    super(snapshot);
  }

  public void handle(HtmlWriter html, Query query) throws IOException {
    String id = query.get("id", mSnapshot.getRootSite().getId());
    Site site = mSnapshot.getSiteById(id);
    if (site == null) {
      html.println("No site with id %s", id);
      return;
    }

    html.println("<h1>Site %s</h1>", site.getName());
    html.println("%R", "<h2>Allocation Site:</h2>");
    SitePrinter.printSite(html, mSnapshot, site);

    html.println("%R", "<h2>Sites Called from Here:</h2>");
    List<Site> children = site.getChildren();
    if (children.isEmpty()) {
      html.println("%R", "(none)");
    } else {
      Collections.sort(children, new Sort.SiteBySize("app"));

      HeapTable.TableConfig<Site> table = new HeapTable.TableConfig<Site>() {
        public String getHeapsDescription() {
          return "Reachable Bytes Allocated on Heap";
        }

        public long getSize(Site element, Heap heap) {
          return element.getSize(heap.getName());
        }

        public List<HeapTable.ValueConfig<Site>> getValueConfigs() {
          HeapTable.ValueConfig<Site> value = new HeapTable.ValueConfig<Site>() {
            public String getDescription() {
              return "Child Site";
            }

            public void render(HtmlWriter html, Site element) {
              html.println("<a href=\"site?id=%s\">%s</a>", element.getId(), element.getName());
            }
          };
          return Collections.singletonList(value);
        }
      };
      HeapTable.render(html, table, mSnapshot, children);
    }

    html.println("%R", "<h2>Objects Allocated:</h2>");
    html.println("%R", "<table class=\"objects\">");
    html.println("%R", "<tr><th>Reachable Bytes Allocated</th>");
    html.println("%R", "<th>Instances</th><th>Heap</th><th>Class</th></tr>");
    List<Site.ObjectsInfo> infos = site.getObjectsInfos();
    Comparator<Site.ObjectsInfo> compare = new Sort.WithPriority<Site.ObjectsInfo>(
        new Sort.ObjectsInfoByHeapName(),
        new Sort.ObjectsInfoBySize(),
        new Sort.ObjectsInfoByClassName());
    Collections.sort(infos, compare);
    for (Site.ObjectsInfo info : infos) {
      String className = AhatSnapshot.getClassName(info.classObj);
      html.println("<tr><td>%,14d</td>", info.numBytes);
      html.println("<td><a href=\"objects?site=%s&heap=%s&class=%s\">%,14d</a></td>",
          site.getId(), info.heap.getName(), className, info.numInstances);
      html.println("<td>%s</td><td>%I</td></tr>", info.heap.getName(), info.classObj);
    }
    html.println("%R", "</table>");
  }
}

