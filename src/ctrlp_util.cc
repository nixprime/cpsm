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

#include "ctrlp_util.h"

#include <stdexcept>

#include "path_util.h"
#include "str_util.h"

namespace cpsm {

std::function<boost::string_ref(boost::string_ref)> match_mode_item_substr_fn(
    boost::string_ref mmode) {
  if (mmode.empty() || mmode == "full-line") {
    return nullptr;
  } else if (mmode == "filename-only") {
    return path_basename;
  } else if (mmode == "first-non-tab") {
    return [](boost::string_ref const x)
        -> boost::string_ref { return x.substr(0, x.find_first_of('\t')); };
  } else if (mmode == "until-last-tab") {
    return [](boost::string_ref const x) -> boost::string_ref {
      auto const pos = x.find_last_of('\t');
      if (pos == boost::string_ref::npos) {
        return x;
      }
      return x.substr(pos + 1);
    };
  }
  throw Error("unknown match mode ", mmode);
}

}  // namespace cpsm
