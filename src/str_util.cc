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

#include "str_util.h"

namespace cpsm {

std::vector<boost::string_ref> str_split(boost::string_ref str,
                                         char const delimiter) {
  std::vector<boost::string_ref> splits;
  while (true) {
    auto const dpos = str.find_first_of(delimiter);
    if (dpos == boost::string_ref::npos) {
      break;
    }
    splits.push_back(str.substr(0, dpos));
    str.remove_prefix(dpos+1);
  }
  splits.push_back(str);
  return splits;
}

}  // namespace cpsm
