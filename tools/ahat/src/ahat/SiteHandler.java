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

import java.io.IOException;
import java.net.URI;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

import com.android.tools.perflib.heap.*;

class SiteHandler extends AhatHandler {
  public SiteHandler(AhatSnapshot snapshot) {
    super(snapshot);
  }

  public void handle(HtmlWriter html, URI uri) throws IOException {
    String id = mSnapshot.getRootSite().getId();
    String query = uri.getQuery();
    if (query != null) {
      for (String param : query.split("&")) {
        if (param.startsWith("id=")) {
          id = param.substring(3);
        }
      }
    }

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
      Collections.sort(children, new Site.CompareBySize("app"));
      final HeapTable.ValueConfig<Site> value = new HeapTable.ValueConfig<Site>() {
        public String getDescription() {
          return "Child Site";
        }

        public void render(HtmlWriter html, Site element) {
          html.println("<a href=\"site?id=%s\">%s</a>", element.getId(), element.getName());
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
      HeapTable.render(html, table, mSnapshot, children); 
    }

    html.println("%R", "<h2>Objects Allocated:</h2>");
    html.println("%R", "<table class=\"objects\">");
    html.println("%R", "<tr><th>Reachable Bytes Allocated</th><th>Instances</th><th>Heap</th><th>Class</th></tr>");
    List<Site.ObjectsInfo> infos = site.getObjectsInfos();
    Comparator<Site.ObjectsInfo> compare = null;
    compare = new ObjectsInfoByClassName(compare);
    compare = new ObjectsInfoBySize(compare);
    compare = new ObjectsInfoByHeapName(compare);
    Collections.sort(infos, compare);
    for (Site.ObjectsInfo info : infos) {
      String className = info.classObj == null ? "null" : info.classObj.getClassName();
      html.println("<tr><td>%,14d</td><td><a href=\"objects?site=%s&heap=%s&class=%s\">%,14d</a></td><td>%s</td><td>%I</td></tr>",
          info.numBytes, site.getId(), info.heap.getName(), className, info.numInstances, info.heap.getName(), info.classObj);
    }
    html.println("%R", "</table>");
  }

  private static class ObjectsInfoBySize implements Comparator<Site.ObjectsInfo>  {
    private Comparator<Site.ObjectsInfo> mFallback;

    public ObjectsInfoBySize(Comparator<Site.ObjectsInfo> fallback) {
      mFallback = fallback;
    }

    public int compare(Site.ObjectsInfo a, Site.ObjectsInfo b) {
      int res = Long.compare(b.numBytes, a.numBytes);
      return (res == 0 && mFallback != null) ? mFallback.compare(a, b) : res;
    }
  }

  private static class ObjectsInfoByHeapName implements Comparator<Site.ObjectsInfo>  {
    private Comparator<Site.ObjectsInfo> mFallback;

    public ObjectsInfoByHeapName(Comparator<Site.ObjectsInfo> fallback) {
      mFallback = fallback;
    }

    public int compare(Site.ObjectsInfo a, Site.ObjectsInfo b) {
      int res = a.heap.getName().compareTo(b.heap.getName());
      return (res == 0 && mFallback != null) ? mFallback.compare(a, b) : res;
    }
  }

  private static class ObjectsInfoByClassName implements Comparator<Site.ObjectsInfo>  {
    private Comparator<Site.ObjectsInfo> mFallback;

    public ObjectsInfoByClassName(Comparator<Site.ObjectsInfo> fallback) {
      mFallback = fallback;
    }

    public int compare(Site.ObjectsInfo a, Site.ObjectsInfo b) {
      int res = a.classObj.getClassName().compareTo(b.classObj.getClassName());
      return (res == 0 && mFallback != null) ? mFallback.compare(a, b) : res;
    }
  }
}

