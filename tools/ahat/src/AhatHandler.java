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

import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpServer;
import java.io.IOException;
import java.io.PrintStream;
import java.net.URI;

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

  abstract public void handle(HtmlWriter html, Query query) throws IOException;

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

      handle(new HtmlWriter(ps), new Query(exchange.getRequestURI()));
    } catch (RuntimeException e) {
      // Print runtime exceptions to standard error for debugging purposes,
      // because otherwise they are swallowed and not reported.
      System.err.println("Exception when handling " + exchange.getRequestURI() + ": ");
      e.printStackTrace();
      throw e;
    }
    ps.close();
  }
}

