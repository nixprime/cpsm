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

#ifndef CPSM_MATCHER_H_
#define CPSM_MATCHER_H_

#include <functional>
#include <string>
#include <vector>

#include <boost/utility/string_ref.hpp>

#include "match.h"

namespace cpsm {

struct MatcherOpts {
  // If true, the query and all items are paths.
  bool is_path = true;

  // The currently open file.
  std::string cur_file;

  enum class QueryPathMode {
    // Match path separators like any other character in the query.
    NORMAL,
    // All query characters between two path separators must match within a
    // single path component.
    STRICT,
    // Allow query characters to span path components if no path separators
    // appear in the query; otherwise treat them strictly.
    AUTO,
  };
  QueryPathMode query_path_mode = QueryPathMode::AUTO;

  // If not null, this function is applied to each item to form the actual
  // string that is matched on.
  std::function<boost::string_ref(boost::string_ref)> item_substr_fn;
};

class Matcher {
 public:
  explicit Matcher(std::string query, MatcherOpts opts = MatcherOpts());

  // Attempts to match the query represented by this matcher against the given
  // item. If successful, match adds a Match object representing the item to
  // the given vector and returns true. Otherwise match returns false.
  //
  // (This method is not const - or thread-safe - since it uses a
  // Matcher-private buffer.)
  bool append_match(boost::string_ref item, std::vector<Match>& matches);

 private:
  std::string query_;
  MatcherOpts opts_;
  bool is_case_sensitive_;
  bool require_full_part_;
  std::vector<std::vector<char32_t>> query_parts_chars_;
  std::vector<char32_t> key_part_chars_;
  std::vector<boost::string_ref> cur_file_parts_;
};

}  // namespace cpsm

#endif /* CPSM_MATCHER_H_ */
