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
import java.io.File;
import java.util.Collections;
import java.util.List;

class OverviewHandler extends AhatHandler {
  private File mHprof;

  public OverviewHandler(AhatSnapshot snapshot, File hprof) {
    super(snapshot);
    mHprof = hprof;
  }

  public void handle(HtmlWriter html, Query query) throws IOException {
    html.println("%R", "<h1>Overview</h1>");

    html.println("%R", "<h2>General Information:</h2>");
    html.println("%R", "<table class=\"overview\">");
    html.println("<tr><th>ahat version:</th><td>ahat-%s</td></tr>",
        OverviewHandler.class.getPackage().getImplementationVersion());
    html.println("<tr><th>hprof file:</th><td>%s</td></tr>", mHprof);
    html.println("%R", "</table>");

    html.println("%R", "<h2>Heap Sizes:</h2>");
    printHeapSizes(html);

    html.print("%R", "<h2><a href=\"roots\">Roots</a> - ");
    html.print("%R", "<a href=\"site\">Allocations</a> - ");
    html.println("%R", "<a href=\"help\">Help</a></h2>");
  }

  private void printHeapSizes(HtmlWriter html) {
    List<Object> dummy = Collections.singletonList(null);

    HeapTable.TableConfig<Object> table = new HeapTable.TableConfig<Object>() {
      public String getHeapsDescription() {
        return "Bytes Retained by Heap";
      }

      public long getSize(Object element, Heap heap) {
        return mSnapshot.getHeapSize(heap);
      }

      public List<HeapTable.ValueConfig<Object>> getValueConfigs() {
        return Collections.emptyList();
      }
    };
    HeapTable.render(html, table, mSnapshot, dummy);
  }
}

