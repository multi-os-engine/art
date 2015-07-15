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

import java.io.PrintStream;
import java.util.List;

/**
 * An Html implementation of Doc.
 */
public class HtmlDoc implements Doc {
  private PrintStream ps;
  private Column[] mCurrentTableColumns;

  public HtmlDoc(PrintStream ps) {
    this.ps = ps;
  }

  // Output the title of the page.
  // Format args are supported.
  public void title(String format, Object... args) {
    ps.print("<h1>");
    ps.print(DocString.text(String.format(format, args)).html());
    ps.println("</h1>");
  }

  // Start a new section with the given title.
  public void section(String title) {
    ps.print("<h2>");
    ps.print(DocString.text(title).html());
    ps.println(":</h2>");
  }

  public void println(DocString string) {
    ps.print(string.html());
    ps.println("<br />");
  }

  // Show a DocString in a large font that is easy to see.
  public void big(DocString str) {
    ps.print("<h2>");
    ps.print(str.html());
    ps.println("</h2>");
  }

  public void table(Column... columns) {
    if (columns.length == 0) {
      throw new IllegalArgumentException("No columns specified");
    }

    mCurrentTableColumns = columns;
    ps.println("<table>");
    for (int i = 0; i < columns.length - 1; i++) {
      ps.format("<th>%s</th>", columns[i].heading.html());
    }

    // Align the last header to the left so it's easier to see if the last
    // column is very wide.
    ps.format("<th align=\"left\">%s</th>", columns[columns.length - 1].heading.html());
  }

  // Start a table with the following heading structure:
  //   |  description  |  c2  | c3 | ... |
  //   | h1 | h2 | ... |      |    |     |
  // This is used by the HeapTable.
  public void table(DocString description, List<Column> subcols, List<Column> cols) {
    mCurrentTableColumns = new Column[subcols.size() + cols.size()];
    int j = 0;
    for (Column col : subcols) {
      mCurrentTableColumns[j] = col;
      j++;
    }
    for (Column col : cols) {
      mCurrentTableColumns[j] = col;
      j++;
    }

    ps.println("<table>");
    ps.format("<tr><th colspan=\"%d\">%s</th>", subcols.size(), description.html());
    for (int i = 0; i < cols.size() - 1; i++) {
      ps.format("<th rowspan=\"2\">%s</th>", cols.get(i).heading.html());
    }
    if (!cols.isEmpty()) {
      // Align the last column header to the left so it can still be seen if
      // the last column is very wide.
      ps.format("<th align=\"left\" rowspan=\"2\">%s</th>",
          cols.get(cols.size() - 1).heading.html());
    }
    ps.println("</tr>");

    ps.print("<tr>");
    for (Column subcol : subcols) {
      ps.format("<th>%s</th>", subcol.heading.html());
    }
    ps.println("</tr>");
  }

  // Add a row of a table.
  public void row(DocString... values) {
    if (mCurrentTableColumns == null) {
      throw new IllegalStateException("table method must be called before row");
    }

    if (mCurrentTableColumns.length != values.length) {
      throw new IllegalArgumentException(String.format(
          "Wrong number of row values. Expected %d, but got %d",
          mCurrentTableColumns.length, values.length));
    }

    ps.print("<tr>");
    for (int i = 0; i < values.length; i++) {
      ps.print("<td");
      if (mCurrentTableColumns[i].align == Align.RIGHT) {
        ps.print(" align=\"right\"");
      }
      ps.format(">%s</td>", values[i].html());
    }
    ps.println("</tr>");
  }

  // Start a description list.
  public void descriptions() {
    ps.println("<table>");
  }

  public void description(DocString key, DocString value) {
    ps.format("<tr><th align=\"left\">%s:</th><td>%s</td></tr>", key.html(), value.html());
  }

  // End the current table or description.
  public void end() {
    ps.println("</table>");
    mCurrentTableColumns = null;
  }
}
