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

#ifndef CPSM_PATH_UTIL_H_
#define CPSM_PATH_UTIL_H_

#include <algorithm>
#include <cstddef>

#include <boost/algorithm/cxx14/mismatch.hpp>
#include <boost/utility/string_ref.hpp>

#include "str_util.h"

namespace cpsm {

// PathTraits type for platform paths.
struct PlatformPathTraits {
  // Returns true if `c` is the conventional coarsest-grained separator of
  // parts in a filename.
  static constexpr bool is_extension_separator(char const c) {
    return c == '.';
  }

  // Returns true if `c` separates path components.
  static constexpr bool is_path_separator(char const c) {
#ifdef _WIN32
    // TODO: Support shellslash
    return c == '\\';
#else
    return c == '/';
#endif
  }
};

// PathTraits type for non-paths.
struct NonPathTraits {
  static constexpr bool is_extension_separator(char const c) { return false; }
  static constexpr bool is_path_separator(char const c) { return false; }
};

// If the given path contains a path separator, returns an iterator to after
// the last path separator. Otherwise returns `first`.
template <typename PathTraits, typename InputIt>
InputIt path_basename(InputIt first, InputIt last) {
  return std::find_if(std::reverse_iterator<InputIt>(last),
                      std::reverse_iterator<InputIt>(first),
                      PathTraits::is_path_separator).base();
}

// Returns the distance (in path components) between the two given paths.
template <typename PathTraits, typename InputIt1, typename InputIt2>
std::size_t path_distance(InputIt1 first1, InputIt2 last1, InputIt2 first2,
                          InputIt2 last2) {
  auto const mm = boost::algorithm::mismatch(first1, last1, first2, last2);
  if (mm.first == last1 && mm.second == last2) {
    return 0;
  }
  return std::count_if(mm.first, last1, PathTraits::is_path_separator) +
         std::count_if(mm.second, last2, PathTraits::is_path_separator) + 1;
}

}  // namespace cpsm

#endif  // CPSM_PATH_UTIL_H_
