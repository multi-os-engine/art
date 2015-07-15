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
import java.util.HashSet;
import java.util.List;

import com.android.tools.perflib.heap.*;

class RootsHandler extends AhatHandler {
  public RootsHandler(AhatSnapshot snapshot) {
    super(snapshot);
  }

  public void handle(HtmlWriter html, URI uri) throws IOException {
    html.println("%R", "<h1>Roots</h1>");

    HashSet<Instance> rootset = new HashSet<Instance>();
    for (RootObj root : mSnapshot.getGCRoots()) {
      Instance inst = root.getReferredInstance();
      if (inst != null) {
        rootset.add(inst);
      }
    }

    List<Instance> roots = new ArrayList<Instance>();
    for (Instance inst : rootset) {
      roots.add(inst);
    }

    DominatedList list = new DominatedList(mSnapshot, roots, uri);
    list.render(html);
  }
}

