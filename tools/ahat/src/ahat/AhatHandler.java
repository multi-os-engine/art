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
import java.io.PrintStream;
import java.net.URI;

import com.android.tools.perflib.heap.*;
import com.sun.net.httpserver.*;

/**
 * AhatHandler.
 *
 * Common base class of all the ahat HttpHandlers.
 */
abstract class AhatHandler implements HttpHandler {

  protected AhatSnapshot mSnapshot;

  public AhatHandler(AhatSnapshot snapshot) {
    mSnapshot = snapshot;
  }

  abstract public void handle(HtmlWriter html, URI uri) throws IOException;

  public void handle(HttpExchange exchange) throws IOException {
    exchange.getResponseHeaders().add("Content-Type", "text/html");
    exchange.sendResponseHeaders(200, 0);
    PrintStream ps = new PrintStream(exchange.getResponseBody());
    try {
      ps.println("<head>");
      ps.println("<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">");
      ps.println("</head>");
      ps.print("<div class=\"menu\"><a href=\"/\">overview</a> - ");
      ps.print("<a href=\"roots\">roots</a> - ");
      ps.print("<a href=\"sites\">allocations</a> - ");
      ps.println("<a href=\"help\">help</a></div>");

      handle(new HtmlWriter(ps), exchange.getRequestURI());
    } catch (Exception e) {
      System.err.println("Handler Exception: ");
      e.printStackTrace();
      throw e;
    }
    ps.close();
  }
}

