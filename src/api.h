// cpsm - fuzzy path matcher
// Copyright (C) 2016 the Authors
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

#ifndef CPSM_API_H_
#define CPSM_API_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <boost/utility/string_ref.hpp>

#include "matcher.h"
#include "par_util.h"
#include "str_util.h"

namespace cpsm {

// User options that influence match behavior.
struct Options {
 private:
  // The currently open file.
  boost::string_ref crfile_;

  // The maximum number of matches to return. If 0, there is no limit.
  std::size_t limit_ = 0;

  // If false, do not consider the currently open file as a candidate.
  bool match_crfile_ = false;

  // The number of threads the matcher should use.
  unsigned int nr_threads_ = 1;

  // If true, the query and all items are paths.
  bool path_ = true;

  // If true, attempt to interpret the query and all items as UTF-8-encoded
  // strings.
  bool unicode_ = false;

  // If true, pass `match_info` to match sinks.
  bool want_match_info_ = false;

 public:
  boost::string_ref crfile() const { return crfile_; }
  Options& set_crfile(boost::string_ref const crfile) {
    crfile_ = crfile;
    return *this;
  }

  std::size_t limit() const { return limit_; }
  Options& set_limit(std::size_t const limit) {
    limit_ = limit;
    return *this;
  }

  bool match_crfile() const { return match_crfile_; }
  Options& set_match_crfile(bool const match_crfile) {
    match_crfile_ = match_crfile;
    return *this;
  }

  unsigned int nr_threads() const { return nr_threads_; }
  Options& set_nr_threads(unsigned int const nr_threads) {
    if (nr_threads < 1) {
      throw Error("invalid nr_threads: ", nr_threads);
    }
    nr_threads_ = nr_threads;
    return *this;
  }

  bool path() const { return path_; }
  Options& set_path(bool const path) {
    path_ = path;
    return *this;
  }

  bool unicode() const { return unicode_; }
  Options& set_unicode(bool const unicode) {
    unicode_ = unicode;
    return *this;
  }

  bool want_match_info() const { return want_match_info_; }
  Options& set_want_match_info(bool const want_match_info) {
    want_match_info_ = want_match_info;
    return *this;
  }
};

namespace detail {

template <typename PathTraits, typename StringTraits, typename Item,
          typename Source, typename Sink>
void for_each_match(boost::string_ref const query, Options const& opts,
                    Source&& src, Sink&& dst);

}  // namespace detail

// For each item in a list of items, invoke `dst` in descending order of
// compatibility with the given query in the given context with the given
// options.
//
// `Item` must be a default-constructable, movable type with the following
// member functions:
// - `match_key`, which returns a `boost::string_ref` representing the string
//   that the query should match against.
// - `sort_key`, which returns a value of unspecified type that can be compared
//   to other values of the same type with operator `<`. When the matcher is
//   otherwise unable to order two matched items, it will prefer the one whose
//   `sort_key` compares lower.
//
// `src` must have the following member functions:
// - `bool fill(std::vector<Item>& items)`, which inserts new unmatched items
// into `items` (which must initially be empty) and returns true iff it may
// produce more unmatched items in the future.
// - `size_t batch_size() const`, which returns an optional upper bound on the
// number of items inserted by each call to `fill`.
// If `opts.nr_threads() > 1`, `src` must be thread-safe.
//
// `dst` must be a functor compatible with signature `void(Item& item,
// MatchInfo const* match_info)`, where `item` is a matched item and
// `match_info`, if not null, holds the state of the match. `dst` need not be
// thread-safe.
//
// Example:
//
//   // Prints the top 10 matches of query against items.
//   for_each_match<Item>(
//       query, Options().set_limit(10).set_unicode(true),
//       [&](std::vector<Item>& batch) {
//         if (items.empty()) return false;
//         batch.push_back(std::move(items.back()));
//         items.pop_back();
//         return true;
//       },
//       [&](Item item, void*) {
//         std::cout << item.item << std::endl;
//       });
template <typename Item, typename Source, typename Sink>
void for_each_match(boost::string_ref const query, Options const& opts,
                    Source&& src, Sink&& dst) {
  if (opts.path()) {
    if (opts.unicode()) {
      detail::for_each_match<PlatformPathTraits, Utf8StringTraits, Item>(
          query, opts, std::forward<Source>(src), std::forward<Sink>(dst));
    } else {
      detail::for_each_match<PlatformPathTraits, SimpleStringTraits, Item>(
          query, opts, std::forward<Source>(src), std::forward<Sink>(dst));
    }
  } else {
    if (opts.unicode()) {
      detail::for_each_match<NonPathTraits, Utf8StringTraits, Item>(
          query, opts, std::forward<Source>(src), std::forward<Sink>(dst));
    } else {
      detail::for_each_match<NonPathTraits, SimpleStringTraits, Item>(
          query, opts, std::forward<Source>(src), std::forward<Sink>(dst));
    }
  }
}

// Simple Item type wrapping a `boost::string_ref`.
class StringRefItem {
 public:
  StringRefItem() {}
  explicit StringRefItem(boost::string_ref const item) : item_(item) {}

  boost::string_ref item() const { return item_; }
  boost::string_ref match_key() const { return item_; }
  boost::string_ref sort_key() const { return item_; }

 private:
  boost::string_ref item_;
};

// Thread-unsafe source functor that constructs items from elements of a range
// defined by a pair of iterators.
template <typename Item, typename It>
class RangeSource {
 public:
  explicit RangeSource(It first, It last)
      : it_(std::move(first)), last_(std::move(last)) {}

  bool fill(std::vector<Item>& items) {
    if (it_ == last_) {
      return false;
    }
    items.emplace_back(*it_);
    ++it_;
    return it_ != last_;
  }

  static constexpr size_t batch_size() { return 1; }

 private:
  It it_;
  It const last_;
};

template <typename Item, typename It>
RangeSource<Item, It> source_from_range(It first, It last) {
  return RangeSource<Item, It>(std::move(first), std::move(last));
}

namespace detail {

// Type binding a matched item together with its score.
template <typename Item>
struct Matched {
  Score score;
  Item item;

  Matched() {}
  explicit Matched(Score score, Item item)
      : score(score), item(std::move(item)) {}

  // Returns true if `x` is a better match than `y`.
  static bool is_better(Matched<Item> const &x, Matched<Item> const &y) {
    if (x.score != y.score) {
      return x.score > y.score;
    }
    return x.item.sort_key() < y.item.sort_key();
  }
};

template <typename PathTraits, typename StringTraits, typename Item,
          typename Source, typename Sink>
void for_each_match(boost::string_ref const query, Options const& opts,
                    Source&& src, Sink&& dst) {
  MatcherOptions mopts;
  mopts.crfile = opts.crfile();
  mopts.match_crfile = opts.match_crfile();

  // Match in parallel.
  std::vector<std::vector<Matched<Item>>> thread_matches(opts.nr_threads());
  std::vector<Thread> threads;
  threads.reserve(opts.nr_threads());
  for (unsigned int i = 0; i < opts.nr_threads(); i++) {
    threads.emplace_back([&, i] {
      std::vector<Matched<Item>> matches;
      std::vector<Item> batch;
      // If a limit exists, each thread should only keep that many matches.
      if (opts.limit()) {
        matches.reserve(opts.limit() + 1);
      }
      batch.reserve(src.batch_size());
      Matcher<PathTraits, StringTraits> matcher(query, mopts);
      bool more;
      do {
        // Collect and match a batch.
        more = src.fill(batch);
        for (auto& item : batch) {
          if (matcher.match(item.match_key())) {
            matches.emplace_back(matcher.score(), std::move(item));
            if (opts.limit()) {
              std::push_heap(matches.begin(), matches.end(),
                             Matched<Item>::is_better);
              if (matches.size() > opts.limit()) {
                std::pop_heap(matches.begin(), matches.end(),
                              Matched<Item>::is_better);
                matches.pop_back();
              }
            }
          }
        }
        batch.clear();
      } while (more);
      thread_matches[i] = std::move(matches);
    });
  }

  // Collect matcher threads.
  std::size_t nr_matches = 0;
  for (unsigned int i = 0; i < opts.nr_threads(); i++) {
    auto& thread = threads[i];
    thread.join();
    if (thread.has_exception()) {
      throw Error(thread.exception_msg());
    }
    nr_matches += thread_matches[i].size();
  }

  // Combine per-thread match lists.
  std::vector<Matched<Item>> all_matches;
  all_matches.reserve(nr_matches);
  for (auto& matches : thread_matches) {
    std::move(matches.begin(), matches.end(), std::back_inserter(all_matches));
    matches.shrink_to_fit();
  }

  // Sort and limit matches.
  if (opts.limit() && opts.limit() < all_matches.size()) {
    std::partial_sort(all_matches.begin(), all_matches.begin() + opts.limit(),
                      all_matches.end(), Matched<Item>::is_better);
    all_matches.resize(opts.limit());
  } else {
    std::sort(all_matches.begin(), all_matches.end(), Matched<Item>::is_better);
  }

  // Emit matches.
  if (opts.want_match_info()) {
    Matcher<PathTraits, StringTraits> matcher(query, mopts);
    for (auto& match : all_matches) {
      if (!matcher.match(match.item.match_key())) {
        throw Error("failed to re-match known match '",
                    match.item.match_key(),
                    "' during match position collection");
      }
      dst(match.item, &matcher);
    }
  } else {
    for (auto& match : all_matches) {
      dst(match.item, nullptr);
    }
  }
}

}  // namespace detail

}  // namespace cpsm

#endif  // CPSM_API_H_
