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

import com.android.tools.perflib.heap.*;
import com.google.common.html.HtmlEscapers;

import java.io.PrintStream;

/**
 * Class to safely write html.
 */
public class HtmlWriter {
  private PrintStream ps;

  public HtmlWriter(PrintStream ps) {
    this.ps = ps;
  }

  /**
   * Output formatted of html.
   * The format string supports the following format specifiers:
   *  %s - A string that should be sanitized in the output html.
   *  %R - A raw string that is already sanitized. The user must ensure the
   *       string is safe to include directly in the html output without
   *       introducing cross site scripting vulnerabilities.
   *  %I - An Instance object that will be printed in summary format with a
   *       link to the object view.
   *  %V - The string value of a string object.
   *
   * The format string itself will not be sanitized. To ensure an unsanitized
   * string is not accidentally passed as the format string, this requires at
   * least one format argument. To print an already sanitized string, use:
   *    print("%R", str);
   * To print a string that needs to be sanitized, use:
   *    print("%s", str);
   */
  public void print(String format, Object arg1, Object... args) {
    // TODO: This implementation is very fragile. Worth fixing?
    Object[] nargs = new Object[args.length+1];
    nargs[0] = arg1;
    for (int i = 0; i < args.length; i++) {
      nargs[i+1] = args[i];
    }

    String newformat = "";
    int argi = 0;
    for (int i = 0; i < format.length(); i++) {
      newformat += format.charAt(i);
      if (format.charAt(i) == '%') {
        if (i+1 < format.length()) {
          switch (format.charAt(i+1)) {
            case 's':
              newformat += 's';
              nargs[argi] = sanitize(nargs[argi].toString());
              i++;
              break;

            case 'R':
              newformat += 's';
              i++;
              break;

            case 'I':
              newformat += 's';
              nargs[argi] = formatValue(nargs[argi]);
              i++;
              break;

            case 'V':
              newformat += 's';
              try {
                String str = FieldReader.readStringValue(nargs[argi]);
                nargs[argi] = sanitize(str);
              } catch (FieldReader.FieldReaderException e) {
                nargs[argi] = "(not a string)";
              }
              i++;
              break;
          }
        }
        argi++;
      }
    }
    ps.print(String.format(newformat, nargs));
  }

  public void println(String format, Object arg1, Object... args) {
    print(format + "\n", arg1, args);
  }

  static private String sanitize(String str) {
    return HtmlEscapers.htmlEscaper().escape(str);
  }

  // Return a sanitized short string identifying an instance. with a link to
  // more detail.
  static private String formatInstance(Instance inst) {
    if (inst == null) {
      return "(null)";
    }

    String str = inst.toString();

    // Annotate classes as classes.
    if (inst instanceof ClassObj) {
      str = "class " + str;
    }

    // Annotate Strings with their values.
    try {
      String val = FieldReader.readStringValue(inst);
      str += " \"" + val + "\"";
    } catch (FieldReader.FieldReaderException expected) {
      // If we can't read the value, this isn't a string, so there is nothing
      // to annotate.
    }

    // Annotate DexCache with its location.
    try {
      FieldReader.verifyIsClassInstance(inst, "java.lang.DexCache");
      String location = FieldReader.readStringField(inst, "location");
      str += " for " + location;
    } catch (FieldReader.FieldReaderException expected) {
      // If we can't read the location, this isn't a DexCache with a location,
      // so there is nothing to annotate.
    }

    // Annotate bitmaps with a tag and thumbnail.
    Instance bitmap = FieldReader.getBitmap(inst);
    String thumbnail = "";
    if (bitmap != null) {
      thumbnail = String.format(
          " <img height=\"16\" alt=\"bitmap image\" src=\"bitmap?id=%d\" />",
          bitmap.getId());
    }

    return String.format("<a href=object?id=%d>%s</a>%s",
        inst.getId(), sanitize(str), thumbnail);
  }

  // Return a sanitized short string identifying a field value.
  static private String formatValue(Object val) {
    if (val instanceof Instance) {
      return formatInstance((Instance)val);
    } else {
      return sanitize(String.format("%s", val));
    }
  }
}
