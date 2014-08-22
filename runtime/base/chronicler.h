/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_BASE_CHRONICLER_H_
#define ART_RUNTIME_BASE_CHRONICLER_H_

#include "base/mutex.h"

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>

namespace art {
namespace base {

#if defined(WITH_CHRONICLER)

#define CHRONICLER_LOG_EVENT(action, type, instance, extra) ::art::base::Chronicler::GetInstance()->LogEvent(action, type, instance, extra)
#define CHRONICLER_FLUSH_LOCAL_BUFFERS() ::art::base::Chronicler::GetInstance()->FlushCurrentThreadBuffer()
#define CHRONICLER_FLUSH_ALL_BUFFERS() ::art::base::Chronicler::GetInstance()->FlushAllThreadBuffers()


#define MAX_BUFFER_SIZE 1024

        class Chronicler {
            public:
                enum Action {
                    BEGIN = 0,
                    END,
                    MID
                };
                static const char* GetActionString(Chronicler::Action action);

                Chronicler();
                ~Chronicler();

                // Returns the Singleton instance.
                static Chronicler* GetInstance();

                void LogEvent(Action action, const char* type, const char* instance, const char* extra);
                void FlushAllThreadBuffers();
                // void FlushCurrentThreadBuffer();

                std::fstream* GetOutputFileHandle();

            private:
                // Singleton class utility members.
                static Chronicler* instance_;
                static bool isInited_;


                class EventList {
                    public:
                        EventList();

                        void AddRecord(Chronicler::Action action,
                                        const char* type,
                                        const char* instance,
                                        const char* extra);
                        void FlushListToFile(std::fstream* logFile_);
                        pid_t GetListOwnerTid();

                    private:
                        struct EventRecord {
                            uint64_t timestamp;
                            Chronicler::Action action;
                            const char* type;
                            const char* instance;
                            const char* extra;
                        }  list_[MAX_BUFFER_SIZE];

                        pid_t pid_;
                        pid_t tid_;
                        int count_;
                        int list_max_size_;

                        uint64_t LocalGetTimestamp();
                };

                pid_t pid_;
                // Use thread-local storage to avoid locks.
                static __thread EventList* event_list_;

                // Keeps a record of all allocated thread-local lists.
                std::vector<EventList**> thread_registry_;

                // Mutex for modifying the thread registry.
                mutable Mutex registry_mutex_;

                // The output file to flush the contents.
                std::fstream log_file_;

                // Mutex for Writing into the Log file.
                mutable Mutex log_file_mutex_;

                void DebugLog(std::string str);
                void InitThreadLocalState();
        };

        // Mutex for Global Object Initialization.
        extern Mutex chroniclerInitMutex;
#else

#define CHRONICLER_LOG_EVENT(action, type, instance, extra)
#define CHRONICLER_FLUSH_LOCAL_BUFFERS()
#define CHRONICLER_FLUSH_ALL_BUFFERS()

#endif  // if defined(WITH_CHRONICLER)

}  // namespace base
}  // namespace art

#endif  // ART_RUNTIME_BASE_CHRONICLER_H_
