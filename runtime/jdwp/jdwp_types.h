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

#ifndef ART_RUNTIME_JDWP_JDWP_TYPES_H_
#define ART_RUNTIME_JDWP_JDWP_TYPES_H_

namespace art {

namespace JDWP {

/*
 * Fundamental types.
 *
 * ObjectId and RefTypeId must be the same size.
 * Its OK to change MethodId and FieldId sizes as long as the size is <= 8 bytes.
 * Note that ArtFields are 64 bit pointers on 64 bit targets. So this one must remain 8 bytes.
 */
typedef uint64_t FieldId;     /* static or instance field */
typedef uint64_t MethodId;    /* any kind of method, including constructors */
typedef uint64_t ObjectId;    /* any object (threadID, stringID, arrayID, etc) */
typedef uint64_t RefTypeId;   /* like ObjectID, but unique for Class objects */
typedef uint64_t FrameId;     /* short-lived stack frame ID */

}  // namespace JDWP

}  // namespace art

#endif  // ART_RUNTIME_JDWP_JDWP_TYPES_H_
