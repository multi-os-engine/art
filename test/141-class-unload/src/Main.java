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

import java.lang.ref.WeakReference;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class Main {
    static final String DEX_FILE = System.getenv("DEX_LOCATION") + "/141-class-unload-ex.jar";

    public static void main(String[] args) throws Exception {
        Class pathClassLoader = Class.forName("dalvik.system.PathClassLoader");
        if (pathClassLoader == null) {
            throw new AssertionError("Couldn't find path class loader class");
        }
        Constructor constructor =
            pathClassLoader.getDeclaredConstructor(String.class, ClassLoader.class);
        try {
            WeakReference<Class> klass = testLoader(constructor);
            // No strong refernces to class loader, should get unloaded.
            Runtime.getRuntime().gc();
            // If the weak reference is cleared, then it was unloaded.
            System.out.println(klass.get());
        } catch (Exception e) {
            System.out.println(e);
        }
    }

    private static WeakReference<Class> testLoader(Constructor constructor) throws Exception {
        ClassLoader loader = (ClassLoader) constructor.newInstance(
            DEX_FILE, ClassLoader.getSystemClassLoader());
        Class intHolder = loader.loadClass("IntHolder");
        Method getValue = intHolder.getDeclaredMethod("getValue");
        Method setValue = intHolder.getDeclaredMethod("setValue", Integer.TYPE);
        System.out.println((int) getValue.invoke(intHolder));
        setValue.invoke(intHolder, 2);
        System.out.println((int) getValue.invoke(intHolder));
        return new WeakReference(intHolder);
    }
}
