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

#include <boost/program_options.hpp>
#include <boost/utility/string_ref.hpp>

#include "api.h"
#include "str_util.h"

namespace po = boost::program_options;

int main(int argc, char** argv) {
  std::cin.sync_with_stdio(false);
  std::cout.sync_with_stdio(false);
  std::cerr.sync_with_stdio(false);

  po::options_description opts_desc("Options");
  opts_desc.add_options()
      ("crfile", po::value<std::string>()->default_value(""),
       "'currently open file' passed to the matcher")
      ("limit", po::value<std::size_t>()->default_value(10),
       "maximum number of matches to return")
      ("query", po::value<std::string>()->default_value(""),
       "query to match items against")
      ("help", "display this help and exit")
      ;

  po::variables_map opts;
  po::store(po::parse_command_line(argc, argv, opts_desc), opts);
  po::notify(opts);

  if (opts.count("help")) {
    std::cout << opts_desc << std::endl;
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
