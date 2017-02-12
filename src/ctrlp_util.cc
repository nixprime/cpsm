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

#include "ctrlp_util.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace cpsm {

namespace {

// Groups match positions into matched intervals.
std::vector<std::pair<std::size_t, std::size_t>> group_positions_detailed(
    std::vector<std::size_t> const& positions) {
  std::vector<std::pair<std::size_t, std::size_t>> groups;
  std::size_t begin = 0;
  std::size_t end = 0;
  for (std::size_t const pos : positions) {
    if (pos != end) {
      // End of previous group, start of new group.
      if (begin != end) {
        groups.emplace_back(begin, end);
      }
      begin = end = pos;
    }
    end++;
  }
  if (begin != end) {
    groups.emplace_back(begin, end);
  }
  return groups;
}

// Returns a single match group spanning from the first to last match.
std::vector<std::pair<std::size_t, std::size_t>> group_positions_basic(
    std::vector<std::size_t> const& positions) {
  std::vector<std::pair<std::size_t, std::size_t>> group;
  if (!positions.empty()) {
    group.emplace_back(*positions.cbegin(), (*positions.crbegin()) + 1);
  }
  return group;
}

std::vector<std::pair<std::size_t, std::size_t>> group_positions(
    boost::string_ref const mode, std::vector<std::size_t> const& positions) {
  if (mode.empty() || mode == "none") {
    return std::vector<std::pair<std::size_t, std::size_t>>();
  } else if (mode == "basic") {
    return group_positions_basic(positions);
  } else if (mode == "detailed") {
    return group_positions_detailed(positions);
  }
  throw Error("unknown highlight mode '", mode, "'");
}

}  // anonymous namespace

CtrlPMatchMode parse_ctrlp_match_mode(boost::string_ref const mmode) {
  if (mmode.empty() || mmode == "full-line") {
    return CtrlPMatchMode::FULL_LINE;
  } else if (mmode == "filename-only") {
    return CtrlPMatchMode::FILENAME_ONLY;
  } else if (mmode == "first-non-tab") {
    return CtrlPMatchMode::FIRST_NON_TAB;
  } else if (mmode == "until-last-tab") {
    return CtrlPMatchMode::UNTIL_LAST_TAB;
  }
  throw Error("unknown match mode ", mmode);
}

void get_highlight_regexes(boost::string_ref const mode,
                           boost::string_ref const item,
                           std::vector<std::size_t> const& positions,
                           std::vector<std::string>& regexes,
                           boost::string_ref const line_prefix) {
  for (auto const group : group_positions(mode, positions)) {
    // Each match group's regex has the same structure:
    // - "\V": very nomagic (only "\" needs to be escaped)
    // - "\C": forces case sensitivity
    // - "\^": beginning of string
    // - the line prefix
    // - characters in the item before the match
    // - "\zs": starts the match
    // - characters in the match group
    // - "\ze": ends the match
    // - characters in the item after the match
    // - "\$": end of string
    std::string regex = R"(\V\C\^)";
    auto const write_char = [&](char c) {
      if (c == '\\') {
        regex += R"(\\)";
      } else {
        regex += c;
      }
    };
    for (char const c : line_prefix) {
      write_char(c);
    }
    std::size_t i = 0;
    for (; i < group.first; i++) {
      write_char(item[i]);
    }
    regex += R"(\zs)";
    for (; i < group.second; i++) {
      write_char(item[i]);
    }
    regex += R"(\ze)";
    for (; i < item.size(); i++) {
      write_char(item[i]);
    }
    regex += R"(\$)";
    regexes.emplace_back(std::move(regex));
  }
}

}  // namespace cpsm
