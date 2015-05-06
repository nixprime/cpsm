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

#ifndef CPSM_MATCH_H_
#define CPSM_MATCH_H_

#include <climits>
#include <string>
#include <utility>

namespace cpsm {

class Match {
 public:
  // Default constructor provided so that std::vector<Match>::resize() can
  // exist.
  Match()
      : part_sum_(INT_MAX),
        path_distance_(INT_MAX),
        prefix_len_(INT_MIN),
        unmatched_len_(INT_MAX),
        word_prefixes_(INT_MIN),
        prefix_match_(false) {}

  // Trivial constructor for matches on empty query strings.
  explicit Match(std::string item)
      : item_(std::move(item)),
        part_sum_(INT_MAX),
        path_distance_(INT_MAX),
        prefix_len_(INT_MIN),
        unmatched_len_(INT_MAX),
        word_prefixes_(INT_MIN),
        prefix_match_(false) {}

  Match(std::string item, int part_sum, int path_distance, int prefix_len,
        bool prefix_match, int unmatched_len, int word_prefixes)
      : item_(std::move(item)),
        part_sum_(part_sum),
        path_distance_(path_distance),
        prefix_len_(prefix_len),
        unmatched_len_(unmatched_len),
        word_prefixes_(word_prefixes),
        prefix_match_(prefix_match) {}

  std::string const& item() const { return item_; }

  // Returns true if this is a *better* match than other. In all of the below,
  // "filename" means the last path component of the match substring for path
  // matches, or the entire match substring for non-path matches.
  bool operator<(Match const& other) const {
    // Prefer query prefix matches (cases where the first character of the query
    // matched the first character of the filename).
    if (prefix_match_ != other.prefix_match_) {
      return prefix_match_;
    }
    // Prefer longer filename prefix matches.
    if (prefix_len_ != other.prefix_len_) {
      return prefix_len_ > other.prefix_len_;
    }
    // Prefer more filename word prefixes (character matches at a character
    // preceded by punctuation, or a capital letter preceded by a non-capital
    // letter.)
    if (word_prefixes_ != other.word_prefixes_) {
      return word_prefixes_ > other.word_prefixes_;
    }
    // For path matches, prefer fewer matches in path components, and matches
    // further to the right, which together signal higher confidence that such
    // matches are actually correct.
    if (part_sum_ != other.part_sum_) {
      return part_sum_ < other.part_sum_;
    }
    // For path matches, prefer paths that are "closer" to the currently open
    // file.
    if (path_distance_ != other.path_distance_) {
      return path_distance_ < other.path_distance_;
    }
    // Prefer matches with fewer unmatched characters rightward of the rightmost
    // match. This is because it's easier to add characters at the end of a
    // query to refine a match.
    if (unmatched_len_ != other.unmatched_len_) {
      return unmatched_len_ < other.unmatched_len_;
    }
    // Sort lexicographically on the item if all else fails.
    return item_ < other.item_;
  }

 private:
  std::string item_;
  int part_sum_;
  int path_distance_;
  int prefix_len_;
  int unmatched_len_;
  int word_prefixes_;
  bool prefix_match_;
};

}  // namespace cpsm

#endif /* CPSM_MATCH_H_ */
