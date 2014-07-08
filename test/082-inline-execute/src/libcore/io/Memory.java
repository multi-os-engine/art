/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

package libcore.io;

/**
 * Unsafe access to memory.
 */
public final class Memory {
    private Memory() { }

    public static native byte peekByte(long address);

    public static native int peekIntNative(long address);

    public static native short peekShortNative(long address);

    public static native long peekLongNative(long address);

    public static native void pokeByte(long address, byte value);

    public static native void pokeShortNative(long address, short value);

    public static native void pokeIntNative(long address, int value);

    public static native void pokeLongNative(long address, long value);
}
