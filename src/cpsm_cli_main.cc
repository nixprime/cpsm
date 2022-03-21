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

#include <algorithm>
#include <iostream>
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

#include "api.h"
#include "str_util.h"

#include "cxxopts.hpp"

int main(int argc, char** argv) {
  std::cin.sync_with_stdio(false);
  std::cout.sync_with_stdio(false);
  std::cerr.sync_with_stdio(false);

  cxxopts::Options options("Options", "Options for cpsm cli");

  options.add_options()
      ("crfile", "'currently open file' passed to the matcher",
       cxxopts::value<std::string>()->default_value(""))
      ("limit", "maximum number of matches to return",
       cxxopts::value<std::size_t>()->default_value("10"))
      ("query", "query to match items against", 
       cxxopts::value<std::string>()->default_value(""))
      ("help", "display this help and exit")
      ;

  const auto opts = options.parse(argc, argv);

  if (opts.count("help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  std::vector<std::string> lines;
  std::string line;
  while (std::getline(std::cin, line)) {
    lines.emplace_back(std::move(line));
    line.clear();
  }

  auto const crfile = opts["crfile"].as<std::string>();
  auto const limit = opts["limit"].as<std::size_t>();
  auto const query = opts["query"].as<std::string>();
  auto const mopts =
      cpsm::Options().set_crfile(crfile).set_limit(limit).set_want_match_info(
          true);
  cpsm::for_each_match<cpsm::StringRefItem>(
      query, mopts, cpsm::source_from_range<cpsm::StringRefItem>(lines.cbegin(),
                                                                 lines.cend()),
      [&](cpsm::StringRefItem item, cpsm::MatchInfo const* info) {
        std::cout << item.item() << "\n- score: " << info->score() << "; "
                  << info->score_debug_string() << "\n- match positions: "
                  << cpsm::str_join(info->match_positions(), ", ") << std::endl;
      });

  return 0;
}
