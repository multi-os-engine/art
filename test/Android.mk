# Copyright (C) 2011 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH := $(call my-dir)

include art/build/Android.common.mk

########################################################################

# subdirectories which are used as inputs for gtests
TEST_DEX_DIRECTORIES := \
	AbstractMethod \
	AllFields \
	ExceptionHandle \
	GetMethodSignature \
	Interfaces \
	Main \
	MyClass \
	MyClassNatives \
	Nested \
	NonStaticLeafMethods \
	ProtoCompare \
	ProtoCompare2 \
	StaticLeafMethods \
	Statics \
	StaticsFromCode \
	Transaction \
	XandY

# subdirectories of which are used with test-art-target-oat
# Declare the simplest tests (Main, HelloWorld) first, the rest are alphabetical
TEST_OAT_DIRECTORIES := \
	Main \
	HelloWorld \
	InterfaceTest \
	JniTest \
	SignalTest \
	NativeAllocations \
	ParallelGC \
	ReferenceMap \
	StackWalk \
	ThreadStress \
	UnsafeTest

# TODO: Enable when the StackWalk2 tests are passing
#	StackWalk2 \

TEST_OAT_NOISY_TESTS := \
	ParallelGC \
	ThreadStress

ART_TEST_TARGET_DEX_FILES :=
ART_TEST_TARGET_DEX_FILES$(ART_PHONY_TEST_TARGET_SUFFIX) :=
ART_TEST_TARGET_DEX_FILES$(2ND_ART_PHONY_TEST_TARGET_SUFFIX) :=
ART_TEST_HOST_DEX_FILES :=

# $(1): module prefix
# $(2): input test directory
# $(3): target output module path (default module path is used on host)
define build-art-test-dex
  ifeq ($(ART_BUILD_TARGET),true)
    include $(CLEAR_VARS)
    LOCAL_MODULE := $(1)-$(2)
    LOCAL_MODULE_TAGS := tests
    LOCAL_SRC_FILES := $(call all-java-files-under, $(2))
    LOCAL_JAVA_LIBRARIES := $(TARGET_CORE_JARS)
    LOCAL_NO_STANDARD_LIBRARIES := true
    LOCAL_MODULE_PATH := $(3)
    LOCAL_DEX_PREOPT_IMAGE_LOCATION := $(TARGET_CORE_IMG_OUT)
    LOCAL_DEX_PREOPT := false
    LOCAL_ADDITIONAL_DEPENDENCIES := art/build/Android.common.mk
    LOCAL_ADDITIONAL_DEPENDENCIES += $(LOCAL_PATH)/Android.mk
    include $(BUILD_JAVA_LIBRARY)

    ART_TEST_TARGET_DEX_FILES += $$(LOCAL_INSTALLED_MODULE)
    ART_TEST_TARGET_DEX_FILES$(ART_PHONY_TEST_TARGET_SUFFIX) += $$(LOCAL_INSTALLED_MODULE)
  endif

  ifeq ($(ART_BUILD_HOST),true)
    include $(CLEAR_VARS)
    LOCAL_MODULE := $(1)-$(2)
    LOCAL_SRC_FILES := $(call all-java-files-under, $(2))
    LOCAL_JAVA_LIBRARIES := $(HOST_CORE_JARS)
    LOCAL_NO_STANDARD_LIBRARIES := true
    LOCAL_DEX_PREOPT_IMAGE := $(HOST_CORE_IMG_LOCATION)
    LOCAL_DEX_PREOPT := false
    LOCAL_ADDITIONAL_DEPENDENCIES := art/build/Android.common.mk
    LOCAL_ADDITIONAL_DEPENDENCIES += $(LOCAL_PATH)/Android.mk
    include $(BUILD_HOST_DALVIK_JAVA_LIBRARY)
    ART_TEST_HOST_DEX_FILES += $$(LOCAL_INSTALLED_MODULE)
  endif
endef
$(foreach dir,$(TEST_DEX_DIRECTORIES), $(eval $(call build-art-test-dex,art-test-dex,$(dir),$(ART_NATIVETEST_OUT))))
$(foreach dir,$(TEST_OAT_DIRECTORIES), $(eval $(call build-art-test-dex,oat-test-dex,$(dir),$(ART_TEST_OUT))))

# Used outside the art project to get a list of the current tests
ART_TEST_DEX_MAKE_TARGETS := $(addprefix art-test-dex-, $(TEST_DEX_DIRECTORIES))
ART_TEST_OAT_MAKE_TARGETS := $(addprefix oat-test-dex-, $(TEST_OAT_DIRECTORIES))

########################################################################

ART_TEST_TARGET_OAT_DEFAULT$(ART_PHONY_TEST_TARGET_SUFFIX)_RULES :=
ART_TEST_TARGET_OAT_DEFAULT$(2ND_ART_PHONY_TEST_TARGET_SUFFIX)_RULES :=
ART_TEST_TARGET_OAT_DEFAULT_RULES :=
ART_TEST_TARGET_OAT_INTERPRETER$(ART_PHONY_TEST_TARGET_SUFFIX)_RULES :=
ART_TEST_TARGET_OAT_INTERPRETER$(2ND_ART_PHONY_TEST_TARGET_SUFFIX)_RULES :=
ART_TEST_TARGET_OAT_INTERPRETER_RULES :=
ART_TEST_TARGET_OAT$(ART_PHONY_TEST_TARGET_SUFFIX)_RULES :=
ART_TEST_TARGET_OAT$(2ND_ART_PHONY_TEST_TARGET_SUFFIX)_RULES :=
ART_TEST_TARGET_OAT_RULES :=

# Define rule to run an individual oat test on the host. Output from the test is written to the
# host in /tmp/android-data in a directory named after test's rule name (its target) and the parent
# process' PID (ie the PID of make). On failure the output is dumped to the console. To test for
# success on the target device a file is created following a successful test and this is pulled
# onto the host. If the pull fails then the file wasn't created because the test failed.
# $(1): directory - the name of the test we're building such as HelloWorld.
# $(2): 2ND_ or undefined - used to differentiate between the primary and secondary architecture.
# $(3): the target (rule name), e.g. test-art-target-oat-default-HelloWorld64
# $(4): -Xint or undefined - do we want to run with the interpreter or default.
define define-test-art-oat-rule-target

.PHONY: $(3)
$(3): $(ART_TEST_OUT)/oat-test-dex-$(1).jar test-art-target-sync
	$(hide) mkdir -p /tmp/android-data/$$@-$$$$PPID
	$(hide) echo Running: $$@
	$(hide) adb shell touch $(ART_TEST_DIR)/$(TARGET_$(2)ARCH)/$$@-$$$$PPID
	$(hide) adb shell rm $(ART_TEST_DIR)/$(TARGET_$(2)ARCH)/$$@-$$$$PPID
	$(hide) adb shell sh -c "/system/bin/dalvikvm$($(2)ART_PHONY_TEST_TARGET_SUFFIX) \
		$(DALVIKVM_FLAGS) $(4) -XXlib:libartd.so -Ximage:$(ART_TEST_DIR)/core.art \
	  -classpath $(ART_TEST_DIR)/oat-test-dex-$(1).jar \
	  -Djava.library.path=$(ART_TEST_DIR)/$(TARGET_$(2)ARCH) $(1) \
	  	&& touch $(ART_TEST_DIR)/$(TARGET_$(2)ARCH)/$$@-$$$$PPID" \
				> /tmp/android-data/$$@-$$$$PPID/output.txt 2>&1
	$(hide) (adb pull $(ART_TEST_DIR)/$(TARGET_$(2)ARCH)/$$@-$$$$PPID /tmp/android-data/$$@-$$$$PPID \
	  && echo $$@ PASSED) \
		|| (cat /tmp/android-data/$$@-$$$$PPID/output.txt && echo $$@ FAILED && exit 1)
	$(hide) rm -r /tmp/android-data/$$@-$$$$PPID

endef  # define-test-art-oat-rule-target

# Define rules to run oat tests on the target.
# $(1): directory - the name of the test we're building such as HelloWorld.
# $(2): 2ND_ or undefined - used to differentiate between the primary and secondary architecture.
define define-test-art-oat-rules-target
  # Define a phony rule to run a target oat test using the default compiler.
  default_test_rule := test-art-target-oat-default-$(1)$($(2)ART_PHONY_TEST_TARGET_SUFFIX)
	$(call define-test-art-oat-rule-target,$(1),$(2),$$(default_test_rule),)

  ART_TEST_TARGET_OAT_DEFAULT$($(2)ART_PHONY_TEST_TARGET_SUFFIX)_RULES += $$(default_test_rule)
  ART_TEST_TARGET_OAT_DEFAULT_RULES += $$(default_test_rule)
  ART_TEST_TARGET_OAT_DEFAULT_$(1)_RULES += $$(default_test_rule)

  # Define a phony rule to run a target oat test using the interpeter.
  interpreter_test_rule := test-art-target-oat-interpreter-$(1)$($(2)ART_PHONY_TEST_TARGET_SUFFIX)
	$(call define-test-art-oat-rule-target,$(1),$(2),$$(interpreter_test_rule),-Xint)

  ART_TEST_TARGET_OAT_INTERPRETER$($(2)ART_PHONY_TEST_TARGET_SUFFIX)_RULES += $$(interpreter_test_rule)
  ART_TEST_TARGET_OAT_INTERPRETER_RULES += $$(interpreter_test_rule)
  ART_TEST_TARGET_OAT_INTERPRETER_$(1)_RULES += $$(interpreter_test_rule)

  # Define a phony rule to run both the default and interpreter variants.
  both_test_rule :=  test-art-target-oat-$(1)$($(2)ART_PHONY_TEST_TARGET_SUFFIX)
.PHONY: $$(both_test_rule)
$$(both_test_rule): $$(default_test_rule) $$(interpreter_test_rule)
	@echo $$@ PASSED

  ART_TEST_TARGET_OAT$($(2)ART_PHONY_TEST_TARGET_SUFFIX)_RULES += $$(both_test_rule)
  ART_TEST_TARGET_OAT_RULES += $$(both_test_rule)
  ART_TEST_TARGET_OAT_$(1)_RULES += $$(both_test_rule)

  # Clear locally used variables.
  interpreter_test_rule :=
  default_test_rule :=
  both_test_rule :=
endef  # define-test-art-oat-rules-target

ART_TEST_HOST_OAT_DEFAULT$(ART_PHONY_TEST_HOST_SUFFIX)_RULES :=
ART_TEST_HOST_OAT_DEFAULT$(2ND_ART_PHONY_TEST_HOST_SUFFIX)_RULES :=
ART_TEST_HOST_OAT_DEFAULT_RULES :=
ART_TEST_HOST_OAT_INTERPRETER$(ART_PHONY_TEST_HOST_SUFFIX)_RULES :=
ART_TEST_HOST_OAT_INTERPRETER$(2ND_ART_PHONY_TEST_HOST_SUFFIX)_RULES :=
ART_TEST_HOST_OAT_INTERPRETER_RULES :=
ART_TEST_HOST_OAT$(ART_PHONY_TEST_HOST_SUFFIX)_RULES :=
ART_TEST_HOST_OAT$(2ND_ART_PHONY_TEST_HOST_SUFFIX)_RULES :=
ART_TEST_HOST_OAT_RULES :=

# Define rule to run an individual oat test on the host. Output from the test is written to the
# host in /tmp/android-data in a directory named after test's rule name (its target) and the parent
# process' PID (ie the PID of make). On failure the output is dumped to the console.
# $(1): directory - the name of the test we're building such as HelloWorld.
# $(2): 2ND_ or undefined - used to differentiate between the primary and secondary architecture.
# $(3): the target (rule name), e.g. test-art-host-oat-default-HelloWorld64
# $(4): file name of odex that will be run, used as a dependency
# $(5): -Xint or undefined
define define-test-art-oat-rule-host

.PHONY: $(3)
$(3): $(4) test-art-host-dependencies
	$(hide) mkdir -p /tmp/android-data/$$@-$$$$PPID
	$(hide) echo Running: $$@
	$(hide) ANDROID_DATA=/tmp/android-data/$$@-$$$$PPID \
	ANDROID_ROOT=$(HOST_OUT) \
	LD_LIBRARY_PATH=$$($(3)HOST_OUT_SHARED_LIBRARIES) \
	$(HOST_OUT_EXECUTABLES)/dalvikvm$$($(2)ART_PHONY_TEST_HOST_SUFFIX) $(DALVIKVM_FLAGS) $(5) \
	    -XXlib:libartd$(HOST_SHLIB_SUFFIX) -Ximage:$$(HOST_CORE_IMG_LOCATION) \
	    -classpath $$(HOST_OUT_JAVA_LIBRARIES)/oat-test-dex-$(1).jar \
	    -Djava.library.path=$$($(2)HOST_OUT_SHARED_LIBRARIES) $(1) \
			  > /tmp/android-data/$$@-$$$$PPID/output.txt 2>&1 \
		&& echo $$@ PASSED \
		|| (cat /tmp/android-data/$$@-$$$$PPID/output.txt && echo $$@ FAILED && exit 1)
	$(hide) rm -r /tmp/android-data/$$@-$$$$PPID

endef  # define-test-art-oat-rule-host

# Define rules to run oat tests on the host.
# $(1): directory - the name of the test we're building such as HelloWorld.
# $(2): 2ND_ or undefined - used to differentiate between the primary and secondary architecture.
define define-test-art-oat-rules-host
  odex_filename := $(HOST_OUT_JAVA_LIBRARIES)/$($(2)HOST_ARCH)/oat-test-dex-$(1).odex
$$(odex_filename): $(HOST_OUT_JAVA_LIBRARIES)/oat-test-dex-$(1).jar $($(2)HOST_CORE_IMG_OUT) | $(DEX2OATD)
	$(hide) $(DEX2OATD) $(DEX2OAT_FLAGS) --runtime-arg -Xms16m --runtime-arg -Xmx16m \
		--boot-image=$$(HOST_CORE_IMG_LOCATION) --dex-file=$$(realpath $$<) --oat-file=$$@ \
		--instruction-set=$($(2)HOST_ARCH) --host --android-root=$(HOST_OUT)

  # Create a rule to run the host oat test with the default compiler.
  default_test_rule := test-art-host-oat-default-$(1)$($(2)ART_PHONY_TEST_HOST_SUFFIX)
  $(call define-test-art-oat-rule-host,$(1),$(2),$$(default_test_rule),$$(odex_filename),)

  ART_TEST_HOST_OAT_DEFAULT$($(2)ART_PHONY_TEST_HOST_SUFFIX)_RULES += $$(default_test_rule)
  ART_TEST_HOST_OAT_DEFAULT_RULES += $$(default_test_rule)
  ART_TEST_HOST_OAT_DEFAULT_$(1)_RULES += $$(default_test_rule)

  # Create a rule to run the host oat test with the interpreter.
  interpreter_test_rule := test-art-host-oat-interpreter-$(1)$($(2)ART_PHONY_TEST_HOST_SUFFIX)
  $(call define-test-art-oat-rule-host,$(1),$(2),$$(interpreter_test_rule),$$(odex_filename),-Xint)

  ART_TEST_HOST_OAT_INTERPRETER$($(2)ART_PHONY_TEST_HOST_SUFFIX)_RULES += $$(interpreter_test_rule)
  ART_TEST_HOST_OAT_INTERPRETER_RULES += $$(interpreter_test_rule)
  ART_TEST_HOST_OAT_INTERPRETER_$(1)_RULES += $$(interpreter_test_rule)

  # Define a phony rule to run both the default and interpreter variants.
  both_test_rule :=  test-art-host-oat-$(1)$($(2)ART_PHONY_TEST_HOST_SUFFIX)
.PHONY: $$(both_test_rule)
$$(both_test_rule): $$(default_test_rule) $$(interpreter_test_rule)
	@echo $$@ PASSED

  ART_TEST_HOST_OAT$($(2)ART_PHONY_TEST_HOST_SUFFIX)_RULES += $$(both_test_rule)
  ART_TEST_HOST_OAT_RULES += $$(both_test_rule)
  ART_TEST_HOST_OAT_$(1)_RULES += $$(both_test_rule)

  # Clear locally used variables.
  odex_filename :=
  interpreter_test_rule :=
  default_test_rule :=
  both_test_rule :=
endef  # define-test-art-oat-rules-host

# Define target and host oat test rules for the differing multilib flavors and default vs
# interpreter runs. The format of the generated rules (for running an individual test) is:
#   test-art-(host|target)-oat-(default|interpreter)-${directory}(32|64)
# The rules are appended to various lists to enable shorter phony build rules to be built.
# $(1): directory
define define-test-art-oat-rules
  # Define target tests.
  ART_TEST_TARGET_OAT_DEFAULT_$(1)_RULES :=
  ART_TEST_TARGET_OAT_INTERPRETER_$(1)_RULES :=
  ART_TEST_TARGET_OAT_$(1)_RULES :=
  $(call define-test-art-oat-rules-target,$(1),)
  ifdef TARGET_2ND_ARCH
    $(call define-test-art-oat-rules-target,$(1),2ND_)
  endif

  # Define host tests.
  ART_TEST_HOST_OAT_DEFAULT_$(1)_RULES :=
  ART_TEST_HOST_OAT_INTERPRETER_$(1)_RULES :=
  ART_TEST_HOST_OAT_$(1)_RULES :=
  $(call define-test-art-oat-rules-host,$(1),)
  ifneq ($(HOST_PREFER_32_BIT),true)
    $(call define-test-art-oat-rules-host,$(1),2ND_)
  endif

  # Define a phony rule to run tests called $(1) with the default compiler on the host.
.PHONY: test-art-host-oat-default-$(1)
test-art-host-oat-default-$(1): $$(ART_TEST_HOST_OAT_DEFAULT_$(1)_RULES)
	@echo $$@ PASSED

  # Define a phony rule to run tests called $(1) with the interpreter on the host.
.PHONY: test-art-host-oat-interpreter-$(1)
test-art-host-oat-interpreter-$(1): $$(ART_TEST_HOST_OAT_INTERPRETER_$(1)_RULES)
	@echo $$@ PASSED

  # Define a phony rule to run tests called $(1) on the host.
.PHONY: test-art-host-oat-$(1)
test-art-host-oat-$(1): $$(ART_TEST_HOST_OAT_$(1)_RULES)
	@echo $$@ PASSED

  # Define a phony rule to run tests called $(1) with the default compiler on the target.
.PHONY: test-art-target-oat-default-$(1)
test-art-target-oat-default-$(1): $$(ART_TEST_TARGET_OAT_DEFAULT_$(1)_RULES)
	@echo $$@ PASSED

  # Define a phony rule to run tests called $(1) with the interpreter on the target.
.PHONY: test-art-target-oat-interpreter-$(1)
test-art-target-oat-interpreter-$(1): $$(ART_TEST_TARGET_OAT_INTERPRETER_$(1)_RULES)
	@echo $$@ PASSED

  # Define a phony rule to run tests called $(1) on the target.
.PHONY: test-art-target-oat-$(1)
test-art-target-oat-$(1): $$(ART_TEST_TARGET_OAT_$(1)_RULES)
	@echo $$@ PASSED

  # Define a phony rule to run tests called $(1) on the host and target.
.PHONY: test-art-oat-$(1)
test-art-oat-$(1): test-art-host-oat-$(1) test-art-target-oat-$(1)
	@echo $$@ PASSED

  # Clear locally used variables.
  ART_TEST_TARGET_OAT_DEFAULT_$(1)_RULES :=
  ART_TEST_TARGET_OAT_INTERPRETER_$(1)_RULES :=
  ART_TEST_TARGET_OAT_$(1)_RULES :=
  ART_TEST_HOST_OAT_DEFAULT_$(1)_RULES :=
  ART_TEST_HOST_OAT_INTERPRETER_$(1)_RULES :=
  ART_TEST_HOST_OAT_$(1)_RULES :=
endef  # define-test-art-oat-rules
$(foreach dir,$(TEST_OAT_DIRECTORIES), $(eval $(call define-test-art-oat-rules,$(dir))))

# Run oat tests on the host with the default compiler.
.PHONY: test-art-host-oat-default
test-art-host-oat-default: $(ART_TEST_HOST_OAT_DEFAULT_RULES)
	@echo $@ PASSED

# Run oat tests on the host with the interpreter.
.PHONY: test-art-host-oat-interpreter
test-art-host-oat-interpreter: $(ART_TEST_HOST_OAT_INTERPRETER_RULES)
	@echo $@ PASSED

# Run oat tests on the host with the primary architecture.
.PHONY: test-art-host-oat$(ART_PHONY_TEST_HOST_SUFFIX)
test-art-host-oat$(ART_PHONY_TEST_HOST_SUFFIX): $(ART_TEST_HOST_OAT$(ART_PHONY_TEST_HOST_SUFFIX)_RULES)
	@echo $@ PASSED

# Run oat tests on the host with the secondary architecture.
ifneq ($(HOST_PREFER_32_BIT),true)
.PHONY: test-art-host-oat$(2ND_ART_PHONY_TEST_HOST_SUFFIX)
test-art-host-oat$(2ND_ART_PHONY_TEST_HOST_SUFFIX): $(ART_TEST_HOST_OAT$(2ND_ART_PHONY_TEST_HOST_SUFFIX)_RULES)
	@echo $@ PASSED
endif

# Run oat tests on the host with both architectures, the default compiler and interpreter.
.PHONY: test-art-host-oat
test-art-host-oat: $(ART_TEST_HOST_OAT_RULES)
	@echo $@ PASSED

# Run oat tests on the target with the default compiler.
.PHONY: test-art-target-oat-default
test-art-target-oat-default: $(ART_TEST_TARGET_OAT_DEFAULT_RULES)
	@echo $@ PASSED

# Run oat tests on the target with the interpreter.
.PHONY: test-art-target-oat-interpreter
test-art-target-oat-interpreter: $(ART_TEST_TARGET_OAT_INTERPRETER_RULES)
	@echo $@ PASSED

# Run oat tests on the target with the primary architecture.
.PHONY: test-art-target-oat$(ART_PHONY_TEST_TARGET_SUFFIX)
test-art-target-oat$(ART_PHONY_TEST_TARGET_SUFFIX): $(ART_TEST_TARGET_OAT$(ART_PHONY_TEST_TARGET_SUFFIX)_RULES)
	@echo $@ PASSED

# Run oat tests on the target with the secondary architecture.
ifdef TARGET_2ND_ARCH
.PHONY: test-art-target-oat$(2ND_ART_PHONY_TEST_TARGET_SUFFIX)
test-art-host-oat$(2ND_ART_PHONY_TEST_TARGET_SUFFIX): $(ART_TEST_TARGET_OAT$(2ND_ART_PHONY_TEST_TARGET_SUFFIX)_RULES)
	@echo $@ PASSED
endif

# Run oat tests on the host with both architectures, the default compiler and interpreter.
.PHONY: test-art-target-oat
test-art-target-oat: $(ART_TEST_TARGET_OAT_RULES)
	@echo $@ PASSED

# Run all oat tests on the host and target.
.PHONY: test-art-oat
test-art-oat: test-art-host-oat test-art-target-oat
	@echo $@ PASSED

# Clear locally used variables.
define-test-art-oat-rule-target :=
define-test-art-oat-rules-target :=
define-test-art-oat-rule-host :=
define-test-art-oat-rules-host :=
ART_TEST_TARGET_OAT_DEFAULT$(ART_PHONY_TEST_TARGET_SUFFIX)_RULES :=
ART_TEST_TARGET_OAT_DEFAULT$(2ND_ART_PHONY_TEST_TARGET_SUFFIX)_RULES :=
ART_TEST_TARGET_OAT_DEFAULT_RULES :=
ART_TEST_TARGET_OAT_INTERPRETER$(ART_PHONY_TEST_TARGET_SUFFIX)_RULES :=
ART_TEST_TARGET_OAT_INTERPRETER$(2ND_ART_PHONY_TEST_TARGET_SUFFIX)_RULES :=
ART_TEST_TARGET_OAT_INTERPRETER_RULES :=
ART_TEST_TARGET_OAT$(ART_PHONY_TEST_TARGET_SUFFIX)_RULES :=
ART_TEST_TARGET_OAT$(2ND_ART_PHONY_TEST_TARGET_SUFFIX)_RULES :=
ART_TEST_TARGET_OAT_RULES :=
ART_TEST_HOST_OAT_DEFAULT$(ART_PHONY_TEST_HOST_SUFFIX)_RULES :=
ART_TEST_HOST_OAT_DEFAULT$(2ND_ART_PHONY_TEST_HOST_SUFFIX)_RULES :=
ART_TEST_HOST_OAT_DEFAULT_RULES :=
ART_TEST_HOST_OAT_INTERPRETER$(ART_PHONY_TEST_HOST_SUFFIX)_RULES :=
ART_TEST_HOST_OAT_INTERPRETER$(2ND_ART_PHONY_TEST_HOST_SUFFIX)_RULES :=
ART_TEST_HOST_OAT_INTERPRETER_RULES :=
ART_TEST_HOST_OAT$(ART_PHONY_TEST_HOST_SUFFIX)_RULES :=
ART_TEST_HOST_OAT$(2ND_ART_PHONY_TEST_HOST_SUFFIX)_RULES :=
ART_TEST_HOST_OAT_RULES :=

########################################################################

TEST_ART_RUN_TEST_MAKE_RULES :=
art_run_tests_dir := $(call intermediates-dir-for,PACKAGING,art-run-tests)/DATA

# Helper to create individual build targets for tests.
# Must be called with $(eval)
# $(1): the test number
define define-build-art-run-test
dmart_target := $(art_run_tests_dir)/art-run-tests/$(1)/touch
$$(dmart_target): $(DX) $(HOST_OUT_EXECUTABLES)/jasmin
	$(hide) rm -rf $$(dir $$@) && mkdir -p $$(dir $$@)
	$(hide) DX=$(abspath $(DX)) JASMIN=$(abspath $(HOST_OUT_EXECUTABLES)/jasmin) \
		$(LOCAL_PATH)/run-test --build-only --output-path $$(abspath $$(dir $$@)) $(1)
	$(hide) touch $$@

TEST_ART_RUN_TEST_MAKE_RULES += $$(dmart_target)
dmart_target :=
endef

# Expand all tests.
TEST_ART_RUN_TESTS := $(wildcard $(LOCAL_PATH)/[0-9]*)
TEST_ART_RUN_TESTS := $(subst $(LOCAL_PATH)/,, $(TEST_ART_RUN_TESTS))
TEST_ART_TIMING_SENSITIVE_RUN_TESTS := 053-wait-some 055-enum-performance
ifdef dist_goal # disable timing sensitive tests on "dist" builds.
  $(foreach test, $(TEST_ART_TIMING_SENSITIVE_RUN_TESTS), \
    $(info Skipping $(test)) \
    $(eval TEST_ART_RUN_TESTS := $(filter-out $(test), $(TEST_ART_RUN_TESTS))))
endif
$(foreach test, $(TEST_ART_RUN_TESTS), $(eval $(call define-build-art-run-test,$(test))))

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE := art-run-tests
LOCAL_ADDITIONAL_DEPENDENCIES := $(TEST_ART_RUN_TEST_MAKE_RULES)
# The build system use this flag to pick up files generated by declare-make-art-run-test.
LOCAL_PICKUP_FILES := $(art_run_tests_dir)

include $(BUILD_PHONY_PACKAGE)

# clear temp vars
TEST_ART_RUN_TEST_MAKE_TARGETS :=
define-build-art-run-test :=

########################################################################
