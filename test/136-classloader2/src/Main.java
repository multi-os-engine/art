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

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

/**
 * Class loader test.
 */
public class Main {

    static final String DEX_FILE = System.getenv("DEX_LOCATION") + "/136-classloader2-ex.jar";

    /**
     * Main entry point.
     */
    public static void main(String[] args) throws Exception {
        ClassLoader loader = createClassLoader(null);

        Class<?> klass = loader.loadClass("SecondMain");
        Method main = klass.getDeclaredMethod("main", String[].class);
        main.invoke(null, (String)null);
    }

    private static ClassLoader createClassLoader(ClassLoader parent) throws Exception {
        if (parent == null) {
            // Derive the boot class-loader.
            parent = ClassLoader.getSystemClassLoader().getParent();
        }

        // TODO: Reflection for PathClassLoader
        Class<?> pathClassLoaderClass = parent.loadClass("dalvik.system.PathClassLoader");
        Constructor constructor = pathClassLoaderClass.getDeclaredConstructor(String.class,
                ClassLoader.class);

        Object pathClassLoader = constructor.newInstance(DEX_FILE, parent);

        return (ClassLoader)pathClassLoader;
    }
}
