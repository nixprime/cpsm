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

#ifndef CPSM_CPSM_H_
#define CPSM_CPSM_H_

#include <algorithm>
#include <string>
#include <vector>

#include "match.h"
#include "matcher.h"
#include "par_util.h"
#include "str_util.h"

namespace cpsm {

// Performs a match for the given query over the given items, using the given
// set of options, and returns matches in descending quality order. The function
// f must take an item as input and return the string to be matched that it
// represents as a boost::string_ref. If limit is non-zero, at most limit
// matches will be returned. If max_concurrency is non-zero, at most
// max_concurrency concurrent matchers will be used.
template <typename T, typename F>
std::vector<T*> match(std::string const& query, std::vector<T>& items,
                      F const& f, MatcherOpts const& opts = MatcherOpts(),
                      std::size_t limit = 0, unsigned max_concurrency = 0) {
  unsigned nr_threads = Thread::hardware_concurrency();
  if (nr_threads > items.size()) {
    nr_threads = unsigned(items.size());
  }
  if (max_concurrency && (nr_threads > max_concurrency)) {
    nr_threads = max_concurrency;
  }
  std::size_t const items_per_thread = items.size() / nr_threads;
  std::size_t const first_thread_extra_items =
      nr_threads ? (items.size() % nr_threads) : 0;
  std::size_t start_item = 0;
  std::size_t end_item = items_per_thread + first_thread_extra_items;
  std::vector<std::vector<Match<T>>> thread_matches(nr_threads);
  std::vector<Thread> threads;
  for (unsigned i = 0; i < nr_threads; i++) {
    auto& matches = thread_matches[i];
    threads.emplace_back(
        [query, &items, f, opts, start_item, end_item, &matches]() {
          cpsm::Matcher matcher(std::move(query), std::move(opts));
          std::vector<char32_t> buf;
          for (std::size_t i = start_item; i < end_item; i++) {
            T& item = items[i];
            Match<T> m(item);
            if (matcher.match(f(item), m, &buf)) {
              matches.emplace_back(std::move(m));
            }
          }
        });
    start_item = end_item;
    end_item = start_item + items_per_thread;
  }
  std::vector<Match<T>> all_matches;
  for (unsigned i = 0; i < nr_threads; i++) {
    threads[i].join();
    if (threads[i].has_exception()) {
      throw Error(threads[i].exception_msg());
    }
    auto& matches = thread_matches[i];
    all_matches.reserve(all_matches.capacity() + matches.size());
    std::move(matches.begin(), matches.end(), std::back_inserter(all_matches));
    matches.clear();
    matches.shrink_to_fit();
  }
  if (limit && all_matches.size() > limit) {
    std::partial_sort(all_matches.begin(), all_matches.begin() + limit,
                      all_matches.end());
    all_matches.resize(limit);
  } else {
    std::sort(all_matches.begin(), all_matches.end());
  }
  std::vector<T*> match_items;
  for (auto const& m : all_matches) {
    match_items.push_back(m.item);
  }
  return match_items;
}

} // namespace cpsm

#endif /* CPSM_CPSM_H_ */
