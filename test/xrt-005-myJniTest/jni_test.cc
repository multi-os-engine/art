/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <vector>

#include "jni.h"

static JavaVM* jvm = NULL;

extern "C" jbyte JNICALL Java_Main_byteMethod(JNIEnv* env, jclass klass, jbyte b1, jbyte b2,
                                              jbyte b3, jbyte b4, jbyte b5, jbyte b6,
                                              jbyte b7, jbyte b8, jbyte b9, jbyte b10) {
  // We use b1 to drive the output.
    if((b2 == 2) && (b3 == -3) && (b4 == 4) && (b5 == -5)&&
       (b6 == 6) && (b7 == -7) && (b8 == 8) && (b9 == -9) && (b10 == 10)){
        printf("byteMethod OK\n");
    } else {
        printf("byteMethod FAIL!\n");
        printf("args are  %d %d %d %d %d %d %d %d %d\n", b2, b3, b4, b5, b6, b7, b8, b9, b10);
        printf("should be 2 -3 4 -5 6 -7 8 -9 10\n");
    }

  return b1;
}

extern "C" jshort JNICALL Java_Main_shortMethod(JNIEnv* env, jclass klass, jshort s1, jshort s2,
                                                jshort s3, jshort s4, jshort s5, jshort s6,
                                                jshort s7, jshort s8, jshort s9, jshort s10) {
  // We use s1 to drive the output.
    if((s2 == 2) && (s3 == -3) && (s4 == 4) && (s5 == -5)&&
       (s6 == 6) && (s7 == -7) && (s8 == 8) && (s9 == -9) && (s10 == 10)){
        printf("shortMethod OK\n");
    } else {
        printf("shortMethod FAIL!\n");
        printf("args are  %d %d %d %d %d %d %d %d %d\n", s2, s3, s4, s5, s6, s7, s8, s9, s10);
        printf("should be 2 -3 4 -5 6 -7 8 -9 10\n");
    }

  return s1;
}

extern "C" jboolean JNICALL Java_Main_booleanMethod(JNIEnv* env, jclass klass, jboolean b1,
                                                    jboolean b2, jboolean b3, jboolean b4,
                                                    jboolean b5, jboolean b6, jboolean b7,
                                                    jboolean b8, jboolean b9, jboolean b10) {
  // We use b1 to drive the output.
    if((b2 == JNI_TRUE) && (b3 == JNI_FALSE) && (b4 == JNI_TRUE) && (b5 == JNI_FALSE)&&
       (b6 == JNI_TRUE) && (b7 == JNI_FALSE) && (b8 == JNI_TRUE) && (b9 == JNI_FALSE) && (b10 == JNI_TRUE)){
        printf("booleanMethod OK\n");
    } else {
        printf("booleanMethod FAIL!\n");
        printf("args are  %d %d %d %d %d %d %d %d %d\n", b2, b3, b4, b5, b6, b7, b8, b9, b10);
        printf("should be  1 0 1 0 1 0 1 0 1\n");
    }

  assert(b1 == JNI_TRUE || b1 == JNI_FALSE);
  return b1;
}

extern "C" jchar JNICALL Java_Main_charMethod(JNIEnv* env, jclass klacc, jchar c1, jchar c2,
                                              jchar c3, jchar c4, jchar c5, jchar c6, jchar c7,
                                              jchar c8, jchar c9, jchar c10) {
  // We use c1 to drive the output.
    if((c2 == 'a') && (c3 == 'b') && (c4 == 'c') && (c5 == '0')&&
       (c6 == '1') && (c7 == '2') && (c8 == 1234) && (c9 == 2345) && (c10 == 3456)){
        printf("charMethod OK\n");
    } else {
        printf("charMethod FAIL!\n");
        printf("args are  %c %c %c %c %c %c %c %c %c\n", c2, c3, c4, c5, c6, c7, c8, c9, c10);
        printf("should be a b c 0 1 2 \322 ) \200\n");
    }

  return c1;
}

extern "C" jint JNICALL Java_Main_intMethod(JNIEnv* env, jclass klacc, jint i1,  jint i2,
                                                                  jint i3, jint i4, jint i5, jint i6, jint i7, jint i8, jint i9, jint i10) {
    // We use c1 to drive the output.
    if((i2 == 2) && (i3 == -3) && (i4 == 4) && (i5 == -5)&&
       (i6 == 6) && (i7 == -7) && (i8 == 8) && (i9 == -30000) && (i10 == 30000)){
        printf("intMethod OK\n");
    } else {
        printf("intMethod FAIL!\n");
        printf("args are  %d %d %d %d %d %d %d %d %d\n", i2, i3, i4, i5, i6, i7, i8, i9, i10);
        printf("should be 2 -3 4 -5 6 -7 8 -30000 30000\n");
    }
    
    return i1;
}

extern "C" jlong JNICALL Java_Main_longMethod(JNIEnv* env, jclass klacc, jlong l1,  jlong l2,
                                                                jlong l3, jlong l4, jlong l5, jlong l6, jlong l7, jlong l8, jlong l9, jlong l10) {
    // We use c1 to drive the output.
    if((l2 == 2) && (l3 == -3) && (l4 == 4) && (l5 == -5)&&
       (l6 == 6) && (l7 == -7) && (l8 == 8) && (l9 == -100000) && (l10 == 100000)){
        printf("longMethod OK\n");
    } else {
        printf("longMethod FAIL!\n");
        printf("args are  %ld %ld %ld %ld %ld %ld %ld %ld %ld\n", (long)l2, (long)l3, (long)l4, (long)l5, (long)l6, (long)l7, (long)l8, (long)l9, (long)l10);
        printf("should be 2 -3 4 -5 6 -7 8 -100000 100000\n");
    }
    
    return l1;
}

extern "C" jbyte JNICALL Java_Main_byteshortMethod(JNIEnv* env, jclass klacc, jbyte b1,  jbyte b2,
                                                                  jbyte b3, jbyte b4, jbyte b5, jbyte b6, jbyte b7, jshort s1, jbyte b8, jshort s2) {
    // We use c1 to drive the output.
    if((b2 == 2) && (b3 == -3) && (b4 == 4) && (b5 == -5)&&
       (b6 == 6) && (b7 == -7) && (s1 == 8) && (b8 == -9) && (s2 == 10)){
        printf("byteshortMethod OK\n");
    } else {
        printf("byteshortMethod FAIL!\n");
        printf("args are  %d %d %d %d %d %d %d %d %d\n", b2, b3, b4, b5, b6, b7, s1, b8, s2);
        printf("should be 2 -3 4 -5 6 -7 8 -9 10\n");
    }
    
    return b1;
}

extern "C" jbyte JNICALL Java_Main_byteintMethod(JNIEnv* env, jclass klacc, jbyte b1,  jbyte b2,
                                                                       jbyte b3, jbyte b4, jbyte b5, jbyte b6, jbyte b7, jint i1, jbyte b8, jint i2) {
    // We use c1 to drive the output.
    if((b2 == 2) && (b3 == -3) && (b4 == 4) && (b5 == -5)&&
       (b6 == 6) && (b7 == -7) && (i1 == 8) && (b8 == -9) && (i2 == 30000)){
        printf("byteintMethod OK\n");
    } else {
        printf("byteintMethod FAIL!\n");
        printf("args are  %d %d %d %d %d %d %d %d %d\n", b2, b3, b4, b5, b6, b7, i1, b8, i2);
        printf("should be 2 -3 4 -5 6 -7 8 -9 30000\n");
    }
    
    return b1;
}

extern "C" jbyte JNICALL Java_Main_bytelongMethod(JNIEnv* env, jclass klacc, jbyte b1,  jbyte b2,
                                                                       jbyte b3, jbyte b4, jbyte b5, jbyte b6, jbyte b7, jlong l1, jbyte b8, jlong l2) {
    // We use c1 to drive the output.
    if((b2 == 2) && (b3 == -3) && (b4 == 4) && (b5 == -5)&&
       (b6 == 6) && (b7 == -7) && (l1 == 8) && (b8 == -9) && (l2 == 100000)){
        printf("bytelongMethod OK\n");
    } else {
        printf("bytelongMethod FAIL!\n");
        printf("args are  %d %d %d %d %d %d %ld %d %ld\n", b2, b3, b4, b5, b6, b7, l1, b8, (long)l2);
        printf("should be 2 -3 4 -5 6 -7 8 -9 100000\n");
    }
    
    return b1;
}

extern "C" jchar JNICALL Java_Main_charintMethod(JNIEnv* env, jclass klacc, jchar c1,  jchar c2,
                                                                       jchar c3, jchar c4, jchar c5, jchar c6, jchar c7, jint i1, jchar c8, jint i2) {
    // We use c1 to drive the output.
    if((c2 == 'a') && (c3 == 'b') && (c4 == 'c') && (c5 == 'd')&&
       (c6 == 'e') && (c7 == 'f') && (i1 == 8) && (c8 == 'g') && (i2 == 30000)){
        printf("charintMethod OK\n");
    } else {
        printf("charintMethod FAIL!\n");
        printf("args are  %c %c %c %c %c %c %d %c %d\n", c2, c3, c4, c5, c6, c7,  i1, c8, i2);
        printf("should be a b c d e f 8 g 30000\n");
    }
    
    return c1;
}

extern "C" jchar JNICALL Java_Main_charlongMethod(JNIEnv* env, jclass klacc, jchar c1,  jchar c2,
                                                                     jchar c3, jchar c4, jchar c5, jchar c6, jchar c7, jlong l1, jchar c8, jlong l2) {
    // We use c1 to drive the output.
    if((c2 == 'a') && (c3 == 'b') && (c4 == 'c') && (c5 == 'd')&&
       (c6 == 'e') && (c7 == 'f') && (l1 == 8) && (c8 == 'g') && (l2 == 100000)){
        printf("charintMethod OK\n");
    } else {
        printf("charintMethod FAIL!\n");
        printf("args are  %c %c %c %c %c %c %ld %c %ld\n", c2, c3, c4, c5, c6, c7,  (long)l1, c8, (long)l2);
        printf("should be a b c d e f 8 g 100000n");
    }
    
    return c1;
}

extern "C" jint JNICALL Java_Main_intlongMethod(JNIEnv* env, jclass klacc, jint i1,  jint i2,
                                                                     jint i3, jint i4, jint i5, jint i6, jint i7, jlong l1, jint i8, jlong l2) {
    // We use c1 to drive the output.
    if((i2 == 2) && (i3 == -3) && (i4 == 4) && (i5 == -5)&&
       (i6 == 6) && (i7 == -7) && (l1 == 8) && (i8 == -30000) && (l2 == 100000)){
        printf("intlongMethod OK\n");
    } else {
        printf("intlongMethod FAIL!\n");
        printf("args are  %d %d %d %d %d %d %ld %d %ld\n", i2, i3, i4, i5, i6, i7, (long)l1, i8, (long)l2);
        printf("should be 2 -3 4 -5 6 -7 8 -30000 100000\n");
    }
    
    return i1;
}

extern "C" jint JNICALL Java_Main_intstringMethod(JNIEnv* env, jclass klacc, jint i1,  jint i2,
                                                                    jint i3, jint i4, jint i5, jint i6, jint i7, jstring s1, jint i8, jstring s2) {
    const char* sc1 = env->GetStringUTFChars(s1, JNI_FALSE);
    const char* sc2 = env->GetStringUTFChars(s2, JNI_FALSE);
    // We use c1 to drive the output.
    if((i2 == 2) && (i3 == -3) && (i4 == 4) && (i5 == -5)&&
       (i6 == 6) && (i7 == -7) && (!strcmp(sc1, "abc")) && (i8 == -30000) && (!strcmp(sc2, "def"))){
        printf("intlongMethod OK\n");
    } else {
        printf("intlongMethod FAIL!\n");
        printf("args are  %d %d %d %d %d %d %s %d %s\n", i2, i3, i4, i5, i6, i7, sc1, i8, sc2);
        printf("should be 2 -3 4 -5 6 -7 abc -30000 def\n");
    }
    
    env->ReleaseStringUTFChars(s1, sc1);
    env->ReleaseStringUTFChars(s2, sc2);
    
    return i1;
}