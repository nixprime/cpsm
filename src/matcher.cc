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

Matcher::Matcher(std::string query, MatcherOpts opts)
    : query_(std::move(query)), opts_(std::move(opts)) {
  if (opts_.is_path) {
    switch (opts_.query_path_mode) {
      case MatcherOpts::QueryPathMode::NORMAL:
        require_full_part_ = false;
        break;
      case MatcherOpts::QueryPathMode::STRICT:
        require_full_part_ = true;
        break;
      case MatcherOpts::QueryPathMode::AUTO:
        require_full_part_ = (query_.find_first_of('/') != std::string::npos);
        break;
    }
    for (boost::string_ref const query_part : path_components_of(query_)) {
      std::vector<char32_t> query_part_chars;
      decompose_utf8_string(query_part, query_part_chars);
      query_parts_chars_.emplace_back(std::move(query_part_chars));
    }
  } else {
    require_full_part_ = false;
    std::vector<char32_t> query_chars;
    decompose_utf8_string(query_, query_chars);
    query_parts_chars_.emplace_back(std::move(query_chars));
  }
  // Queries are smartcased (case-sensitive only if any uppercase appears in the
  // query).
  is_case_sensitive_ = std::any_of(query_.begin(), query_.end(), is_uppercase);
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
                         std::vector<char32_t>* item_part_chars) const {
  m = MatchBase();

  std::vector<char32_t> item_part_chars_local;
  if (!item_part_chars) {
    item_part_chars = &item_part_chars_local;
  }

  std::vector<boost::string_ref> item_parts;
  if (opts_.is_path) {
    item_parts = path_components_of(item);
    m.path_distance = path_distance_between(cur_file_parts_, item_parts);
  } else {
    item_parts.push_back(item);
  }

  if (query_parts_chars_.empty()) {
    return true;
  }

  // Since for paths (the common case) we prefer rightmost path components, we
  // scan path components right-to-left.
  auto query_part_chars_it = query_parts_chars_.rbegin();
  auto const query_part_chars_end = query_parts_chars_.rend();
  // Index of the last unmatched character in query_part.
  std::ptrdiff_t end = std::ptrdiff_t(query_part_chars_it->size()) - 1;
  // Index into item_parts, counting from the right.
  CharCount part_index = 0;
  for (boost::string_ref const item_part :
       boost::adaptors::reverse(item_parts)) {
    auto const& query_part_chars = *query_part_chars_it;
    item_part_chars->clear();
    decompose_utf8_string(item_part, *item_part_chars);
    if (part_index == 0) {
      m.unmatched_len = item_part_chars->size();
    }

    // Since path components are matched right-to-left, query characters must be
    // consumed greedily right-to-left.
    std::ptrdiff_t start = end;  // index of last unmatched query char
    if (start >= 0) {
      for (char32_t const c : boost::adaptors::reverse(*item_part_chars)) {
        if (match_char(c, query_part_chars[start])) {
          start--;
          if (start < 0) {
            break;
          }
        }
      }
    }
    if (require_full_part_ && start >= 0) {
      // Didn't consume all characters, but strict query path mode is on, so the
      // consumed characters don't count.
      part_index++;
      continue;
    }
    std::ptrdiff_t const next_end = start;

    // Since within a path component we usually prefer leftmost character
    // matches, we pick the leftmost match for each consumed character.
    start++;  // now index of first matched query char
    if (start <= end) {
      const auto is_word_prefix = [&](std::size_t const i) -> bool {
        if (i == 0) {
          return true;
        }
        if (is_alphanumeric((*item_part_chars)[i]) &&
            !is_alphanumeric((*item_part_chars)[i - 1])) {
          return true;
        }
        if (is_uppercase((*item_part_chars)[i]) &&
            !is_uppercase((*item_part_chars)[i - 1])) {
          return true;
        }
        return false;
      };
      bool at_word_start = false;
      for (std::size_t i = 0; i < item_part_chars->size(); i++) {
        if (match_char((*item_part_chars)[i], query_part_chars[start])) {
          if (part_index == 0) {
            if (is_word_prefix(i)) {
              at_word_start = true;
            }
            if (at_word_start) {
              m.word_prefix_len++;
            }
            if (i == 0 && start == 0) {
              m.prefix_match = MatchBase::PrefixMatch::PARTIAL;
            }
          }
          start++;
          if (start > end) {
            if (part_index == 0) {
              m.unmatched_len = item_part_chars->size() - (i + 1);
              if (i == std::size_t(end) && next_end < 0) {
                m.prefix_match = MatchBase::PrefixMatch::FULL;
              }
            }
            break;
          }
        } else {
          at_word_start = false;
        }
      }
    }

    // Did we match anything in this item part?
    if (end != next_end) {
      end = next_end;
      m.part_index_sum += part_index;
    }
    if (end < 0) {
      query_part_chars_it++;
      if (query_part_chars_it == query_part_chars_end) {
        return true;
      }
      end = std::ptrdiff_t(query_part_chars_it->size()) - 1;
    }
    part_index++;
  }

  return false;
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
