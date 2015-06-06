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

#include "ctrlp_util.h"

#include <sstream>
#include <stdexcept>
#include <utility>

#include "path_util.h"

namespace cpsm {

namespace {

// Groups match positions into matched intervals.
std::set<std::pair<std::size_t, std::size_t>> group_positions_detailed(
    std::set<CharCount> const& positions) {
  std::set<std::pair<std::size_t, std::size_t>> groups;
  std::size_t begin = 0;
  std::size_t end = 0;
  for (CharCount const pos : positions) {
    if (pos != end) {
      // End of previous group, start of new group.
      if (begin != end) {
        groups.emplace(begin, end);
      }
      begin = end = pos;
    }
    end++;
  }
  if (begin != end) {
    groups.emplace(begin, end);
  }
  return groups;
}

// Returns a single match group spanning from the first to last match.
std::set<std::pair<std::size_t, std::size_t>> group_positions_basic(
    std::set<CharCount> const& positions) {
  std::set<std::pair<std::size_t, std::size_t>> group;
  if (!positions.empty()) {
    group.emplace(*positions.cbegin(), (*positions.crbegin()) + 1);
  }
  return group;
}

std::set<std::pair<std::size_t, std::size_t>> group_positions(
    boost::string_ref const mode, std::set<CharCount> const& positions) {
  if (mode == "basic") {
    return group_positions_basic(positions);
  } else if (mode == "detailed") {
    return group_positions_detailed(positions);
  } else {
    throw Error("unknown highlight mode '", mode, "'");
  }
}

} // anonymous namespace

std::function<boost::string_ref(boost::string_ref)> match_mode_item_substr_fn(
    boost::string_ref mmode) {
  if (mmode.empty() || mmode == "full-line") {
    return nullptr;
  } else if (mmode == "filename-only") {
    return path_basename;
  } else if (mmode == "first-non-tab") {
    return [](boost::string_ref const x)
        -> boost::string_ref { return x.substr(0, x.find_first_of('\t')); };
  } else if (mmode == "until-last-tab") {
    return [](boost::string_ref const x) -> boost::string_ref {
      auto const pos = x.find_last_of('\t');
      if (pos == boost::string_ref::npos) {
        return x;
      }
      return x.substr(pos + 1);
    };
  }
  throw Error("unknown match mode ", mmode);
}

void get_highlight_regexes(boost::string_ref const mode,
                           boost::string_ref const item,
                           std::set<CharCount> const& positions,
                           std::vector<std::string>& regexes) {
  for (auto const group : group_positions(mode, positions)) {
    // Each match group's regex has the same structure:
    // - "\V": very nomagic (only "\" needs to be escaped)
    // - "\C": forces case sensitivity
    // - "\^": beginning of string
    // - "> ": appears at the start of each line
    // - characters in the item before the match
    // - "\zs": starts the match
    // - characters in the match group
    // - "\ze": ends the match
    // - characters in the item after the match
    // - "\$": end of string
    std::size_t i = 0;
    std::string regex = R"(\V\C\^> )";
    auto const write_char = [&](std::size_t i) {
      if (item[i] == '\\') {
        regex += R"(\\)";
      } else {
        regex += item[i];
      }
    };
    for (; i < group.first; i++) {
      write_char(i);
    }
    regex += R"(\zs)";
    for (; i < group.second; i++) {
      write_char(i);
    }
    regex += R"(\ze)";
    for (; i < item.size(); i++) {
      write_char(i);
    }
    regex += R"(\$)";
    regexes.emplace_back(std::move(regex));
  }
}

}  // namespace cpsm
