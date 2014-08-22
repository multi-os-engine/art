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

#include <stdio.h>
#include "thread.h"
#include "base/stl_util.h"
#include <fstream>
#include <string>
#include <sstream>
#include "runtime.h"
#include "thread-inl.h"
#include "chronicler.h"

namespace art {
namespace base {

#if defined(WITH_CHRONICLER)

        // Initialize the Global mutex.
        Mutex chroniclerInitMutex("ObjInitMutex");

        // Initialize all static members.
        Chronicler* Chronicler::instance_ = nullptr;
        bool Chronicler::isInited_ = false;
        __thread Chronicler::EventList* Chronicler::event_list_ = nullptr;


        Chronicler* Chronicler::GetInstance() {
            // Early-out if object already exists.
            if (isInited_) {
                return instance_;
            }

            // Acquire lock to make sure you are only one creating the object.
            MutexLock mu(Thread::Current(), chroniclerInitMutex);

            // Make sure object is still not initialized.
            if (isInited_) {
                // some other thread took care of it. We no longer need to do anything.
                return instance_;
            }

            instance_ = new Chronicler();
            isInited_ =  true;
            return instance_;
        }

        Chronicler::Chronicler(): registry_mutex_("RegistryMutex"), log_file_mutex_("LogFileMutex") {
            pid_ = getpid();

            /*
             * TODO: add an android property to optionally
             * specify the results directory
             */
            std::stringstream ss;
            ss << "/data/art-chronicler/art-chronicle-" << pid_ << ".csv";

            MutexLock mu(Thread::Current(), log_file_mutex_);

            log_file_.open(ss.str().c_str(), std::fstream::out | std::fstream::app);

            // Print column headers in file.
            std::string header_str = "PID|TID|Timestamp|Action|Type|Instance|Extra\n";
            log_file_.write(header_str.c_str(), header_str.length());
            log_file_.flush();
        }

        Chronicler::~Chronicler() {
            // Flush all thread local buffers.
            FlushAllThreadBuffers();

            // Iterate over the thread registry and free all the
            // thread-local buffers.
            // Also remove the element from the list
            while (!thread_registry_.empty()) {
                EventList** item = thread_registry_.back();
                delete *item;
                *item = nullptr;
                thread_registry_.pop_back();
            }

            // close the log file.
            log_file_.close();

            // Reset Singleton members.
            Chronicler::isInited_ = false;
        }

        void Chronicler::InitThreadLocalState() {
            // FIXME: Assert event_list_
            Chronicler::event_list_ = new EventList;

            // Acquire scoped registry-lock to add info into registry.
            MutexLock mu(Thread::Current(), registry_mutex_);
            thread_registry_.push_back(&Chronicler::event_list_);
        }

        void Chronicler::LogEvent(Action action, const char* type, const char* instance, const char* extra) {
            if (event_list_ == nullptr) {
                InitThreadLocalState();
            }

            // Add the record to the Thread-local list.
            event_list_->AddRecord(action, type, instance, extra);
        }

        /*
         * Not thread safe. Call only when you are absolutely sure that
         * no other thread is logging.
         */
        void Chronicler::FlushAllThreadBuffers() {
            std::vector<EventList**>::const_iterator iter;
            for (iter = thread_registry_.begin() ; iter != thread_registry_.end() ; ++iter) {
                (**iter)->FlushListToFile(&log_file_);
            }
        }

        std::fstream* Chronicler::GetOutputFileHandle() {
            return &log_file_;
        }

        Chronicler::EventList::EventList() {
            count_ = 0;
            pid_ = getpid();
            tid_ = art::GetTid();
            list_max_size_ = MAX_BUFFER_SIZE;
        }

        pid_t Chronicler::EventList::GetListOwnerTid() {
            return tid_;
        }

        void Chronicler::EventList::AddRecord(Chronicler::Action action, const char* type, const char* instance, const char* extra) {
            // Get time stamp before possible file write for accurate measurement.
            uint64_t timestamp = LocalGetTimestamp();

            // If the list is already full, flush it to the log-file before adding more records to it.
            if (count_ >= list_max_size_) {
                FlushListToFile(Chronicler::GetInstance()->GetOutputFileHandle());
            }

            EventRecord& current_record = list_[count_];
            current_record.timestamp = timestamp;
            current_record.action = action;
            current_record.type = type;
            current_record.instance = instance;
            current_record.extra = extra;
            count_++;
        }

        const char* Chronicler::GetActionString(Chronicler::Action action) {
            switch (action) {
                case Chronicler::BEGIN: return "BEGIN";
                case Chronicler::END: return "END";
                case Chronicler::MID: return "MID";
                // Default should never occur.
                default:
                    return "Unknown";
            }
        }

        void Chronicler::EventList::FlushListToFile(std::fstream* log_file) {
            // Make a note of time before dumping contents.
            uint64_t timestamp = LocalGetTimestamp();

            // Acquire scoped lock for write access to file.
            MutexLock mu(Thread::Current(), Chronicler::GetInstance()->log_file_mutex_);

            std::stringstream ss;

            // Write a DUMP_BEGIN record to the file.
            ss << pid_ << "|"
                    << tid_ << "|"
                    << timestamp << "|"
                    << "DUMP_BEGIN" << "|"
                    << "Dump to file" << "|"
                    << "" << "|"
                    << "" << "|\n";
            log_file->write(ss.str().c_str(), ss.str().length());
            ss.str("");


            // Write the list contents to the file.
            EventRecord* it;

            for (int i = 0; i < count_; i++) {
                it = &(list_[i]);

                // Make a string out of the record object and write it to the file.
                ss << pid_ << "|"
                    << tid_ << "|"
                    << it->timestamp << "|"
                    << Chronicler::GetActionString(it->action) << "|"
                    << it->type << "|"
                    << it->instance << "|"
                    << it->extra << "|\n";

                log_file->write(ss.str().c_str(), ss.str().length());
                ss.str("");
            }

            // Write a DUMP_END record to the file.
            timestamp = LocalGetTimestamp();
            ss << pid_ << "|"
                    << tid_ << "|"
                    << timestamp << "|"
                    << "DUMP_BEGIN" << "|"
                    << "Dump to file" << "|"
                    << "" << "|"
                    << "" << "|\n";
            log_file->write(ss.str().c_str(), ss.str().length());
            ss.str("");

            log_file->flush();

            // Reset the list count to 0.
            count_ = 0;
        }


        inline uint64_t Chronicler::EventList::LocalGetTimestamp() {
            // Currently returns system wall-clock time.
            return NanoTime();
        }

#endif  // if defined(WITH_CHRONICLER)
}  // namespace base
}  // namespace art
