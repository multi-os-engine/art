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

import java.awt.image.BufferedImage;
import javax.imageio.ImageIO;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;
import java.net.URI;

import com.android.tools.perflib.heap.*;
import com.sun.net.httpserver.*;

class BitmapHandler implements HttpHandler {
  private AhatSnapshot mSnapshot;

  public BitmapHandler(AhatSnapshot snapshot) {
    mSnapshot = snapshot;
  }

  public void handle(HttpExchange exchange) throws IOException {
    try {
      URI uri = exchange.getRequestURI();

      long id = 0;
      String query = uri.getQuery();
      if (query != null) {
        for (String param : query.split("&")) {
          if (param.startsWith("id=")) {
            id = Long.parseLong(param.substring(3));
          }
        }
      }

      Instance inst = mSnapshot.findInstance(id);
      FieldReader.Bitmap data = FieldReader.readBitmap(inst);
      if (data != null) {
        int width = data.width;
        int height = data.height;
        byte[] buffer = data.buffer;

        // Convert BGRA to ABGR
        int[] abgr = new int[height * width];
        for (int i = 0; i < abgr.length; i++) {
          abgr[i] = (
              (((int)buffer[i * 4 + 3] & 0xFF) << 24) +
              (((int)buffer[i * 4 + 0] & 0xFF) << 16) +
              (((int)buffer[i * 4 + 1] & 0xFF) << 8) +
              ((int)buffer[i * 4 + 2] & 0xFF));
        }

        BufferedImage bitmap = new BufferedImage(
            width, height, BufferedImage.TYPE_4BYTE_ABGR);
        bitmap.setRGB(0, 0, width, height, abgr, 0, width);

        exchange.getResponseHeaders().add("Content-Type", "image/png");
        exchange.sendResponseHeaders(200, 0);
        OutputStream os = exchange.getResponseBody();
        ImageIO.write(bitmap, "png", os);
        os.close();
      } else {
        exchange.getResponseHeaders().add("Content-Type", "text/html");
        exchange.sendResponseHeaders(404, 0);
        PrintStream ps = new PrintStream(exchange.getResponseBody());
        ps.println("No bitmap found for the given request.");
        ps.close();
      }
    } catch (Exception e) {
      System.err.println("Bitmap Handler Exception: ");
      e.printStackTrace();
      throw e;
    }
  }
}

