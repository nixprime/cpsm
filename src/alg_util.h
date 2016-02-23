// cpsm - fuzzy path matcher
// Copyright (C) 2016 Jamie Liu
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

#ifndef CPSM_ALG_UTIL_H_
#define CPSM_ALG_UTIL_H_

#include <utility>

namespace cpsm {

// C++14-ish 4-iterator version of `mismatch` which allows both ranges to be
// bounded.
template <typename InputIt1, typename InputIt2>
std::pair<InputIt1, InputIt2> mismatch(InputIt1 first1, InputIt1 last1,
                                       InputIt2 first2, InputIt2 last2) {
  while (first1 != last1 && first2 != last2 && *first1 == *first2) {
    ++first1;
    ++first2;
  }
  return std::make_pair(first1, first2);
}

}  // namespace cpsm

#endif  // CPSM_ALG_UTIL_H_
