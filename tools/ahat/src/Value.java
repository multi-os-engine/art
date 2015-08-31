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

import com.android.tools.perflib.heap.ClassObj;
import com.android.tools.perflib.heap.Instance;
import java.net.URI;

/**
 * Class to render an hprof value to a DocString.
 */
class Value {

  // For string literals, we limit the number of characters we show to
  // kMaxCharsOfStringLiteralToShow in case the string is really long.
  private static int kMaxCharsOfStringLiteralToShow = 200;

  /**
   * Create a DocString representing a summary of the given instance.
   */
  private static DocString renderInstance(Instance inst) {
    DocString link = new DocString();
    if (inst == null) {
      link.append("(null)");
      return link;
    }

    // Annotate classes as classes.
    if (inst instanceof ClassObj) {
      link.append("class ");
    }

    link.append(inst.toString());

    // Annotate Strings with their values.
    String stringValue = InstanceUtils.asString(inst);
    if (stringValue != null) {
      link.append(" \"%s", initialCharsOf(stringValue, "\"", "..."));
    }

    // Annotate DexCache with its location.
    String dexCacheLocation = InstanceUtils.getDexCacheLocation(inst);
    if (dexCacheLocation != null) {
      boolean isLong = dexCacheLocation.length() > 200;
      link.append(" for %s", initialCharsOf(dexCacheLocation, "", "..."));
    }

    URI objTarget = DocString.uri("object?id=%d", inst.getId());
    DocString formatted = DocString.link(objTarget, link);

    // Annotate bitmaps with a thumbnail.
    Instance bitmap = InstanceUtils.getAssociatedBitmapInstance(inst);
    String thumbnail = "";
    if (bitmap != null) {
      URI uri = DocString.uri("bitmap?id=%d", bitmap.getId());
      formatted.appendThumbnail(uri, "bitmap image");
    }
    return formatted;
  }

  /**
   * Create a DocString summarizing the given value.
   */
  public static DocString render(Object val) {
    if (val instanceof Instance) {
      return renderInstance((Instance)val);
    } else {
      return DocString.text("%s", val);
    }
  }

  // Return at most the first kMaxCharsOfStringLiteralToShow characters of the
  // given string.  suffixIfComplete is appended if all the characters are
  // shown.  suffixIfTruncated is appended if not all of the characters are
  // shown.
  private static String initialCharsOf(String str,
      String suffixIfComplete, String suffixIfTruncated) {
    if (str.length() > kMaxCharsOfStringLiteralToShow) {
      String truncated = str.substring(0, kMaxCharsOfStringLiteralToShow);
      return String.format("%s%s", truncated, suffixIfTruncated);
    }
    return String.format("%s%s", str, suffixIfComplete);
  }
}
