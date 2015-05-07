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

#include "path_util.h"

#include <algorithm>

// TODO: Support non-Unix non-UTF-8 paths.

namespace cpsm {

boost::string_ref path_basename(boost::string_ref const path) {
  auto const pos = path.find_last_of('/');
  if (pos != boost::string_ref::npos) {
    return path.substr(pos + 1);
  }
  return path;
}

std::vector<boost::string_ref> path_components_of(boost::string_ref path) {
  std::vector<boost::string_ref> parts;
  while (true) {
    auto const pos = path.find_first_of('/');
    if (pos == boost::string_ref::npos) {
      if (!path.empty()) {
        parts.push_back(path);
      }
      return parts;
    }
    auto const len = pos + 1;  // include the /
    parts.emplace_back(path.substr(0, len));
    path.remove_prefix(len);
  }
}

CharCount path_distance_between(std::vector<boost::string_ref> const& x,
                                std::vector<boost::string_ref> const& y) {
  auto const end = std::min(x.size(), y.size());
  CharCount common_ancestors;
  for (common_ancestors = 0; common_ancestors < end; common_ancestors++) {
    if (x[common_ancestors] != y[common_ancestors]) {
      break;
    }
  }
  return x.size() + y.size() - (2 * common_ancestors);
}

}  // namespace cpsm
