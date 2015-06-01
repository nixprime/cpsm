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
#include <limits>
#include <utility>

#include <boost/range/adaptor/reversed.hpp>

#include "path_util.h"
#include "str_util.h"

namespace cpsm {

Matcher::Matcher(boost::string_ref const query, MatcherOpts opts)
    : opts_(std::move(opts)) {
  decompose_utf8_string(query, query_chars_);
  if (opts_.is_path) {
    // Store the index of the first character after the rightmost path
    // separator in the query. (Store an index rather than an iterator to keep
    // Matcher copyable/moveable.)
    query_key_begin_index_ =
        std::find(query_chars_.crbegin(), query_chars_.crend(),
                  path_separator()).base() -
        query_chars_.cbegin();
    switch (opts_.query_path_mode) {
      case MatcherOpts::QueryPathMode::NORMAL:
        require_full_part_ = false;
        break;
      case MatcherOpts::QueryPathMode::STRICT:
        require_full_part_ = true;
        break;
      case MatcherOpts::QueryPathMode::AUTO:
        require_full_part_ =
            (query.find_first_of(path_separator()) != std::string::npos);
        break;
    }
  } else {
    query_key_begin_index_ = 0;
    require_full_part_ = false;
  }

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
    m.unmatched_len = key.size();
    return;
  }
  bool const query_key_at_begin =
      (query_key == (query_chars_.cbegin() + query_key_begin_index_));
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

  // Attempt two matches. In the first pass, only match word prefixes and
  // non-alphanumeric characters to try and get a word prefix-only match. In
  // the second pass, match greedily.
  for (int pass = 0; pass < 2; pass++) {
    CharCount word_index = 0;
    bool at_word_start = true;
    bool word_matched = false;
    bool is_full_prefix = query_key_at_begin;
    m.prefix_score = std::numeric_limits<CharCount2>::max();
    switch (pass) {
      case 0:
        if (query_key_at_begin) {
          m.prefix_score = 0;
        }
        break;
      case 1:
        if (query_key_at_begin) {
          m.prefix_score = std::numeric_limits<CharCount2>::max() - 1;
        }
        // Need to reset word_prefix_len after failed first pass.
        m.word_prefix_len = 0;
        break;
    }
    for (std::size_t i = 0; i < key.size(); i++) {
      if (is_word_prefix(i)) {
        word_index++;
        at_word_start = true;
        word_matched = false;
      }
      if (pass == 0 && is_alphanumeric(*query_key) && !at_word_start) {
        is_full_prefix = false;
        continue;
      }
      if (match_char(key[i], *query_key)) {
        if (at_word_start) {
          m.word_prefix_len++;
        }
        if (pass == 0 && query_key_at_begin && !word_matched) {
          m.prefix_score += word_index;
          word_matched = true;
        }
        if (pass == 1 && query_key_at_begin && i == 0) {
          m.prefix_score = std::numeric_limits<CharCount2>::max() - 2;
        }
        ++query_key;
        if (query_key == query_key_end) {
          m.unmatched_len = key.size() - (i + 1);
          if (is_full_prefix) {
            m.prefix_score = 0;
          }
          return;
        }
      } else {
        at_word_start = false;
        is_full_prefix = false;
      }
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
