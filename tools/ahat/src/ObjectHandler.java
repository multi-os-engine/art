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

import com.android.tools.perflib.heap.ArrayInstance;
import com.android.tools.perflib.heap.ClassInstance;
import com.android.tools.perflib.heap.ClassObj;
import com.android.tools.perflib.heap.Field;
import com.android.tools.perflib.heap.Heap;
import com.android.tools.perflib.heap.Instance;
import com.android.tools.perflib.heap.RootObj;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;

class ObjectHandler extends AhatHandler {
  public ObjectHandler(AhatSnapshot snapshot) {
    super(snapshot);
  }

  public void handle(HtmlWriter html, Query query) throws IOException {
    long id = query.getLong("id", 0);
    Instance inst = mSnapshot.findInstance(id);
    if (inst == null) {
      html.println("No object with id %08xl", id);
      return;
    }

    html.println("<h1>Object %08x</h1>", inst.getUniqueId());
    html.println("<h2>%I</h2>", inst);

    printAllocationSite(html, inst);
    printDominatorPath(html, inst);

    html.println("%R", "<h2>Object Info:</h2>");
    ClassObj cls = inst.getClassObj();
    html.println("Class: %s<br />", AhatSnapshot.getClassName(cls));
    html.println("Size: %d<br />", inst.getSize());
    html.println("Retained Size: %d<br />", inst.getTotalRetainedSize());
    html.println("Heap: %s<br />", inst.getHeap().getName());

    printBitmap(html, inst);
    if (inst instanceof ClassInstance) {
      printClassInstanceFields(html, (ClassInstance)inst);
    } else if (inst instanceof ArrayInstance) {
      printArrayElements(html, (ArrayInstance)inst);
    } else if (inst instanceof ClassObj) {
      printClassInfo(html, (ClassObj)inst);
    }
    printReferences(html, inst);
    printDominatedObjects(html, query, inst);
  }

  static private void printClassInstanceFields(HtmlWriter html, ClassInstance inst) {
    html.println("%R", "<h2>Fields:</h2>");
    html.println("%R", "<table class=\"fields\">");
    html.println("%R", "<tr><th>Type</th><th>Name</th><th>Value</th></tr>");
    for (ClassInstance.FieldValue field : inst.getValues()) {
      html.println("<tr><td>%s</td><td>%s</td><td>%I</td></tr>",
          field.getField().getType(), field.getField().getName(),
          field.getValue());
    }
    html.println("%R", "</table>");
  }

  static private void printArrayElements(HtmlWriter html, ArrayInstance array) {
    html.println("%R", "<h2>Array Elements:</h2>");
    html.println("%R", "<table>");
    html.println("%R", "<tr><th>Index</th><th>Value</th></tr>");
    Object[] elements = array.getValues();
    for (int i = 0; i < elements.length; i++) {
      html.println("<tr><td>%d</td><td>%I</td></tr>", i, elements[i]);
    }
    html.println("%R", "</table>");
  }

  private static void printClassInfo(HtmlWriter html, ClassObj clsobj) {
    html.println("Super Class: %I<br />", clsobj.getSuperClassObj());
    html.println("Class Loader: %I<br />", clsobj.getClassLoader());

    html.println("%R", "<h2>Static Fields:</h2>");
    html.println("%R", "<table>");
    html.println("%R", "<tr><th>Type</th><th>Name</th><th>Value</th></tr>");
    for (Map.Entry<Field, Object> field : clsobj.getStaticFieldValues().entrySet()) {
      html.println("<tr><td>%s</td><td>%s</td><td>%I</td></tr>",
          field.getKey().getType(), field.getKey().getName(),
          field.getValue());
    }
    html.println("%R", "</table>");
  }

  private static void printReferences(HtmlWriter html, Instance inst) {
    html.println("%R", "<h2>Objects with References to this Object: </h2>");
    if (inst.getHardReferences().isEmpty()) {
      html.println("%R", "(none)");
    } else {
      html.println("%R", "<table>");
      for (Instance ref : inst.getHardReferences()) {
        html.println("<tr><td>%I</td></tr>", ref);
      }
      html.println("%R", "</table>");
    }

    if (inst.getSoftReferences() != null) {
      html.println("%R", "<h2>Objects with Soft References to this Object: </h2>");
      html.println("%R", "<table>");
      for (Instance ref : inst.getSoftReferences()) {
        html.println("<tr><td>%I</td></tr>", inst);
      }
      html.println("%R", "</table>");
    }
  }

  private void printAllocationSite(HtmlWriter html, Instance inst) {
    html.println("%R", "<h2>Allocation Site: </h2>");
    Site site = mSnapshot.getSiteByObjectId(inst.getId());
    SitePrinter.printSite(html, mSnapshot, site);
  }

  // Draw the bitmap corresponding to this instance if there is one.
  private static void printBitmap(HtmlWriter html, Instance inst) {
    Instance bitmap = FieldReader.getBitmap(inst);
    if (bitmap != null) {
      html.println("%R", "<h2>Bitmap Image:</h2>");
      html.println("<img alt=\"bitmap image\" src=\"bitmap?id=%d\" /><br />", bitmap.getId());
    }
  }

  private void printDominatorPath(HtmlWriter html, Instance inst) {
    html.println("%R", "<h2>Dominator Path from Root:</h2>");
    List<Instance> path = new ArrayList<Instance>();
    for (Instance parent = inst;
        parent != null && !(parent instanceof RootObj);
        parent = parent.getImmediateDominator()) {
      path.add(parent);
    }

    // Add 'null' as a marker for the root.
    path.add(null);
    Collections.reverse(path);

    HeapTable.TableConfig<Instance> table = new HeapTable.TableConfig<Instance>() {
      public String getHeapsDescription() {
        return "Bytes Retained by Heap";
      }

      public long getSize(Instance element, Heap heap) {
        if (element == null) {
          return mSnapshot.getHeapSize(heap);
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
              html.println("%R", "<a href=\"roots\">ROOT</a>");
            } else {
              html.println("&#x2192;&nbsp;%I", element);
            }
          }
        };
        return Collections.singletonList(value);
      }
    };
    HeapTable.render(html, table, mSnapshot, path);
  }

  public void printDominatedObjects(HtmlWriter html, Query query, Instance inst) {
    html.println("%R", "<h2>Immediately Dominated Objects:</h2>");
    List<Instance> instances = mSnapshot.getDominated(inst);
    if (instances != null) {
      DominatedList list = new DominatedList(mSnapshot, instances, query);
      list.render(html);
    } else {
      html.println("%R", "(none)");
    }
  }
}

