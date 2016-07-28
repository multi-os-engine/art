/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "agent.h"
#include "java_vm_ext.h"
#include "runtime.h"

namespace art {
namespace ti {

std::ostream& operator<<(std::ostream &os, const Agent* m) {
  return os << *m;
}

std::ostream& operator<<(std::ostream &os, Agent const& m) {
  return os << "Agent { name=\"" << m.name_ << "\", args=\"" << m.args_ << "\", handle="
            << m.dlopen_handle_ << " }";
}

Agent Agent::Create(std::string arg) {
  size_t eq = arg.find_first_of('=');
  if (eq == std::string::npos) {
    return Agent(arg, "");
  } else {
    return Agent(arg.substr(0, eq), arg.substr(eq + 1, arg.length()));
  }
}

// TODO We need to acquire some locks probably.
Agent::LoadError Agent::Load(/*out*/jint* call_res, /*out*/ std::string* error_msg) {
  if (IsStarted()) {
    *error_msg = StringPrintf("the agent at %s has already been started!", name_.c_str());
    LOG(ERROR) << "err: " << *error_msg;
    return AlreadyStarted;
  }
  LoadError err = DoDlOpen(error_msg);
  if (err != NoError) {
    LOG(ERROR) << "err: " << *error_msg;
    return err;
  }
  if (onload_ == nullptr) {
    *error_msg = StringPrintf("Unable to start agent %s: No Agent_OnLoad function found",
                              name_.c_str());
    LOG(ERROR) << "err: " << *error_msg;
    return LoadingError;
  }
  // TODO Need to do some checks that we are at a good spot etc.
  *call_res = onload_(static_cast<JavaVM*>(Runtime::Current()->GetJavaVM()),
                      args_.c_str(),
                      nullptr);
  if (*call_res != 0) {
    *error_msg = StringPrintf("Initialization of %s returned non-zero value of %d",
                              name_.c_str(), *call_res);
    LOG(ERROR) << "err: " << *error_msg;
    return InitializationError;
  } else {
    return NoError;
  }
}

Agent::LoadError Agent::DoDlOpen(/*out*/std::string* error_msg) {
  dlopen_handle_ = dlopen(name_.c_str(), RTLD_LAZY);
  if (dlopen_handle_ == nullptr) {
    *error_msg = StringPrintf("Unable to dlopen %s: %s", name_.c_str(), dlerror());
    return LoadingError;
  }

  onload_ = reinterpret_cast<AgentOnLoadFunction>(dlsym(dlopen_handle_, "Agent_OnLoad"));
  if (onload_ == nullptr) {
    // TODO Make this a vlog.
    LOG(WARNING) << "Unable to find 'Agent_OnLoad' symbol in " << this;
  }
  onattach_ = reinterpret_cast<AgentOnAttachFunction>(dlsym(dlopen_handle_, "Agent_OnAttach"));
  if (onattach_ == nullptr) {
    // TODO Make this a vlog.
    LOG(WARNING) << "Unable to find 'Agent_OnAttach' symbol in " << this;
  }
  onunload_= reinterpret_cast<AgentOnUnloadFunction>(dlsym(dlopen_handle_, "Agent_OnUnload"));
  if (onunload_ == nullptr) {
    // TODO Make this a vlog.
    LOG(WARNING) << "Unable to find 'Agent_OnUnload' symbol in " << this;
  }
  return NoError;
}

// TODO Lock some stuff probably.
void Agent::Unload() {
  if (dlopen_handle_ != nullptr) {
    if (onunload_ != nullptr) {
      onunload_(Runtime::Current()->GetJavaVM());
    }
    dlclose(dlopen_handle_);
    dlopen_handle_ = nullptr;
  } else {
    LOG(WARNING) << this << " is not currently loaded!";
  }
}

Agent::Agent(const Agent& other)
  : name_(other.name_),
    args_(other.args_),
    dlopen_handle_(other.dlopen_handle_),
    onload_(other.onload_),
    onattach_(other.onattach_),
    onunload_(other.onunload_) {
  if (other.dlopen_handle_ != nullptr) {
    dlopen(other.name_.c_str(), 0);
  }
}

Agent::~Agent() {
  if (dlopen_handle_ != nullptr) {
    dlclose(dlopen_handle_);
  }
}

}  // namespace ti
}  // namespace art
