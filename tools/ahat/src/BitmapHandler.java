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
import com.android.tools.perflib.heap.Instance;
import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import java.awt.image.BufferedImage;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;
import java.net.URI;
import javax.imageio.ImageIO;

class BitmapHandler implements HttpHandler {
  private AhatSnapshot mSnapshot;

  public BitmapHandler(AhatSnapshot snapshot) {
    mSnapshot = snapshot;
  }

  public void handle(HttpExchange exchange) throws IOException {
    try {
      Query query = new Query(exchange.getRequestURI());
      long id = query.getLong("id", 0);
      Instance inst = mSnapshot.findInstance(id);
      if (inst == null) {
        noBitmapFound(exchange);
        return;
      }

      BufferedImage bitmap = FieldReader.readBitmap(inst);
      if (bitmap == null) {
        noBitmapFound(exchange);
        return;
      }

      exchange.getResponseHeaders().add("Content-Type", "image/png");
      exchange.sendResponseHeaders(200, 0);
      OutputStream os = exchange.getResponseBody();
      ImageIO.write(bitmap, "png", os);
      os.close();
    } catch (Exception e) {
      System.err.println("Bitmap Handler Exception: ");
      e.printStackTrace();
      throw e;
    }
  }

  /**
   * Report that no bitmap has been found for the given request.
   */
  static private void noBitmapFound(HttpExchange exchange) throws IOException {
    exchange.getResponseHeaders().add("Content-Type", "text/html");
    exchange.sendResponseHeaders(404, 0);
    PrintStream ps = new PrintStream(exchange.getResponseBody());
    ps.println("No bitmap found for the given request.");
    ps.close();
  }
}

