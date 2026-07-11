/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2015 Sony Mobile Communications Inc.
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
 *
 * NOTE: This file has been modified by Sony Mobile Communications Inc.
 * Modifications are licensed under the License.
 */

#include "signal_catcher.h"

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <sstream>

#include "arch/instruction_set.h"
#include "base/time_utils.h"
#include "base/unix_file/fd_file.h"
#include "class_linker.h"
#include "gc/heap.h"
#include "jit/profile_saver.h"
#include "os.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "signal_set.h"
#include "thread.h"
#include "thread_list.h"
#include "utils.h"

namespace art {

static void DumpCmdLine(std::ostream& os) {
#if defined(__linux__)
  // Show the original command line, and the current command line too if it's changed.
  // On Android, /proc/self/cmdline will have been rewritten to something like "system_server".
  // Note: The string "Cmd line:" is chosen to match the format used by debuggerd.
  std::string current_cmd_line;
  if (ReadFileToString("/proc/self/cmdline", &current_cmd_line)) {
    current_cmd_line.resize(current_cmd_line.find_last_not_of('\0') + 1);  // trim trailing '\0's
    std::replace(current_cmd_line.begin(), current_cmd_line.end(), '\0', ' ');

    os << "Cmd line: " << current_cmd_line << "\n";
    const char* stashed_cmd_line = GetCmdLine();
    if (stashed_cmd_line != nullptr && current_cmd_line != stashed_cmd_line
            && strcmp(stashed_cmd_line, "<unset>") != 0) {
      os << "Original command line: " << stashed_cmd_line << "\n";
    }
  }
#else
  os << "Cmd line: " << GetCmdLine() << "\n";
#endif
}

SignalCatcher::SignalCatcher(const std::string& stack_trace_file)
    : stack_trace_file_(stack_trace_file),
      lock_("SignalCatcher lock"),
      cond_("SignalCatcher::cond_", lock_),
      thread_(nullptr) {
  SetHaltFlag(false);

  // Create a raw pthread; its start routine will attach to the runtime.
  CHECK_PTHREAD_CALL(pthread_create, (&pthread_, nullptr, &Run, this), "signal catcher thread");

  Thread* self = Thread::Current();
  MutexLock mu(self, lock_);
  while (thread_ == nullptr) {
    cond_.Wait(self);
  }
}

SignalCatcher::~SignalCatcher() {
  // Since we know the thread is just sitting around waiting for signals
  // to arrive, send it one.
  SetHaltFlag(true);
  CHECK_PTHREAD_CALL(pthread_kill, (pthread_, SIGQUIT), "signal catcher shutdown");
  CHECK_PTHREAD_CALL(pthread_join, (pthread_, nullptr), "signal catcher shutdown");
}

void SignalCatcher::SetHaltFlag(bool new_value) {
  MutexLock mu(Thread::Current(), lock_);
  halt_ = new_value;
}

bool SignalCatcher::ShouldHalt() {
  MutexLock mu(Thread::Current(), lock_);
  return halt_;
}

// Cook the intermediate stack trace string to final output strings.
// Output the full stack trace string and the mini stack traces string.
// Arguments:
//   s: (input) intermediate stack trace string. It may has the intermediate
//      tags TAG_BEGIN_MINOR_LOG and TAG_END_MINOR_LOG to indicate minor part.
//      - TAG_BEGIN_MINOR_LOG and TAG_END_MINOR_LOG should be paired.
//      - TAG_BEGIN_MINOR_LOG and TAG_END_MINOR_LOG shuold not be nested.
//   full_str: (output) full stack trace text.
//   mini_str: (output) mini stack trace text that minor parts are removed.
void SignalCatcher::CookOutputString(const std::string& s,
                                     std::string& full_str, std::string& mini_str) {
  std::string str_tag_begin_minor(TAG_BEGIN_MINOR_LOG);
  std::string str_tag_end_minor(TAG_END_MINOR_LOG);
  size_t len_tag_begin_minor = str_tag_begin_minor.length();
  size_t len_tag_end_minor = str_tag_end_minor.length();
  size_t len_source_str = s.length();

  std::ostringstream full_trace_os;
  std::ostringstream mini_trace_os;

  bool in_minor = false;
  bool prev_in_minor = false;
  bool end = false;
  size_t clip_spos = 0;
  size_t clip_epos = 0;
  size_t tag_len = 0;

  while (!end) {
    prev_in_minor = in_minor;
    if (!in_minor) {
      size_t tag_head_pos = s.find(TAG_BEGIN_MINOR_LOG, clip_spos);
      if (tag_head_pos != std::string::npos) {
        in_minor = true;
        clip_epos = tag_head_pos;
        tag_len = len_tag_begin_minor;
      } else {
        clip_epos = len_source_str;
        end = true;
      }
    } else {
      size_t tag_head_pos = s.find(TAG_END_MINOR_LOG, clip_spos);
      if (tag_head_pos != std::string::npos) {
        in_minor = false;
        clip_epos = tag_head_pos;
        tag_len = len_tag_end_minor;
      } else {
        clip_epos = len_source_str;
        end = true;
      }
    }

    if (clip_epos > clip_spos) {
      std::string part_str = s.substr(clip_spos, clip_epos - clip_spos);

      full_trace_os << part_str;
      if (!prev_in_minor) {
        mini_trace_os << part_str;
      }
    }

    clip_spos = clip_epos + tag_len;
    tag_len = 0;
  }

  full_str = full_trace_os.str();
  mini_str = mini_trace_os.str();
}

void SignalCatcher::Output(const std::string& s) {
  std::string full_trace;
  std::string mini_trace;
  CookOutputString(s, full_trace, mini_trace);

  if (stack_trace_file_.empty()) {
    LOG(INFO) << full_trace;
    return;
  }

  ScopedThreadStateChange tsc(Thread::Current(), kWaitingForSignalCatcherOutput);
  // Output mini traces first.
  std::string mini_stack_trace_file(stack_trace_file_ + ".mini");
  std::unique_ptr<File> mini_file(new File(mini_stack_trace_file, O_APPEND | O_CREAT | O_WRONLY, 0666, true));
  if (!mini_file->IsOpened()) {
    PLOG(ERROR) << "Unable to open mini stack trace file '" << mini_stack_trace_file << "'";
  } else {
    bool success = mini_file->WriteFully(mini_trace.data(), mini_trace.size());
    if (success) {
      success = mini_file->FlushCloseOrErase() == 0;
    } else {
      mini_file->Erase();
    }
    if (success) {
      LOG(INFO) << "Wrote mini stack traces to '" << mini_stack_trace_file << "'";
    } else {
      PLOG(ERROR) << "Failed to write mini stack traces to '" << mini_stack_trace_file << "'";
    }
  }

  // Write full traces last, the user component will wait for writing of this file.
  int fd = open(stack_trace_file_.c_str(), O_APPEND | O_CREAT | O_WRONLY, 0666);
  if (fd == -1) {
    PLOG(ERROR) << "Unable to open stack trace file '" << stack_trace_file_ << "'";
    return;
  }
  std::unique_ptr<File> file(new File(fd, stack_trace_file_, true));
  bool success = file->WriteFully(full_trace.data(), full_trace.size());
  if (success) {
    success = file->FlushCloseOrErase() == 0;
  } else {
    file->Erase();
  }
  if (success) {
    LOG(INFO) << "Wrote stack traces to '" << stack_trace_file_ << "'";
  } else {
    PLOG(ERROR) << "Failed to write stack traces to '" << stack_trace_file_ << "'";
  }
}

void SignalCatcher::HandleSigQuit() {
  Runtime* runtime = Runtime::Current();
  std::ostringstream os;
  os << "\n"
      << "----- pid " << getpid() << " at " << GetIsoDate() << " -----\n";

  DumpCmdLine(os);

  // Note: The strings "Build fingerprint:" and "ABI:" are chosen to match the format used by
  // debuggerd. This allows, for example, the stack tool to work.
  std::string fingerprint = runtime->GetFingerprint();
  os << "Build fingerprint: '" << (fingerprint.empty() ? "unknown" : fingerprint) << "'\n";
  os << "ABI: '" << GetInstructionSetString(runtime->GetInstructionSet()) << "'\n";

  os << "Build type: " << (kIsDebugBuild ? "debug" : "optimized") << "\n";

  runtime->DumpForSigQuit(os);

  if ((false)) {
    std::string maps;
    if (ReadFileToString("/proc/self/maps", &maps)) {
      os << "/proc/self/maps:\n" << maps;
    }
  }
  os << "----- end " << getpid() << " -----\n";
  Output(os.str());
}

void SignalCatcher::HandleSigUsr1() {
  LOG(INFO) << "SIGUSR1 forcing GC (no HPROF) and profile save";
  Runtime::Current()->GetHeap()->CollectGarbage(false);
  ProfileSaver::ForceProcessProfiles();
}

int SignalCatcher::WaitForSignal(Thread* self, SignalSet& signals) {
  ScopedThreadStateChange tsc(self, kWaitingInMainSignalCatcherLoop);

  // Signals for sigwait() must be blocked but not ignored.  We
  // block signals like SIGQUIT for all threads, so the condition
  // is met.  When the signal hits, we wake up, without any signal
  // handlers being invoked.
  int signal_number = signals.Wait();
  if (!ShouldHalt()) {
    // Let the user know we got the signal, just in case the system's too screwed for us to
    // actually do what they want us to do...
    LOG(INFO) << *self << ": reacting to signal " << signal_number;

    // If anyone's holding locks (which might prevent us from getting back into state Runnable), say so...
    Runtime::Current()->DumpLockHolders(LOG_STREAM(INFO));
  }

  return signal_number;
}

void* SignalCatcher::Run(void* arg) {
  SignalCatcher* signal_catcher = reinterpret_cast<SignalCatcher*>(arg);
  CHECK(signal_catcher != nullptr);

  Runtime* runtime = Runtime::Current();
  CHECK(runtime->AttachCurrentThread("Signal Catcher", true, runtime->GetSystemThreadGroup(),
                                     !runtime->IsAotCompiler()));

  Thread* self = Thread::Current();
  DCHECK_NE(self->GetState(), kRunnable);
  {
    MutexLock mu(self, signal_catcher->lock_);
    signal_catcher->thread_ = self;
    signal_catcher->cond_.Broadcast(self);
  }

  // Set up mask with signals we want to handle.
  SignalSet signals;
  signals.Add(SIGQUIT);
  signals.Add(SIGUSR1);

  while (true) {
    int signal_number = signal_catcher->WaitForSignal(self, signals);
    if (signal_catcher->ShouldHalt()) {
      runtime->DetachCurrentThread();
      return nullptr;
    }

    switch (signal_number) {
    case SIGQUIT:
      signal_catcher->HandleSigQuit();
      break;
    case SIGUSR1:
      signal_catcher->HandleSigUsr1();
      break;
    default:
      LOG(ERROR) << "Unexpected signal %d" << signal_number;
      break;
    }
  }
}

}  // namespace art
