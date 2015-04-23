/*
 * Copyright (C) 2014 The Android Open Source Project
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

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class Main {
  private boolean secondary;

  public Main(boolean secondary) {
      this.secondary = secondary;
  }

  public static void main(String[] args) throws Exception {
//      String[] cmdline = getCmdLine();
//      System.out.println(">>> " + java.util.Arrays.toString(cmdline) + " <<<");
//
//      if (args.length == 0) {
//          String[] secCmdLine = new String[cmdline.length + 1];
//          System.arraycopy(cmdline, 0, secCmdLine, 0, cmdline.length);
//          secCmdLine[secCmdLine.length - 1] = "--secondary";
//          exec(secCmdLine);
//      } else {
//          System.out.println(java.util.Arrays.toString(args));
//      }
      boolean secondary = false;
      if (args.length > 0 && args[0].equals("--secondary")) {
          secondary = true;
      }
      new Main(secondary).run();
  }

  static {
    System.loadLibrary("arttest");
  }

  private void run() {
    if (secondary) {
        runSecondary();
    } else {
        runPrimary();
    }
  }

  private void runSecondary() {
    foo(true);
    throw new RuntimeException("Didn't expect to get back...");
  }

  private void runPrimary() {
      // First do the in-process unwinding.
      foo(false);

      // Fork the secondary.
      String[] cmdline = getCmdLine();
      String[] secCmdLine = new String[cmdline.length + 1];
      System.arraycopy(cmdline, 0, secCmdLine, 0, cmdline.length);
      secCmdLine[secCmdLine.length - 1] = "--secondary";
      Process p = exec(secCmdLine);

      int pid = getPid(p);
      if (pid <= 0) {
          p.destroy();
          throw new RuntimeException("Couldn't parse process");
      }

      // Wait 1s, so the forked process has time to run until its sleep phase.
      try {
          Thread.sleep(5000);
      } catch (Exception e) {
          throw new RuntimeException(e);
      }

      unwindOtherProcess(pid);
  }

  private static Process exec(String[] args) {
      try {
          return Runtime.getRuntime().exec(args);
      } catch (Exception exc) {
          throw new RuntimeException(exc);
      }
  }

  private static int getPid(Process p) {
      // Could do reflection for the private pid field, but String parsing is easier.
      String s = p.toString();
      if (s.startsWith("Process[pid=")) {
          return Integer.parseInt(s.substring("Process[pid=".length(), s.length() - 1));
      } else {
          return -1;
      }
  }

  //Read /proc/self/cmdline to find the invocation command line (so we can fork another runtime).
  private static String[] getCmdLine() {
      try {
        BufferedReader in = new BufferedReader(new FileReader("/proc/self/cmdline"));
        String s = in.readLine();
        in.close();
        return s.split("\0");
      } catch (Exception exc) {
          throw new RuntimeException(exc);
      }
  }

  public boolean foo(boolean b) {
    return bar(b);
  }

  public boolean bar(boolean b) {
    if (b) {
        return sleep(2, b);
    } else {
        return unwindInProcess(1, b);
    }
  }

  public native boolean sleep(int i, boolean b);

  public native boolean unwindInProcess(int i, boolean b);
  public native boolean unwindOtherProcess(int pid);
}
