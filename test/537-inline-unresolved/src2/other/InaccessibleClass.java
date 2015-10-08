/*
 * Copyright (C) 2009 The Android Open Source Project
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

package other;

import stable.Stable;

/*package*/ class InaccessibleClass {
    public static void staticMethod() {}
    public static int staticField = 5;
    public void instanceMethod() {}
    public int instanceField = 5;

    public static void testInlineBadAccessClass() {
        Stable.$opt$inline$BadAccessClass(new Object());
    }

    public static void testInlineBadAccessStaticMethod() {
        Stable.$opt$inline$BadAccessStaticMethod();
    }

    public static void testInlineBadAccessStaticField() {
        Stable.$opt$inline$BadAccessStaticField();
    }
}
