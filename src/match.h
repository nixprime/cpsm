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

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "str_util.h"

namespace cpsm {

struct Scorer {
  // - By default, prefix_score is the maximum possible value.
  //
  // - If any character in the query matched the first character in the
  // rightmost path component of the item, prefix_score is instead the maximum
  // possible value - 1.
  //
  // - If all characters in the rightmost path component of the query matched
  // in the rightmost path component of the item, prefix_score is instead the
  // maximum possible value - 2.
  //
  // - If, in addition, the first character in the rightmost path component of
  // the query matched the first character in the rightmost path component of
  // the item (i.e. both of the above conditions hold), prefix_score is instead
  // the maximum possible value - 3.
  //
  // - If, in addition, all alphanumeric characters in the rightmost path
  // component of the query matched in a prefix of a word in the rightmost path
  // component of the item, prefix_score is the sum of the 1-indexed indices of
  // matched words.
  //
  // - If, in addition, all characters in the rightmost path component of the
  // query matched at the beginning of the rightmost path component of the item,
  // prefix_score is 0.
  //
  // Lower is better.
  CharCount2 prefix_score = std::numeric_limits<CharCount2>::max();

  // The number of matched characters at the beginning of the "words" in
  // rightmost path component of the item. Higher is better.
  CharCount word_prefix_len = 0;

  // Number of path components containing at least one match. Lower is better.
  CharCount parts = 0;

  // The number of bytes that are shared between the beginning of the rightmost
  // path component of the match and the rightmost path component of the
  // current file. Higher is better.
  CharCount cur_file_prefix_len = 0;

  // The number of path components that must be traversed between the query and
  // item paths. Lower is better.
  CharCount path_distance = 0;

  // The number of consecutive unmatched characters at the end of the rightmost
  // path component of the item. Since it's easier to add characters at the end
  // of a query (vs. in the middle) to refine a match, lower values are weakly
  // preferred.
  CharCount unmatched_len = 0;

  // Returns a reverse score (lower is better) summarizing the current value of
  // this scorer.
  std::uint64_t reverse_score() const {
    return (
        (std::uint64_t(prefix_score) << 31) +
        (std::uint64_t(std::numeric_limits<CharCount>::max() - word_prefix_len)
         << 28) +
        (std::uint64_t(parts) << 20) +
        (std::uint64_t(std::numeric_limits<CharCount>::max() -
                       cur_file_prefix_len)
         << 14) +
        (std::uint64_t(path_distance) << 8) + std::uint64_t(unmatched_len));
  }

  std::string debug_string() const {
    return str_cat("prefix_score=", prefix_score, ", word_prefix_len=",
                   word_prefix_len, ", parts=", parts, ", cur_file_prefix_len=",
                   cur_file_prefix_len, ", path_distance=", path_distance,
                   ", unmatched_len=", unmatched_len);
  }
};

struct MatchBase {
  std::uint64_t reverse_score = std::numeric_limits<std::uint64_t>::max();
};

template <typename T>
struct Match : public MatchBase {
  T item;

  Match() = default;
  explicit Match(T item) : item(std::move(item)) {}
};

// Returns true if lhs is a *better* match than rhs (so that sorting in
// ascending order, as is default for std::sort, sorts in *descending* match
// quality).
template <typename T>
bool operator<(Match<T> const& lhs, Match<T> const& rhs) {
  if (lhs.reverse_score != rhs.reverse_score) {
    return lhs.reverse_score < rhs.reverse_score;
  }
  // Sort lexicographically on the item if all else fails.
  return lhs.item < rhs.item;
}

template <typename T>
void swap(Match<T>& x, Match<T>& y) {
  using std::swap;
  swap(x.reverse_score, y.reverse_score);
  swap(x.item, y.item);
}

template <typename T>
void sort_limit(std::vector<T>& vec,
                typename std::vector<T>::size_type const limit = 0) {
  if (limit && limit < vec.size()) {
    std::partial_sort(vec.begin(), vec.begin() + limit, vec.end());
    vec.resize(limit);
  } else {
    std::sort(vec.begin(), vec.end());
  }
}

}  // namespace cpsm

#endif /* CPSM_MATCH_H_ */
