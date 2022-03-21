// cpsm - fuzzy path matcher
// Copyright (C) 2015 the Authors
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

// CtrlP-specific support utilities.

#ifndef CPSM_CTRLP_UTIL_H_
#define CPSM_CTRLP_UTIL_H_

#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#if __has_include("string_view")
#include <string_view>
using std::string_view;
#else
#include <experimental/string_view>
using std::experimental::string_view;
#endif


#include "path_util.h"
#include "str_util.h"

namespace cpsm {

enum class CtrlPMatchMode {
  // Match the entire line.
  FULL_LINE,

  // Match only the filename.
  FILENAME_ONLY,

  // Match until the first tab char.
  FIRST_NON_TAB,

  // Match until the last tab char.
  UNTIL_LAST_TAB,
};

// Parses a CtrlP match mode.
CtrlPMatchMode parse_ctrlp_match_mode(string_view mmode);

// Functor types implementing transformations for each CtrlP match mode.

struct FullLineMatch {
  string_view operator()(string_view const item) const {
    return item;
  }
};

struct FilenameOnlyMatch {
  string_view operator()(string_view const item) const {
    return ref_str_iters(
        path_basename<PlatformPathTraits>(item.cbegin(), item.cend()),
        item.cend());
  }
};

struct FirstNonTabMatch {
  string_view operator()(string_view const item) const {
    return ref_str_iters(item.cbegin(),
                         std::find(item.cbegin(), item.cend(), '\t'));
  }
};

struct UntilLastTabMatch {
  string_view operator()(string_view const item) const {
    auto const item_rend = item.crend();
    auto const last_tab_rit = std::find(item.crbegin(), item_rend, '\t');
    return ref_str_iters(item.cbegin(), (last_tab_rit == item_rend)
                                            ? item.cend()
                                            : (last_tab_rit + 1).base());
  }
};

// Item type that wraps another, but applies a CtrlP match mode to their
// `match_key`s.
template <typename InnerItem, typename MatchMode>
struct CtrlPItem {
  InnerItem inner;

  CtrlPItem() {}
  explicit CtrlPItem(InnerItem inner) : inner(std::move(inner)) {}

  string_view match_key() const { return MatchMode()(inner.match_key()); }
  string_view sort_key() const { return inner.sort_key(); }
};

// Appends a set of Vim regexes to highlight the bytes at `positions` in `item`
// for the given highlight mode. `positions` must be sorted.
void get_highlight_regexes(string_view mode, string_view item,
                           std::vector<std::size_t> const& positions,
                           std::vector<std::string>& regexes,
                           string_view line_prefix);

}  // namespace cpsm

#endif /* CPSM_CTRLP_UTIL_H_ */
