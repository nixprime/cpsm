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

#include "matcher.h"

#include <algorithm>
#include <cstdint>
#include <utility>

#include <boost/range/adaptor/reversed.hpp>

#include "path_util.h"
#include "str_util.h"

namespace cpsm {

Matcher::Matcher(boost::string_ref const query, MatcherOpts opts)
    : opts_(std::move(opts)) {
  if (opts_.is_path) {
    switch (opts_.query_path_mode) {
      case MatcherOpts::QueryPathMode::NORMAL:
        require_full_part_ = false;
        break;
      case MatcherOpts::QueryPathMode::STRICT:
        require_full_part_ = true;
        break;
      case MatcherOpts::QueryPathMode::AUTO:
        require_full_part_ = (query.find_first_of(path_separator()) != std::string::npos);
        break;
    }
  } else {
    require_full_part_ = false;
  }
  decompose_utf8_string(query, query_chars_);
  // Queries are smartcased (case-sensitive only if any uppercase appears in the
  // query).
  is_case_sensitive_ =
      std::any_of(query_chars_.begin(), query_chars_.end(), is_uppercase);
  cur_file_parts_ = path_components_of(opts_.cur_file);
  // Keeping the filename in cur_file_parts_ causes the path distance metric to
  // favor the currently open file. While we don't want to exclude the
  // currently open file from being matched, it shouldn't be favored over its
  // siblings on path distance.
  if (!cur_file_parts_.empty()) {
    cur_file_parts_.pop_back();
  }
}

bool Matcher::match_base(boost::string_ref const item, MatchBase& m,
                         std::vector<char32_t>* key_chars,
                         std::vector<char32_t>* temp_chars) const {
  m = MatchBase();

  std::vector<char32_t> key_chars_local;
  if (!key_chars) {
    key_chars = &key_chars_local;
  }
  std::vector<char32_t> temp_chars_local;
  if (!temp_chars) {
    temp_chars = &temp_chars_local;
  }

  std::vector<boost::string_ref> item_parts;
  if (opts_.is_path) {
    item_parts = path_components_of(item);
    m.path_distance = path_distance_between(cur_file_parts_, item_parts);
  } else {
    item_parts.push_back(item);
  }

  if (query_chars_.empty()) {
    return true;
  }

  // Since for paths (the common case) we prefer rightmost path components, we
  // scan path components right-to-left.
  auto query_it = query_chars_.crbegin();
  auto const query_end = query_chars_.crend();
  auto query_key_begin = query_chars_.cend();
  // Index into item_parts, counting from the right.
  CharCount part_index = 0;
  for (boost::string_ref const item_part :
       boost::adaptors::reverse(item_parts)) {
    if (query_it == query_end) {
      break;
    }

    std::vector<char32_t>* const item_part_chars =
        part_index ? temp_chars : key_chars;
    item_part_chars->clear();
    decompose_utf8_string(item_part, *item_part_chars);

    // Since path components are matched right-to-left, query characters must be
    // consumed greedily right-to-left.
    auto query_prev = query_it;
    for (char32_t const c : boost::adaptors::reverse(*item_part_chars)) {
      if (match_char(c, *query_it)) {
        ++query_it;
        if (query_it == query_end) {
          break;
        }
      }
    }

    // If strict query path mode is on, the match must have run to a path
    // separator. If not, discard the match.
    if (require_full_part_ &&
        !((query_it == query_end) || (*query_it == path_separator()))) {
      query_it = query_prev;
      continue;
    }

    m.part_index_sum += part_index;
    if (part_index == 0) {
      query_key_begin = query_it.base();
    }
    part_index++;
  }

  // Did all characters match?
  if (query_it != query_end) {
    return false;
  }

  // Now do more refined matching on the key (the rightmost path component of
  // the item for a path match, and just the full item otherwise).
  match_key(*key_chars, query_key_begin, m);
  return true;
}

void Matcher::match_key(std::vector<char32_t> const& key,
                        std::vector<char32_t>::const_iterator query_key,
                        MatchBase& m) const {
  auto const query_key_end = query_chars_.cend();
  if (query_key == query_key_end) {
    return;
  }
  // Since within a path component we usually prefer leftmost character
  // matches, we pick the leftmost match for each consumed character.
  // key can't be empty since [query_key, query_chars_.end()) is non-empty.
  const auto is_word_prefix = [&](std::size_t const i) -> bool {
    if (i == 0) {
      return true;
    }
    if (is_alphanumeric(key[i]) && !is_alphanumeric(key[i - 1])) {
      return true;
    }
    if (is_uppercase(key[i]) && !is_uppercase(key[i - 1])) {
      return true;
    }
    return false;
  };
  bool at_word_start = false;
  bool is_full_prefix = (query_key == query_chars_.cbegin());
  for (std::size_t i = 0; i < key.size(); i++) {
    if (match_char(key[i], *query_key)) {
      if (is_word_prefix(i)) {
        at_word_start = true;
      }
      if (at_word_start) {
        m.word_prefix_len++;
      }
      if (i == 0) {
        m.prefix_match = MatchBase::PrefixMatch::PARTIAL;
      }
      ++query_key;
      if (query_key == query_key_end) {
        m.unmatched_len = key.size() - (i + 1);
        if (is_full_prefix) {
          m.prefix_match = MatchBase::PrefixMatch::FULL;
        }
        return;
      }
    } else {
      at_word_start = false;
      is_full_prefix = false;
    }
  }
}

bool Matcher::match_char(char32_t item, char32_t const query) const {
  if (!is_case_sensitive_) {
    // The query must not contain any uppercase letters since otherwise the
    // query would be case-sensitive, so just force all uppercase characters to
    // lowercase.
    if (is_uppercase(item)) {
      item = to_lowercase(item);
    }
  }
  return item == query;
}

}  // namespace cpsm
