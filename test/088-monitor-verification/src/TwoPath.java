/*
 * Copyright (C) 2010 The Android Open Source Project
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

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

/*
 * Test case for conditionally using one of two synchronized objects. The dex code for this may
 * not be verifiable correctly for balanced locking.
 */
public class TwoPath {

    /**
     * Conditionally uses one of the synchronized objects.
     */
    public static void twoPath(Object obj1, Object obj2, int x) {
        Object localObj;

        synchronized (obj1) {
            synchronized(obj2) {
                if (x == 0) {
                    localObj = obj2;
                } else {
                    localObj = obj1;
                }
            }
        }

        doNothing(localObj);
    }

    private static void doNothing(Object o) {
    }
}
