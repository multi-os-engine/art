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

import com.android.tools.perflib.heap.Instance;
import com.google.common.collect.Iterables;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

class ObjectsHandler extends AhatHandler {
  public ObjectsHandler(AhatSnapshot snapshot) {
    super(snapshot);
  }

  public void handle(HtmlWriter html, Query query) throws IOException {
    String siteId = query.get("site", mSnapshot.getRootSite().getId());
    String className = query.get("class", null);
    String heapName = query.get("heap", null);

    Site site = mSnapshot.getSiteById(siteId);
    if (site == null) {
      html.println("No site with id %s", siteId);
    }

    List<Instance> insts = new ArrayList<Instance>();
    for (Instance inst : site.getObjects()) {
      if ((heapName == null || inst.getHeap().getName().equals(heapName))
          && (className == null
            || AhatSnapshot.getClassName(inst.getClassObj()).equals(className))) {
        insts.add(inst);
      }
    }

    Collections.sort(insts, Sort.defaultInstanceCompare(mSnapshot));

    html.println("%R", "<h1>Objects</h1>");
    html.println("%R", "<table>");
    html.println("%R", "<tr><th>Size</th><th>Heap</th><th>Object</th></tr>");
    for (Instance inst : insts) {
      html.println("<tr><td>%,14d</td><td>%s</td><td>%I</td></tr>",
            inst.getSize(), inst.getHeap().getName(), inst);
    }
    html.println("%R", "</table>");
  }
}

