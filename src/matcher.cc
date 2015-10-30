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

Matcher::Matcher(boost::string_ref const query, MatcherOpts opts,
                 StringHandler strings)
    : opts_(std::move(opts)), strings_(std::move(strings)) {
  strings_.decode(query, query_);
  if (opts_.is_path) {
    // Store the index of the first character after the rightmost path
    // separator in the query. (Store an index rather than an iterator to keep
    // Matcher copyable/moveable.)
    query_key_begin_index_ =
        std::find(query_.crbegin(), query_.crend(), path_separator()).base() -
        query_.cbegin();
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
      std::any_of(query_.begin(), query_.end(),
                  [&](char32_t const c) { return strings_.is_uppercase(c); });

  cur_file_parts_ = path_components_of(opts_.cur_file);
  if (!cur_file_parts_.empty()) {
    cur_file_key_ = cur_file_parts_.back();
    // Strip the extension from cur_file_key_, if any (but not the trailing .)
    auto const ext_sep_pos = cur_file_key_.find_last_of('.');
    if (ext_sep_pos != boost::string_ref::npos) {
      cur_file_key_ = cur_file_key_.substr(0, ext_sep_pos + 1);
    }
  }
}

bool Matcher::match(boost::string_ref const item, MatchBase& m,
                    std::set<CharCount>* match_positions,
                    std::vector<char32_t>* const buf,
                    std::vector<char32_t>* const buf2) const {
  Scorer scorer;

  std::vector<char32_t> key_chars_local;
  std::vector<char32_t>& key_chars = buf ? *buf : key_chars_local;
  std::vector<char32_t> temp_chars_local;
  std::vector<char32_t>& temp_chars = buf2 ? *buf2 : temp_chars_local;
  std::vector<CharCount> key_char_positions;
  std::vector<CharCount> temp_char_positions;

  std::vector<boost::string_ref> item_parts;
  if (opts_.is_path) {
    item_parts = path_components_of(item);
  } else {
    item_parts.push_back(item);
  }
  if (!item_parts.empty()) {
    scorer.unmatched_len = item_parts.back().size();
  }

  if (query_.empty()) {
    match_path(item_parts, scorer);
    m.reverse_score = scorer.reverse_score();
    return true;
  }

  // Since for paths (the common case) we prefer rightmost path components, we
  // scan path components right-to-left.
  auto query_it = query_.crbegin();
  auto const query_end = query_.crend();
  auto query_key_begin = query_.cend();
  // Index into item_parts, counting from the right.
  CharCount item_part_index = 0;
  // Offset of the beginning of the current item part from the beginning of the
  // item, in bytes.
  CharCount item_part_first_byte = item.size();
  std::set<CharCount> match_part_positions;
  for (boost::string_ref const item_part :
       boost::adaptors::reverse(item_parts)) {
    if (query_it == query_end) {
      break;
    }

    std::vector<char32_t>& item_part_chars =
        (item_part_index == 0) ? key_chars : temp_chars;
    item_part_chars.clear();
    std::vector<CharCount>* item_part_char_positions = nullptr;
    if (match_positions) {
      item_part_first_byte -= item_part.size();
      match_part_positions.clear();
      item_part_char_positions =
          (item_part_index == 0) ? &key_char_positions : &temp_char_positions;
      item_part_char_positions->clear();
    }
    strings_.decode(item_part, item_part_chars, item_part_char_positions);

    // Since path components are matched right-to-left, query characters must be
    // consumed greedily right-to-left.
    auto query_prev = query_it;
    for (auto it = item_part_chars.crbegin(), end = item_part_chars.crend();
         it != end; ++it) {
      if (match_char(*it, *query_it)) {
        // Don't store match positions for the key yet, since match_key will
        // refine them.
        if (match_positions && item_part_index != 0) {
          std::size_t const i =
              item_part_chars.size() - (it - item_part_chars.crbegin());
          CharCount begin = (*item_part_char_positions)[i - 1];
          CharCount end;
          if (i == item_part_chars.size()) {
            end = item_part.size();
          } else {
            end = (*item_part_char_positions)[i];
          }
          for (; begin < end; begin++) {
            match_part_positions.insert(item_part_first_byte + begin);
          }
        }
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
      if (match_positions) {
        match_part_positions.clear();
      }
    }

    // Ok, done matching this part.
    if (query_it != query_prev) {
      scorer.parts++;
    }
    if (item_part_index == 0) {
      query_key_begin = query_it.base();
    }
    item_part_index++;
    if (match_positions) {
      for (auto const pos : match_part_positions) {
        match_positions->insert(pos);
      }
    }
  }

  // Did all characters match?
  if (query_it != query_end) {
    return false;
  }

  // Fill path match data.
  match_path(item_parts, scorer);

  // Now do more refined matching on the key (the rightmost path component of
  // the item for a path match, and just the full item otherwise).
  if (match_positions) {
    // Adjust key_char_positions to be relative to the beginning of the string
    // rather than the beginning of the key. item_parts can't be empty because
    // query is non-empty and matching was successful.
    CharCount const item_key_first_byte =
        item.size() - item_parts.back().size();
    for (auto& pos : key_char_positions) {
      pos += item_key_first_byte;
    }
    // Push item.size() to simplify key_char_positions indexing in match_key
    // and save an extra parameter.
    key_char_positions.push_back(item.size());
  }
  match_key(key_chars, query_key_begin, scorer, match_positions,
            key_char_positions);
  m.reverse_score = scorer.reverse_score();
  return true;
}

void Matcher::match_path(std::vector<boost::string_ref> const& item_parts,
                         Scorer& scorer) const {
  if (!opts_.is_path) {
    return;
  }
  scorer.path_distance = path_distance_between(cur_file_parts_, item_parts);
  // We don't want to exclude cur_file as a match, but we also don't want it
  // to be the top match, so force cur_file_prefix_len to 0 for cur_file (i.e.
  // if path_distance is 0).
  if (scorer.path_distance != 0 && !item_parts.empty()) {
    scorer.cur_file_prefix_len =
        common_prefix(cur_file_key_, item_parts.back());
  }
}

void Matcher::match_key(
    std::vector<char32_t> const& key,
    std::vector<char32_t>::const_iterator const query_key, Scorer& scorer,
    std::set<CharCount>* const match_positions,
    std::vector<CharCount> const& key_char_positions) const {
  auto const query_key_end = query_.cend();
  if (query_key == query_key_end) {
    return;
  }
  // key can't be empty since [query_key, query_.end()) is non-empty.
  const auto is_word_prefix = [&](std::size_t const i) -> bool {
    if (i == 0) {
      return true;
    }
    if (strings_.is_alphanumeric(key[i]) &&
        !strings_.is_alphanumeric(key[i - 1])) {
      return true;
    }
    if (strings_.is_uppercase(key[i]) && !strings_.is_uppercase(key[i - 1])) {
      return true;
    }
    return false;
  };

  // Attempt two matches. In the first pass, only match word prefixes and
  // non-alphanumeric characters to try and get a word prefix-only match. In
  // the second pass, match greedily.
  for (int pass = 0; pass < 2; pass++) {
    auto query_it = query_key;
    CharCount word_index = 0;
    CharCount2 word_index_sum = 0;
    CharCount word_prefix_len = 0;
    bool start_matched = false;
    bool at_word_start = true;
    bool word_matched = false;
    std::set<CharCount> match_positions_pass;
    for (std::size_t i = 0; i < key.size(); i++) {
      if (is_word_prefix(i)) {
        word_index++;
        at_word_start = true;
        word_matched = false;
      }
      if (pass == 0 && strings_.is_alphanumeric(*query_it) && !at_word_start) {
        continue;
      }
      if (match_char(key[i], *query_it)) {
        if (at_word_start) {
          word_prefix_len++;
        }
        if (!word_matched) {
          word_index_sum += word_index;
          word_matched = true;
        }
        if (i == 0) {
          start_matched = true;
        }
        if (match_positions) {
          CharCount begin = key_char_positions[i];
          CharCount const end = key_char_positions[i+1];
          for (; begin < end; begin++) {
            match_positions_pass.insert(begin);
          }
        }
        ++query_it;
        if (query_it == query_key_end) {
          auto const query_key_begin = query_.cbegin() + query_key_begin_index_;
          if (query_key != query_key_begin) {
            scorer.prefix_score = std::numeric_limits<CharCount2>::max();
            if (start_matched) {
              scorer.prefix_score = std::numeric_limits<CharCount2>::max() - 1;
            }
          } else if ((i + 1) == std::size_t(query_key_end - query_key_begin)) {
            scorer.prefix_score = 0;
          } else if (pass == 0) {
            scorer.prefix_score = word_index_sum;
          } else if (start_matched) {
            scorer.prefix_score = std::numeric_limits<CharCount2>::max() - 3;
          } else {
            scorer.prefix_score = std::numeric_limits<CharCount2>::max() - 2;
          }
          scorer.word_prefix_len = word_prefix_len;
          scorer.unmatched_len = key.size() - (i + 1);
          if (match_positions) {
            for (auto const pos : match_positions_pass) {
              match_positions->insert(pos);
            }
          }
          return;
        }
      } else {
        at_word_start = false;
      }
    }
  }
}

bool Matcher::match_char(char32_t item, char32_t const query) const {
  if (!is_case_sensitive_) {
    // The query must not contain any uppercase letters since otherwise the
    // query would be case-sensitive, so just force all uppercase characters to
    // lowercase.
    if (strings_.is_uppercase(item)) {
      item = strings_.to_lowercase(item);
    }
  }
  return item == query;
}

}  // namespace cpsm
