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

struct MatchBase {
  // - By default, prefix_score is the maximum possible value.
  //
  // - If all alphanumeric characters in the rightmost path component of the
  // query matched in the rightmost path component of the item, prefix_score is
  // instead the maximum possible value - 1.
  //
  // - If, in addition, the first character in the rightmost path component of
  // the query matched the first character in the rightmost path component of
  // the item, prefix_score is instead the maximum possible value - 2.
  //
  // - If, in addition, all alphanumeric characters in the rightmost path
  // component of the query matched in a prefix of a word in the rightmost path
  // component of the item, prefix_score is the sum of the 1-indexed indices of
  // matched words.
  //
  // - If, in addition, all characters in the rightmost path component of the
  // query matched at the beginning of the rightmost path component of the item,
  // prefix_score is 0.
  CharCount2 prefix_score = std::numeric_limits<CharCount2>::max();

  // The number of matched characters at the beginning of the "words" in
  // rightmost path component of the item.
  CharCount word_prefix_len = 0;

  // Sum of path component indexes (counting from the right) for all item path
  // components containing at least one match, a lower value of which should
  // indicate higher confidence that the matches are correct.
  CharCount part_index_sum = 0;

  // The number of bytes that are shared between the beginning of the rightmost
  // path component of the match and the rightmost path component of the
  // current file.
  CharCount cur_file_prefix_len = 0;

  // The number of path components that must be traversed between the query and
  // item paths.
  CharCount path_distance = 0;

  // The number of consecutive unmatched characters at the end of the rightmost
  // path component of the item. Since it's easier to add characters at the end
  // of a query (vs. in the middle) to refine a match, lower values are weakly
  // preferred.
  CharCount unmatched_len = 0;

  std::string debug_string() const {
    return str_cat("prefix_score=", prefix_score, ", word_prefix_len=",
                   word_prefix_len, ", part_index_sum=", part_index_sum,
                   ", cur_file_prefix_len=", cur_file_prefix_len,
                   ", path_distance=", path_distance, ", unmatched_len=",
                   unmatched_len);
  }
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
  if (lhs.prefix_score != rhs.prefix_score) {
    return lhs.prefix_score < rhs.prefix_score;
  }
  if (lhs.word_prefix_len != rhs.word_prefix_len) {
    return lhs.word_prefix_len > rhs.word_prefix_len;
  }
  if (lhs.part_index_sum != rhs.part_index_sum) {
    return lhs.part_index_sum < rhs.part_index_sum;
  }
  if (lhs.cur_file_prefix_len != rhs.cur_file_prefix_len) {
    return lhs.cur_file_prefix_len > rhs.cur_file_prefix_len;
  }
  if (lhs.path_distance != rhs.path_distance) {
    return lhs.path_distance < rhs.path_distance;
  }
  if (lhs.unmatched_len != rhs.unmatched_len) {
    return lhs.unmatched_len < rhs.unmatched_len;
  }
  // Sort lexicographically on the item if all else fails.
  return lhs.item < rhs.item;
}

template <typename T>
void swap(Match<T>& x, Match<T>& y) {
  using std::swap;
  swap(x.prefix_score, y.prefix_score);
  swap(x.word_prefix_len, y.word_prefix_len);
  swap(x.part_index_sum, y.part_index_sum);
  swap(x.cur_file_prefix_len, y.cur_file_prefix_len);
  swap(x.path_distance, y.path_distance);
  swap(x.unmatched_len, y.unmatched_len);
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
