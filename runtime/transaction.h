/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_TRANSACTION_H_
#define ART_RUNTIME_TRANSACTION_H_

#include "base/macros.h"
#include "base/mutex.h"
#include "locks.h"
#include "offsets.h"
#include "primitive.h"
#include "root_visitor.h"
#include "safe_map.h"

#include <list>
#include <map>

namespace art {
namespace mirror {
class Array;
class Object;
class String;
}
class InternTable;

class Transaction {
 public:
  Transaction();
  ~Transaction();

  // Record object field changes.
  void RecordWriteField32(mirror::Object* obj, MemberOffset field_offset, uint32_t value,
                          bool is_volatile)
      LOCKS_EXCLUDED(log_lock_);
  void RecordWriteField64(mirror::Object* obj, MemberOffset field_offset, uint64_t value,
                          bool is_volatile)
      LOCKS_EXCLUDED(log_lock_);
  void RecordWriteFieldReference(mirror::Object* obj, MemberOffset field_offset,
                                 mirror::Object* value, bool is_volatile)
      LOCKS_EXCLUDED(log_lock_);

  // Record array change.
  void RecordWriteArray(mirror::Array* array, size_t index, uint64_t value)
      LOCKS_EXCLUDED(log_lock_);

  // Record intern string table changes.
  void RecordStrongStringInsertion(const mirror::String* s, uint32_t hash_code)
      LOCKS_EXCLUDED(log_lock_);
  void RecordWeakStringInsertion(const mirror::String* s, uint32_t hash_code)
      LOCKS_EXCLUDED(log_lock_);
  void RecordStrongStringRemoval(const mirror::String* s, uint32_t hash_code)
      LOCKS_EXCLUDED(log_lock_);
  void RecordWeakStringRemoval(const mirror::String* s, uint32_t hash_code)
      LOCKS_EXCLUDED(log_lock_);

  // Abort transaction by undoing all recorded changes.
  void Abort()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      LOCKS_EXCLUDED(log_lock_);

  void VisitRoots(RootVisitor* visitor, void* arg) LOCKS_EXCLUDED(log_lock_);

 private:
  class ObjectLog {
   public:
    void Log32BitsValue(MemberOffset offset, uint32_t value, bool is_volatile);
    void Log64BitsValue(MemberOffset offset, uint64_t value, bool is_volatile);
    void LogReferenceValue(MemberOffset offset, mirror::Object* obj, bool is_volatile);

    void Undo(mirror::Object* obj) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    void VisitRoots(RootVisitor* visitor, void* arg);

    size_t Size() const {
      return field_values_.size();
    }

   private:
    enum FieldValueKind {
      k32Bits,
      k64Bits,
      kReference
    };
    struct FieldValue {
      // TODO use JValue instead ?
      uint64_t value;
      FieldValueKind kind;
      bool is_volatile;
    };

    void UndoFieldWrite(mirror::Object* obj, MemberOffset field_offset,
                        const FieldValue& field_value) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

    // Maps field's offset to its value.
    std::map<uint32_t, FieldValue> field_values_;
  };

  class ArrayLog {
   public:
    void Log32BitsValue(size_t index, uint32_t value);
    void Log64BitsValue(size_t index, uint64_t value);
    void LogReferenceValue(size_t index, mirror::Object* obj);

    void Undo(mirror::Array* obj) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    void VisitRoots(RootVisitor* visitor, void* arg);

    size_t Size() const {
      return array_values_.size();
    }

   private:
    void UndoArrayWrite(mirror::Array* array, Primitive::Type array_type, size_t index,
                        uint64_t value) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

    // Maps index to value.
    // TODO use JValue instead ?
    std::map<size_t, uint64_t> array_values_;
  };

  class InternStringLog {
   public:
    enum StringKind {
      kStrongString,
      kWeakString
    };
    enum StringOp {
      kInsert,
      kRemove
    };
    InternStringLog(const mirror::String* s, uint32_t hash_code, StringKind kind, StringOp op)
      : str_(s), hash_code_(hash_code), string_kind_(kind), string_op_(op) {
    }

    void Undo(InternTable* intern_table);
    void VisitRoots(RootVisitor* visitor, void* arg);

   private:
    const mirror::String* str_;
    uint32_t hash_code_;
    StringKind string_kind_;
    StringOp string_op_;
  };

  void LogInternedString(InternStringLog& log) LOCKS_EXCLUDED(log_lock_);

  void UndoObjectModifications()
      LOCKS_EXCLUDED(log_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void UndoArrayModifications()
        LOCKS_EXCLUDED(log_lock_)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void UndoInternStringTableModifications();

  void VisitObjectLogs(RootVisitor* visitor, void* arg) LOCKS_EXCLUDED(log_lock_);
  void VisitArrayLogs(RootVisitor* visitor, void* arg) LOCKS_EXCLUDED(log_lock_);
  void VisitStringLogs(RootVisitor* visitor, void* arg);

  Mutex log_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  std::map<mirror::Object*, ObjectLog> object_logs_ GUARDED_BY(log_lock_);
  std::map<mirror::Array*, ArrayLog> array_logs_  GUARDED_BY(log_lock_);
  // TODO this should really be guarded by a lock! But we have a lock ordering issue with the
  // intern table's lock here. When logging a change, we would take our own lock while holding the
  // intern table's lock. When undoing a change (during transaction abort), we would attempt to lock
  // the intern table's lock while already holding our own lock. Maybe we should also use the intern
  // table lock and move it out to global locks?
  std::list<InternStringLog> intern_string_logs_;

  DISALLOW_COPY_AND_ASSIGN(Transaction);
};

}  // namespace art

#endif  // ART_RUNTIME_TRANSACTION_H_
