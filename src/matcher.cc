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

namespace {

// Splits a UTF-8-encoded string into code points and append them to the given
// vector. If the string is not a valid UTF-8 encoded string, invalid bytes are
// are replaced by the invalid code point 0xdc00+(byte). (This is so that a
// match can still be attempted.)
void decompose_utf8_string(boost::string_ref str,
                           std::vector<char32_t>& chars) {
  // Even though most of this function deals with byte-sized quantities, use
  // char32_t throughout to avoid casting.
  auto const lookahead = [&](size_t n) -> char32_t {
    if (n >= str.size()) {
      return 0;
    }
    return str[n];
  };
  auto const invalid = [&](char32_t b) {
    chars.push_back(0xdc00 + b);
    str.remove_prefix(1);
  };
  auto const is_continuation = [](char32_t b) -> bool {
    return (b & 0xc0) == 0x80;
  };
  while (!str.empty()) {
    auto const b0 = lookahead(0);
    if (b0 == 0x00) {
      // Input is a string_ref, not a null-terminated string - premature null?
      invalid(b0);
    } else if (b0 < 0x80) {
      // 1-byte character
      chars.push_back(b0);
      str.remove_prefix(1);
    } else if (b0 < 0xc2) {
      // Continuation or overlong encoding
      invalid(b0);
    } else if (b0 < 0xe0) {
      // 2-byte sequence
      auto const b1 = lookahead(1);
      if (!is_continuation(b1)) {
        invalid(b0);
      } else {
        chars.push_back(((b0 & 0x1f) << 6) | (b1 & 0x3f));
        str.remove_prefix(2);
      }
    } else if (b0 < 0xf0) {
      // 3-byte sequence
      auto const b1 = lookahead(1), b2 = lookahead(2);
      if (!is_continuation(b1) || !is_continuation(b2)) {
        invalid(b0);
      } else if (b0 == 0xe0 && b1 < 0xa0) {
        // Overlong encoding
        invalid(b0);
      } else {
        chars.push_back(((b0 & 0x0f) << 12) | ((b1 & 0x3f) << 6) | (b2 & 0x3f));
        str.remove_prefix(3);
      }
    } else if (b0 < 0xf5) {
      // 4-byte sequence
      auto const b1 = lookahead(1), b2 = lookahead(2), b3 = lookahead(3);
      if (!is_continuation(b1) || !is_continuation(b2) ||
          !is_continuation(b3)) {
        invalid(b0);
      } else if (b0 == 0xf0 && b1 < 0x90) {
        // Overlong encoding
        invalid(b0);
      } else if (b0 == 0xf4 && b1 >= 0x90) {
        // > U+10FFFF
        invalid(b0);
      } else {
        chars.push_back(((b0 & 0x07) << 18) | ((b1 & 0x3f) << 12) |
                        ((b2 & 0x3f) << 6) | (b3 & 0x3f));
        str.remove_prefix(4);
      }
    } else {
      // > U+10FFFF
      invalid(b0);
    }
  }
}

} // anonymous namespace

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
  // query). Casing only applies to ASCII letters.
  is_case_sensitive_ =
      std::any_of(query_.begin(), query_.end(),
                  [](char32_t c) -> bool { return c >= 'A' && c <= 'Z'; });
  cur_file_parts_= path_components_of(opts_.cur_file);
  // Keeping the filename in cur_file_parts_ causes the path distance metric to
  // favor the currently open file. While we don't want to exclude the
  // currently open file from being matched, it shouldn't be favored over its
  // siblings on path distance.
  if (!cur_file_parts_.empty()) {
    cur_file_parts_.pop_back();
  }
}

bool Matcher::append_match(boost::string_ref const item,
                           std::vector<Match>& matches) {
  if (query_parts_chars_.empty()) {
    matches.emplace_back(copy_string_ref(item));
    return true;
  }

  boost::string_ref key = item;
  if (opts_.item_substr_fn) {
    key = opts_.item_substr_fn(item);
  }
  std::vector<boost::string_ref> key_parts;
  int path_distance;
  if (opts_.is_path) {
    key_parts = path_components_of(key);
    path_distance = path_distance_between(cur_file_parts_, key_parts);
  } else {
    key_parts.push_back(key);
    path_distance = 0;
  }

  int part_idx = 0;  // index of key_part from right side
  int part_sum = 0;  // sum of all key_part_idx with any match
  int prefix_len = 0;  // length of match at start of rightmost key_part
  bool prefix_match = false;  // query and rightmost key_part match first char
  int unmatched_len = 0;  // unmatched rightmost chars in rightmost key_part
  // Since for paths (the common case) we prefer rightmost path components, we
  // scan path components right-to-left.
  auto query_part_chars_it = query_parts_chars_.rbegin();
  auto const query_part_chars_end = query_parts_chars_.rend();
  int end = int(query_part_chars_it->size()) - 1;  // last unmatched query char
  for (boost::string_ref const key_part : boost::adaptors::reverse(key_parts)) {
    auto const& query_part_chars = *query_part_chars_it;
    key_part_chars_.clear();
    decompose_utf8_string(key_part, key_part_chars_);
    if (!is_case_sensitive_) {
      // The query must not contain any uppercase ASCII letters since otherwise
      // the query would be case-sensitive.
      for (char32_t& c : key_part_chars_) {
        if (c >= 'A' && c <= 'Z') {
          c += ('a' - 'A');
        }
      }
    }
    if (part_idx == 0) {
      unmatched_len = key_part_chars_.size();
    }

    // Since path components are matched right-to-left, query characters must be
    // consumed greedily right-to-left.
    int start = end;  // index of last unmatched query char
    if (start >= 0) {
      for (char32_t const c : boost::adaptors::reverse(key_part_chars_)) {
        if (c == query_part_chars[start]) {
          start--;
          if (start < 0) {
            break;
          }
        }
      }
    }
    if (require_full_part_ && start >= 0) {
      // Didn't consume all characters, but strict query path mode is on.
      part_idx++;
      continue;
    }
    int const next_end = start;

    // Since within a path component we prefer leftmost character matches, we
    // pick the leftmost match for each consumed character.
    start++;  // now index of first matched query char
    if (start <= end) {
      for (int i = 0; i < key_part_chars_.size(); i++) {
        if (key_part_chars_[i] == query_part_chars[start]) {
          if (part_idx == 0) {
            if (i == prefix_len) {
              prefix_len++;
            }
            if (i == 0 && start == 0) {
              prefix_match = true;
            }
          }
          start++;
          if (start > end) {
            if (part_idx == 0) {
              unmatched_len -= i + 1;
            }
            break;
          }
        }
      }
    }

    // Did we match anything in this key part?
    if (end != next_end) {
      end = next_end;
      part_sum += part_idx;
    }
    if (end < 0) {
      query_part_chars_it++;
      if (query_part_chars_it == query_part_chars_end) {
        matches.emplace_back(copy_string_ref(item), part_sum, path_distance,
                             prefix_len, prefix_match, unmatched_len);
        return true;
      }
      end = int(query_part_chars_it->size()) - 1;
    }
    part_idx++;
  }

  return false;
}

} // namespace cpsm
