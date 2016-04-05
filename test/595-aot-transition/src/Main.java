/*
 * Copyright (C) 2016 The Android Open Source Project
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

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

import java.lang.reflect.InvocationTargetException;

class MyClassLoader extends ClassLoader {
  MyClassLoader() throws Exception {
    super(MyClassLoader.class.getClassLoader());

    // Some magic to get access to the pathList field of BaseDexClassLoader.
    ClassLoader loader = getClass().getClassLoader();
    Class<?> baseDexClassLoader = loader.getClass().getSuperclass();
    Field f = baseDexClassLoader.getDeclaredField("pathList");
    f.setAccessible(true);
    pathList = f.get(loader);

    // Some magic to get access to the dexField field of pathList.
    // Need to make a copy of the dex elements since we don't want an app image with pre-resolved
    // things.
    f = pathList.getClass().getDeclaredField("dexElements");
    f.setAccessible(true);
    Object[] dexElements = (Object[]) f.get(pathList);
    f = dexElements[0].getClass().getDeclaredField("dexFile");
    f.setAccessible(true);
    for (Object element : dexElements) {
      Object dexFile = f.get(element);
      // Make copy.
      Field fileNameField = dexFile.getClass().getDeclaredField("mFileName");
      fileNameField.setAccessible(true);
      dexFiles.add(dexFile.getClass().getDeclaredConstructor(String.class).newInstance(
        fileNameField.get(dexFile)));
    }
  }

  Object pathList;
  ArrayList<Object> dexFiles = new ArrayList<Object>();
  Field dexFileField;

  protected Class<?> loadClass(String className, boolean resolve) throws ClassNotFoundException {
    // We're only going to handle Main.
    if (className != "Main") {
      return getParent().loadClass(className);
    }

    // Mimic what DexPathList.findClass is doing.
    try {
      for (Object dexFile : dexFiles) {
        Method method = dexFile.getClass().getDeclaredMethod(
            "loadClassBinaryName", String.class, ClassLoader.class, List.class);

        if (dexFile != null) {
          Class<?> clazz = (Class<?>)method.invoke(dexFile, className, this, null);
          if (clazz != null) {
            return clazz;
          }
        }
      }
    } catch (Exception e) { /* Ignore */ }
    return null;
  }
}

public class Main {
  static final int STATE_UNKNOWN = 0;
  static final int STATE_INTERPRETER = 1;
  static final int STATE_JIT = 2;
  static final int STATE_AOT = 3;

  public static int $noinline$testJitGetCompilationState() {
    if (doThrow) { throw new Error(); }
    return getCompilationState("$noinline$testJitGetCompilationState");
  }

  public static final boolean testJit() {
    if (doThrow) { throw new Error(); }

    int state = $noinline$testJitGetCompilationState();
    if (state == STATE_UNKNOWN) {
      System.out.println("testJit(): failed to retrieve state!");
      return false;
    }
    if (state != STATE_INTERPRETER || !hasJit()) {
      // Test does not apply, let it pass.
      return true;
    }

    do {
      state = $noinline$testJitGetCompilationState();
    } while (state == STATE_INTERPRETER);
    if (state != STATE_JIT) {
      return false;
    }
    do {
      try {
        // Now that we're in JIT-compiled code, don't waste the CPU time too much.
        Thread.sleep(25L);  // Sleep for 25ms; 40 iterations per second.
      } catch (InterruptedException ie) {
        System.out.println("testJit(): interrupted!");
        return false;
      }
      state = $noinline$testJitGetCompilationState();
    } while (state == STATE_JIT);
    if (state != STATE_AOT) {
      System.out.println("testJit(): unexpected state " + state);
      return false;
    }
    return true;
  }

  public static int $noinline$testInterpreterGetCompilationState() {
    if (doThrow) { throw new Error(); }
    return getCompilationState("$noinline$testInterpreterGetCompilationState");
  }

  public static final boolean testInterpreter() {
    if (doThrow) { throw new Error(); }

    int state = $noinline$testInterpreterGetCompilationState();
    if (state == STATE_UNKNOWN) {
      System.out.println("testInterpreter(): failed to retrieve state!");
      return false;
    }
    if (state != STATE_INTERPRETER) {
      // Test does not apply, let it pass.
      return true;
    }

    do {
      try {
        // Sleep a bit to avoid hitting JIT threshold.
        Thread.sleep(25L);  // Sleep for 25ms; 40 iterations per second.
      } catch (InterruptedException ie) {
        System.out.println("testInterpreter(): interrupted!");
        return false;
      }
      state = $noinline$testInterpreterGetCompilationState();
    } while (state == STATE_INTERPRETER);
    if (state != STATE_AOT) {
      System.out.println("testInterpreter(): unexpected state " + state);
      return false;
    }
    return true;
  }

  public static void main(final String[] args) {
    System.loadLibrary(args[0]);
    mainClassInPrimaryClassLoader = Main.class;
    final ClassLoader myLoader;
    final Class<?> myLoaderMain;
    try {
      myLoader = new MyClassLoader();
      myLoaderMain = myLoader.loadClass("Main");
      myLoaderMain.getDeclaredField("mainClassInPrimaryClassLoader").set(
          null, mainClassInPrimaryClassLoader);
    } catch (Exception e) {
      System.out.println("Failed to create MyClassLoader: " + e);
      return;
    }
    MyThread[] threads = {
        new MyThread("JitThread") {
          public void run() { result = Main.testJit(); }
        },
        new MyThread("InterpreterThread") {
          public void run() { result = Main.testInterpreter(); }
        },
        new MyThread("MyLoaderJitThread") {
          public void run() {
            try {
              result = (Boolean) myLoaderMain.getDeclaredMethod("testJit").invoke(null);
            } catch (InvocationTargetException ite) {
              System.out.println(getName() + " caught ITE: " + ite.getCause());
              result = false;
            } catch (Exception e) {
              System.out.println(getName() + " caught exception: " + e);
              result = false;
            }
          }
        },
        new MyThread("MyLoaderInterpreterThread") {
          public void run() {
            try {
              result = (Boolean) myLoaderMain.getDeclaredMethod("testInterpreter").invoke(null);
            } catch (InvocationTargetException ite) {
              System.out.println(getName() + " caught ITE: " + ite.getCause());
              result = false;
            } catch (Exception e) {
              System.out.println(getName() + " caught exception: " + e);
              result = false;
            }
          }
        },
    };

    for (MyThread t : threads) {
      t.start();
    }
    try {
      Thread.sleep(1000L);  // Sleep for a second, allow jitTest to be compiled.
      transitionToAotCode();

      for (MyThread t : threads) {
        t.join(1000L);
        Thread.State state = t.getState();
        if (state != Thread.State.TERMINATED) {
          System.out.println(t.getName() + " timeout, state = " + state);
          System.exit(1);
        }
        System.out.println(t.getName() + ": " + t.result);
      }
    } catch (InterruptedException ie) {
      System.out.println("Interrupted!");
    }
  }

  private static class MyThread extends Thread {
    public MyThread(String name) {
      super(name);
      result = false;
    }
    public boolean result;
  }

  public static int getCompilationState(String fn) {
    try {
      return (Integer) mainClassInPrimaryClassLoader.getDeclaredMethod(
          "nativeGetCompilationState", String.class).invoke(null, new Object[] { fn });
    } catch (InvocationTargetException ite) {
      System.out.println("getCompilationState() caught ITE: " + ite.getCause());
      return STATE_UNKNOWN;
    } catch (Exception e) {
      System.out.println("getCompilationState() caught: " + e);
      return STATE_UNKNOWN;
    }
  }

  public static boolean hasJit() {
    try {
      return (Boolean) mainClassInPrimaryClassLoader.getDeclaredMethod("nativeHasJit").invoke(null);
    } catch (InvocationTargetException ite) {
      System.out.println("hasJit() caught ITE: " + ite.getCause());
      return false;
    } catch (Exception e) {
      System.out.println("hasJit() caught: " + e);
      return false;
    }
  }

  public static native int nativeGetCompilationState(String fn);
  public static native boolean nativeHasJit();
  public static native void transitionToAotCode();

  public static boolean doThrow = false;
  public static Class<?> mainClassInPrimaryClassLoader;
}
