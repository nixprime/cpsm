// cpsm - fuzzy path matcher
// Copyright (C) 2015 the Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CPSM_PAR_UTIL_H_
#define CPSM_PAR_UTIL_H_

#include <thread>
#include <utility>
#include <vector>

#include "str_util.h"

namespace cpsm {

// Drop-in replacement wrapper around std::thread that handles exceptions safely
// and joins on destruction. (See
// https://akrzemi1.wordpress.com/2012/11/14/not-using-stdthread/.)
class Thread {
 public:
  Thread() : has_exception_msg_(false) {}

  Thread(Thread&& other) = default;

  template <typename F, typename... Args>
  explicit Thread(F&& f, Args&&... args)
      : has_exception_msg_(false),
        thread_(&run<F, Args...>, this, f, std::forward(args)...) {}

  ~Thread() {
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  void swap(Thread& other) { thread_.swap(other.thread_); }

  bool joinable() const { return thread_.joinable(); }
  std::thread::id get_id() const { return thread_.get_id(); }
  std::thread::native_handle_type native_handle() {
    return thread_.native_handle();
  }
  static unsigned hardware_concurrency() {
    return std::thread::hardware_concurrency();
  }

  void join() { thread_.join(); }
  void detach() { thread_.detach(); }

  bool has_exception() const { return has_exception_msg_; }
  std::string const& exception_msg() const { return exception_msg_; }

 private:
  template <typename F, typename... Args>
  static void run(Thread* thread, F const& f, Args&&... args) {
    try {
      f(std::forward(args)...);
    } catch (std::exception const& ex) {
      thread->exception_msg_ = ex.what();
      thread->has_exception_msg_ = true;
    } catch (...) {
      thread->exception_msg_ = "(unknown exception)";
      thread->has_exception_msg_ = true;
    }
  }

  std::string exception_msg_;
  bool has_exception_msg_;
  std::thread thread_;
};

void swap(Thread& x, Thread& y) {
  x.swap(y);
}

}  // namespace cpsm

#endif /* CPSM_PAR_UTIL_H_ */
