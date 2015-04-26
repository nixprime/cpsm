// cpsm - fuzzy path matcher
// Copyright (C) 2015 Jamie Liu
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

// Standard scopeguard idiom. Example usage:
//
//   Py_INCREF(rv);
//   auto rv_decref = defer([rv]() { Py_DECREF(rv); );
//   ...
//   if (!...) {
//     return nullptr;
//   }
//   ...
//   rv_decref.cancel();
//   return rv;

#ifndef CPSM_DEFER_H_
#define CPSM_DEFER_H_

#include <utility>

namespace cpsm {

template <typename F>
class Deferred {
  public:
    explicit Deferred(F f) : f_(std::move(f)), cancelled_(false) {}

    ~Deferred() { if (!cancelled_) f_(); }

    void cancel() { cancelled_ = true; }

  private:
    F f_;
    bool cancelled_;
};

template <typename F>
Deferred<F> defer(F f) {
  return Deferred<F>(f);
}

} // namespace cpsm

#endif /* CPSM_DEFER_H_ */
