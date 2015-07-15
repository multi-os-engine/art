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

import com.android.tools.perflib.heap.*;
import com.google.common.collect.Iterables;

class ObjectsHandler extends AhatHandler {
  public ObjectsHandler(AhatSnapshot snapshot) {
    super(snapshot);
  }

  public void handle(HtmlWriter html, URI uri) throws IOException {
    String siteId = mSnapshot.getRootSite().getId();
    String className = null;
    String heapName = null;

    String query = uri.getQuery();
    for (String param : query.split("&")) {
      if (param.startsWith("site=")) {
        siteId = param.substring("site=".length());
      } else if (param.startsWith("class=")) {
        className = param.substring("class=".length());
      } else if (param.startsWith("heap=")) {
        heapName = param.substring("heap=".length());
      }
    }

    Site site = mSnapshot.getSiteById(siteId);
    if (site == null) {
      html.println("No site with id %s", siteId);
    }

    html.println("%R", "<h1>Objects</h1>");
    html.println("%R", "<table>");
    html.println("%R", "<tr><th>Size</th><th>Heap</th><th>Object</th></tr>");
    for (Instance inst : site.getObjects()) {
      if ((heapName == null || inst.getHeap().getName().equals(heapName))
          && (className == null || inst.getClassObj().getClassName().equals(className))) {
        html.println("<tr><td>%,14d</td><td>%s</td><td>%I</td></tr>",
            inst.getSize(), inst.getHeap().getName(), inst);
      }
    }
    html.println("%R", "</table>");
  }
}

