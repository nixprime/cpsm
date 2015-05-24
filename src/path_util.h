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

#ifndef CPSM_PATH_UTIL_H_
#define CPSM_PATH_UTIL_H_

#include <vector>

#include <boost/utility/string_ref.hpp>

#include "str_util.h"

namespace cpsm {

// Returns the platform path separator.
constexpr char32_t path_separator() {
  return '/';
}

// Returns the part of the given path after the final (rightmost) path
// separator.
boost::string_ref path_basename(boost::string_ref path);

// Splits a path into a list of path components. Components include trailing
// path separators, and no "normalization" is done on the path, such that
// concatenating path components results in the original path.
std::vector<boost::string_ref> path_components_of(boost::string_ref path);

// Returns the distance between two paths that have both been decomposed into
// path components. The two paths must share the same root, or both be
// absolute.
CharCount path_distance_between(std::vector<boost::string_ref> const& x,
                                std::vector<boost::string_ref> const& y);

}  // namespace cpsm

#endif /* CPSM_PATH_UTIL_H_ */
