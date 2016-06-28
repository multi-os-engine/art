/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 * * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_RUNTIME_JIT_PROFILE_SAVER_OPTIONS_H_
#define ART_RUNTIME_JIT_PROFILE_SAVER_OPTIONS_H_

#include <string>
#include <ostream>

namespace art {

struct ProfileSaverOptions {
 public:
  static constexpr bool kEnabled = false;
  static constexpr uint32_t kMinSavePeriodMs = 20 * 1000;  // 20 seconds
  static constexpr uint32_t kSaveResolvedClassesDelayMs = 2 * 1000;  // 2 seconds
  // Minimum number of JIT samples during launch to include a method into the profile.
  static constexpr uint32_t kStartupMethodSamples = 1;
  static constexpr uint32_t kMinNrOfMethodsToSave = 10;
  static constexpr uint32_t kMinNrOfClassesToSave = 10;
  static constexpr uint32_t kMinNrOfNotificationBeforeWake = 10;
  static constexpr uint32_t kMaxNrOfNotificationBeforeWake = 50;

  ProfileSaverOptions() :
    enabled_(kEnabled),
    min_save_period_ms_(kMinSavePeriodMs),
    save_resolved_classes_delay_ms_(kSaveResolvedClassesDelayMs),
    startup_method_samples_(kStartupMethodSamples),
    min_nr_of_methods_to_save_(kMinNrOfMethodsToSave),
    min_nr_of_classes_to_save_(kMinNrOfClassesToSave),
    min_nr_of_notification_before_wake_(kMinNrOfNotificationBeforeWake),
    max_nr_of_notification_before_wake_(kMaxNrOfNotificationBeforeWake) {}

  ProfileSaverOptions(
      bool enabled,
      uint32_t min_save_period_ms,
      uint32_t save_resolved_classes_delay_ms,
      uint32_t startup_method_samples,
      uint32_t min_nr_of_methods_to_save,
      uint32_t min_nr_of_classes_to_save,
      uint32_t min_nr_of_notification_before_wake,
      uint32_t max_nr_of_notification_before_wake):
    enabled_(enabled),
    min_save_period_ms_(min_save_period_ms),
    save_resolved_classes_delay_ms_(save_resolved_classes_delay_ms),
    startup_method_samples_(startup_method_samples),
    min_nr_of_methods_to_save_(min_nr_of_methods_to_save),
    min_nr_of_classes_to_save_(min_nr_of_classes_to_save),
    min_nr_of_notification_before_wake_(min_nr_of_notification_before_wake),
    max_nr_of_notification_before_wake_(max_nr_of_notification_before_wake) {}

  bool IsEnabled() const {
    return enabled_;
  }
  void SetEnabled(bool enabled) {
    enabled_ = enabled;
  }

  uint32_t GetMinSavePeriodMs() const {
    return min_save_period_ms_;
  }
  uint32_t GetSaveResolvedClassesDelayMs() const {
    return save_resolved_classes_delay_ms_;
  }
  uint32_t GetStartupMethodSamples() const {
    return startup_method_samples_;
  }
  uint32_t GetMinNrOfMethodsToSave() const {
    return min_nr_of_methods_to_save_;
  }
  uint32_t GetMinNrOfClassesToSave() const {
    return min_nr_of_classes_to_save_;
  }
  uint32_t GetMinNrOfNotificationBeforeWake() const {
    return min_nr_of_notification_before_wake_;
  }
  uint32_t GetMaxNrOfNotificationBeforeWake() const {
    return max_nr_of_notification_before_wake_;
  }

  friend std::ostream & operator<<(std::ostream &os, const ProfileSaverOptions& pso) {
    os << "enabled_" << pso.enabled_
        << ", min_save_period_ms_" << pso.min_save_period_ms_
        << ", save_resolved_classes_delay_ms_" << pso.save_resolved_classes_delay_ms_
        << ", startup_method_samples_" << pso.startup_method_samples_
        << ", min_nr_of_methods_to_save_" << pso.min_nr_of_methods_to_save_
        << ", min_nr_of_classes_to_save_" << pso.min_nr_of_classes_to_save_
        << ", min_nr_of_notification_before_wake_" << pso.min_nr_of_notification_before_wake_
        << ", max_nr_of_notification_before_wake_" << pso.max_nr_of_notification_before_wake_;
    return os;
  }

  bool enabled_;
  uint32_t min_save_period_ms_;
  uint32_t save_resolved_classes_delay_ms_;
  uint32_t startup_method_samples_;
  uint32_t min_nr_of_methods_to_save_;
  uint32_t min_nr_of_classes_to_save_;
  uint32_t min_nr_of_notification_before_wake_;
  uint32_t max_nr_of_notification_before_wake_;
};

}  // namespace art


#endif  // ART_RUNTIME_JIT_PROFILE_SAVER_OPTIONS_H_
