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

#ifndef CPSM_MATCHER_H_
#define CPSM_MATCHER_H_

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <boost/utility/string_ref.hpp>

#include "path_util.h"
#include "str_util.h"

namespace cpsm {

// Internal type used for character counts.
//
// This is uint_fast16_t because cpsm is mostly used to match paths, and path
// lengths are not capable of exceeding the range of 16 bits on most major
// operating systems:
// - Linux: PATH_MAX = 4096
// - Mac OS X: PATH_MAX = 1024
// - Windows: MAX_PATH = 260; Unicode interfaces may support paths of up to
//   32767 characters
typedef std::uint_fast16_t CharCount;

// Options that apply to all items in a search.
struct MatcherOptions {
  // The currently open file.
  boost::string_ref crfile;

  // If false, do not consider the currently open file as a candidate.
  bool match_crfile = false;
};

// Type representing a match's score.
typedef std::uint64_t Score;

class MatchInfo {
 public:
  virtual ~MatchInfo() = default;

  // Returns the item's match score (higher is better).
  virtual Score score() const = 0;

  // Returns a string summarizing the state used to derive the last item's
  // match score.
  virtual std::string score_debug_string() const = 0;

  // Returns a sorted vector containing the position of each matched character
  // in the item.
  virtual std::vector<std::size_t> match_positions() const = 0;
};

template <typename PathTraits, typename StringTraits>
class Matcher : public MatchInfo {
 public:
  typedef typename StringTraits::Char Char;

  explicit Matcher(boost::string_ref const query, MatcherOptions const& opts)
      // Queries are smartcased (case-sensitive only if any uppercase appears
      // in the query).
      : query_(decode<StringTraits>(query)),
        query_basename_(path_basename<PathTraits>(query_.cbegin(),
                                                  query_.cend())),
        case_sensitive_(std::any_of(query_.cbegin(), query_.cend(),
                                    StringTraits::is_uppercase)),
        crfile_(decode<StringTraits>(opts.crfile)),
        crfile_basename_(path_basename<PathTraits>(crfile_.cbegin(),
                                                   crfile_.cend())),
        crfile_ext_(std::find_if(crfile_.crbegin(),
                                 ReverseIterator(crfile_basename_),
                                 PathTraits::is_extension_separator).base()),
        crfile_basename_word_ends_(find_word_endings(crfile_basename_,
                                                     crfile_ext_)),
        match_crfile_(opts.match_crfile) {}

  // A Matcher can't be trivially copied because it contains iterators into its
  // vectors.
  Matcher(Matcher const& other) = delete;
  Matcher& operator=(Matcher const& other) = delete;

  bool match(boost::string_ref const item) {
    item_.clear();
    decode_to<StringTraits>(item, item_);

    // Determine if the query matches at all.
    if (!scan()) {
      return false;
    }

    // Check for compatibility with `crfile`.
    if (!check_crfile()) {
      return false;
    }

    // Beyond this point, the item is definitely a match, and we're only
    // evaluating its quality. Reset scoring state (other than what's already
    // been assigned by `check_crfile`).
    prefix_level_ = PrefixLevel::NONE;
    whole_basename_match_ = false;
    basename_longest_submatch_ = 0;
    basename_match_count_ = 0;
    basename_word_gaps_ = 0;

    // Don't waste any time on empty queries, which can't do any further
    // differentiation between items. Also return early if the item is empty,
    // so that the remainder of the algorithm can assume it isn't.
    if (query_.empty() || item_.empty()) {
      return true;
    }

    // If the match is case-insensitive, the query must not contain any
    // uppercase letters. Convert all uppercase characters in the item to
    // lowercase so matching below this point is simply equality comparison.
    make_item_matchcase();

    // Try to constrain the match so that matches are required at the start of
    // matched path components.
    if (!check_component_match_front()) {
      // If that fails, conclude that the match is bad and don't do any further
      // matching.
      return true;
    }

    // Try to additionally constrain the match so that all matches in the
    // basename (rightmost path component) occur at the beginning of "words".
    if (check_basename_match_word_prefix()) {
      score_basename_word_prefix_match();
    } else {
      // If that fails, fall back to simple greedy matching.
      score_basename_greedy();
    }

    return true;
  }

  Score score() const final {
    return (Score(prefix_level_) << 62) |
           (Score(whole_basename_match_) << 61) |
           (mask_to(basename_longest_submatch_, 7) << 54) |
           (mask_to(basename_match_count_, 7) << 47) |
           (mask_to(penalty(basename_word_gaps_), 7) << 40) |
           (mask_to(crfile_basename_shared_words_, 7) << 33) |
           (mask_to(penalty(crfile_path_distance_), 11) << 22) |
           (mask_to(penalty(unmatched_suffix_len_), 8) << 14) |
           mask_to(penalty(item_.size()), 14);
  }

  std::string score_debug_string() const final {
    return str_cat("prefix_level = ", Score(prefix_level_),
                   ", whole_basename_match = ", whole_basename_match_,
                   ", basename_longest_submatch = ", basename_longest_submatch_,
                   ", basename_match_count = ", basename_match_count_,
                   ", basename_word_gaps = ", basename_word_gaps_,
                   ", crfile_basename_shared_words = ",
                   crfile_basename_shared_words_, ", crfile_path_distance = ",
                   crfile_path_distance_, ", unmatched_suffix_len = ",
                   unmatched_suffix_len_, ", item_len = ", item_.size());
  }

  std::vector<std::size_t> match_positions() const final {
    std::vector<std::size_t> posns;
    if (prefix_level_ == PrefixLevel::NONE) {
      get_match_positions_sorted_no_prefix(posns);
      return posns;
    }
    get_match_positions_component_prefix_dirpath(posns);
    if (prefix_level_ == PrefixLevel::BASENAME_WORD) {
      get_match_positions_basename_word_prefix(posns);
    } else {
      get_match_positions_basename_non_word_prefix(posns);
    }
    std::sort(posns.begin(), posns.end());
    return posns;
  }

 private:
  typedef std::vector<Char> Vec;
  typedef typename Vec::const_iterator Iterator;
  typedef typename Vec::const_reverse_iterator ReverseIterator;

  static std::vector<Iterator> find_word_endings(Iterator const first,
                                                 Iterator const last) {
    std::vector<Iterator> word_ends;
    bool prev_uppercase = false;
    bool prev_alphanumeric = false;
    for (auto it = first; it != last; ++it) {
      auto const c = *it;
      bool const next_uppercase = StringTraits::is_uppercase(c);
      bool const next_alphanumeric = StringTraits::is_alphanumeric(c);
      if (prev_alphanumeric &&
          (!next_alphanumeric || (!prev_uppercase && next_uppercase))) {
        word_ends.push_back(it - 1);
      }
      prev_uppercase = next_uppercase;
      prev_alphanumeric = next_alphanumeric;
    }
    if (prev_alphanumeric) {
      word_ends.push_back(last - 1);
    }
    return word_ends;
  }

  bool scan() {
    props_.resize(item_.size());
    auto props_it = props_.begin();
    for (auto item_it = item_.cbegin(), item_last = item_.cend();
         item_it != item_last; ++item_it, ++props_it) {
      props_it->uppercase = StringTraits::is_uppercase(*item_it);
    }
    if (case_sensitive_) {
      return scan_match<true>();
    } else {
      return scan_match<false>();
    }
  }

  template <bool CaseSensitive>
  bool scan_match() const {
    auto query_it = query_.cbegin();
    auto const query_last = query_.cend();
    if (query_it == query_last) {
      return true;
    }
    auto props_it = props_.cbegin();
    for (auto item_it = item_.cbegin(), item_last = item_.cend();
         item_it != item_last; ++item_it, ++props_it) {
      auto c = *item_it;
      // If the match is case-insensitive, the query must not contain any
      // uppercase letters.
      if (!CaseSensitive && props_it->uppercase) {
        c = StringTraits::uppercase_to_lowercase(c);
      }
      if (c == *query_it) {
        ++query_it;
        if (query_it == query_last) {
          return true;
        }
      }
    }
    return false;
  }

  bool check_crfile() {
    crfile_path_distance_ = path_distance<PathTraits>(
        item_.cbegin(), item_.cend(), crfile_.cbegin(), crfile_.cend());
    if (!match_crfile_ && crfile_path_distance_ == 0) {
      return false;
    }
    // If the last character in the item is a path separator, skip it for the
    // purposes of determining the item basename to be consistent with
    // `consume_path_component_match_front`.
    if (!item_.empty() && PathTraits::is_path_separator(item_.back())) {
      item_basename_ =
          path_basename<PathTraits>(item_.cbegin(), item_.cend() - 1);
    } else {
      item_basename_ = path_basename<PathTraits>(item_.cbegin(), item_.cend());
    }
    auto props_it = props_.begin() + (item_basename_ - item_.cbegin());
    for (auto item_it = item_basename_, item_last = item_.cend();
         item_it != item_last; ++item_it, ++props_it) {
      props_it->alphanumeric = StringTraits::is_alphanumeric(*item_it);
    }
    crfile_basename_shared_words_ = [this]() -> CharCount {
      auto crfile_word_end_it = crfile_basename_word_ends_.cbegin();
      auto const crfile_word_end_last = crfile_basename_word_ends_.cend();
      if (crfile_word_end_it == crfile_word_end_last) {
        return 0;
      }
      for (auto item_it = item_basename_, item_last = item_.cend(),
                crfile_it = crfile_basename_, crfile_last = crfile_.cend();
           item_it != item_last && crfile_it != crfile_last &&
               *item_it == *crfile_it;
           ++item_it, ++crfile_it) {
        if (crfile_it == *crfile_word_end_it) {
          ++crfile_word_end_it;
          if (crfile_word_end_it == crfile_word_end_last) {
            // Only counts if the next character is plausibly not the
            // continuation of a word.
            std::size_t const i = item_it - item_.cbegin();
            if ((i + 1 < item_.size()) && !props_[i + 1].uppercase &&
                props_[i + 1].alphanumeric) {
              --crfile_word_end_it;
            }
            break;
          }
        }
      }
      return crfile_word_end_it - crfile_basename_word_ends_.cbegin();
    }();
    // Ensure that `unmatched_suffix_len_` is initialized even for empty
    // queries.
    unmatched_suffix_len_ = item_.cend() - item_basename_;
    return true;
  }

  void make_item_matchcase() {
    if (!case_sensitive_) {
      auto props_it = props_.cbegin();
      for (auto item_it = item_.begin(), item_last = item_.end();
           item_it != item_last; ++item_it, ++props_it) {
        if (props_it->uppercase) {
          *item_it = StringTraits::uppercase_to_lowercase(*item_it);
        }
      }
    }
  }

  bool check_component_match_front() {
    auto item_rit = item_.crbegin();
    auto const item_rlast = item_.crend();
    auto query_rit = query_.crbegin();
    auto const query_rlast = query_.crend();

    // Consume the basename.
    consume_path_component_match_front(item_rit, item_rlast, query_rit,
                                       query_rlast);
    qit_basename_ = query_rit.base();
    whole_basename_match_ = qit_basename_ == query_basename_;
    basename_match_count_ = query_.cend() - qit_basename_;

    // Try to consume the remainder of the query.
    while (query_rit != query_rlast) {
      if (item_rit == item_rlast) {
        return false;
      }
      consume_path_component_match_front(item_rit, item_rlast, query_rit,
                                         query_rlast);
    }
    prefix_level_ = PrefixLevel::COMPONENT;
    return true;
  }

  // Advances `item_rit` to the next path separator before `item_rlast`. For
  // each iterated character in the item matched by a character in the query
  // before `query_rlast`, advances `query_rit`. At the end of the path
  // component, backtrack the match to ensure that if any matches occur, they
  // include the last character matched before the path separator.
  //
  // Precondition: `item_rit != item_rlast`.
  // Postcondition: `item_rit` is advanced by at least 1.
  void consume_path_component_match_front(
      ReverseIterator& item_rit, ReverseIterator const item_rlast,
      ReverseIterator& query_rit, ReverseIterator const query_rlast) const {
    auto const query_last = query_rit.base();
    while (true) {
      if (query_rit != query_rlast && *item_rit == *query_rit) {
        ++query_rit;
      }
      ++item_rit;
      if (item_rit == item_rlast ||
          PathTraits::is_path_separator(*item_rit)) {
        break;
      }
    }
    auto const item_pc_front = *item_rit.base();
    auto query_it = query_rit.base();
    for (; query_it != query_last; ++query_it) {
      if (item_pc_front == *query_it) {
        break;
      }
    }
    query_rit = ReverseIterator(query_it);
  }

  bool check_basename_match_word_prefix() {
    qit_basename_words_.clear();
    qit_basename_words_.push_back(qit_basename_);

    auto item_it = item_basename_;
    auto const item_last = item_.cend();
    if (item_it == item_last) {
      return false;
    }
    auto query_it = qit_basename_;
    auto const query_last = query_.cend();
    if (query_it == query_last) {
      return false;
    }
    auto props_it = props_.begin() + (item_basename_ - item_.cbegin());

    bool prev_uppercase = props_it->uppercase;
    bool prev_alphanumeric = props_it->alphanumeric;
    props_it->word_start = true;

    // Advances `item_it` and `props_it` to the beginning of the next word. For
    // each consecutive iterated character in the item matched by a character
    // in the query before `query_last`, advances `query_it`.
    //
    // Precondition: `item_it != item_last`; `query_it != query_last`.
    // Postcondition: `item_it` is always advanced by at least 1.
    auto const consume_word_prefix = [&] {
      bool can_match = true;
      while (true) {
        // Require that all alphanumeric matches in this word be contiguous.
        if (can_match || !prev_alphanumeric) {
          if (*item_it == *query_it) {
            ++query_it;
            if (query_it == query_last) {
              break;
            }
          } else {
            can_match = false;
          }
        }
        ++item_it;
        if (item_it == item_last) {
          break;
        }
        ++props_it;
        bool const uppercase = props_it->uppercase;
        bool const alphanumeric = props_it->alphanumeric;
        bool const word_start = (!prev_uppercase && uppercase) ||
                                (!prev_alphanumeric && alphanumeric);
        props_it->word_start = word_start;
        prev_uppercase = uppercase;
        prev_alphanumeric = alphanumeric;
        if (word_start) {
          break;
        }
      }
    };

    consume_word_prefix();
    while (query_it != query_last) {
      if (item_it == item_last) {
        basename_longest_submatch_ = 0;
        basename_word_gaps_ = 0;
        return false;
      }
      // If the next unmatched query character doesn't match the first
      // character of the next word, allow partial backtracking (all but the
      // first character) of the match in the previous word in order to find a
      // match for this one.
      auto const c = *item_it;
      if (c != *query_it) {
        for (auto steal_rit = ReverseIterator(query_it),
                  steal_rlast = ReverseIterator(qit_basename_words_.back() + 1);
             steal_rit < steal_rlast; ++steal_rit) {
          if (c == *steal_rit) {
            query_it = (steal_rit + 1).base();
            break;
          }
        }
      }
      qit_basename_words_.push_back(query_it);
      consume_word_prefix();
    }
    prefix_level_ = PrefixLevel::BASENAME_WORD;
    // Push `query_it` onto `qit_basename_words_` even though we know it's
    // `query_.cend()` to avoid special-casing the end case in
    // `score_basename_word_prefix_match`.
    qit_basename_words_.push_back(query_it);
    return true;
  }

  void score_basename_word_prefix_match() {
    auto item_it = item_basename_;
    auto props_it = props_.cbegin() + (item_basename_ - item_.cbegin());
    auto query_it = qit_basename_;
    auto const query_last = query_.cend();
    // +1 because the first iteration of the loop skips the word start at the
    // beginning of the basename.
    auto qit_words_it = qit_basename_words_.cbegin() + 1;
    auto query_word_last = *qit_words_it;

    CharCount current_submatch = 0;
    bool any_word_matches = false;

    while (true) {
      if (query_it != query_word_last && *item_it == *query_it) {
        ++query_it;
        current_submatch++;
        any_word_matches = true;
        if (query_it == query_last) {
          break;
        }
      } else {
        basename_longest_submatch_ =
            std::max(basename_longest_submatch_, current_submatch);
        current_submatch = 0;
      }
      ++item_it;
      // At this point we know that the basename *is* a word prefix match, so
      // fully consuming the end of the query should be the only possible way
      // to leave this loop. Hence we skip the comparison to `item_.cend()`.
      // (The same applies to `qit_words_it` and `qit_basename_words_.cend()`
      // below.)
      ++props_it;
      if (props_it->word_start) {
        if (!any_word_matches) {
          basename_word_gaps_++;
        }
        any_word_matches = false;
        ++qit_words_it;
        query_word_last = *qit_words_it;
      }
    }
    basename_longest_submatch_ =
        std::max(basename_longest_submatch_, current_submatch);
    // -1 here because we broke out upon reaching the last match (`query_it ==
    // query_last`) before incrementing `item_it`.
    unmatched_suffix_len_ = item_.cend() - item_it - 1;
  }

  void score_basename_greedy() {
    auto item_it = item_basename_;
    auto const item_last = item_.cend();
    auto query_it = qit_basename_;
    auto const query_last = query_.cend();
    if (item_it == item_last || query_it == query_last) {
      return;
    }

    CharCount current_submatch = 0;

    while (true) {
      if (*item_it == *query_it) {
        ++query_it;
        current_submatch++;
        if (query_it == query_last) {
          break;
        }
      } else {
        basename_longest_submatch_ =
            std::max(basename_longest_submatch_, current_submatch);
        current_submatch = 0;
      }
      ++item_it;
      if (item_it == item_last) {
        break;
      }
    }
    basename_longest_submatch_ =
        std::max(basename_longest_submatch_, current_submatch);
    // -1 here because we broke out upon reaching the last match (`query_it ==
    // query_last`) before incrementing `item_it`.
    unmatched_suffix_len_ = item_last - item_it - 1;
  }

  // In all of these `get_match_positions_*` functions, we assume that the
  // match state is consistent with a successful last match.

  void get_match_positions_sorted_no_prefix(
      std::vector<std::size_t>& posns) const {
    get_match_positions_greedy(posns, item_.cbegin(), item_.cbegin(),
                               query_.cbegin(), query_.cend());
  }

  void get_match_positions_component_prefix_dirpath(
      std::vector<std::size_t>& posns) const {
    auto item_rit = ReverseIterator(item_basename_);
    auto const item_rlast = item_.crend();
    auto const item_first = item_.cbegin();
    auto query_rit = ReverseIterator(qit_basename_);
    auto const query_rlast = query_.crend();
    auto query_pc_last = query_rit.base();

    while (query_rit != query_rlast) {
      consume_path_component_match_front(item_rit, item_rlast, query_rit,
                                         query_rlast);
      get_match_positions_greedy(posns, item_first, item_rit.base(),
                                 query_rit.base(), query_pc_last);
      query_pc_last = query_rit.base();
    }
  }

  void get_match_positions_basename_word_prefix(
      std::vector<std::size_t>& posns) const {
    auto item_it = item_basename_;
    auto const item_first = item_.cbegin();
    auto const item_last = item_.cend();
    auto props_it = props_.cbegin() + (item_basename_ - item_.cbegin());
    auto query_it = qit_basename_;
    auto query_last_it = qit_basename_words_.cbegin();

    while (item_it != item_last) {
      if (props_it->word_start) {
        ++query_last_it;
      }
      if (query_it != *query_last_it && *item_it == *query_it) {
        ++query_it;
        posns.push_back(item_it - item_first);
      }
      ++item_it;
      ++props_it;
    }
  }

  void get_match_positions_basename_non_word_prefix(
      std::vector<std::size_t>& posns) const {
    get_match_positions_greedy(posns, item_.cbegin(), item_basename_,
                               qit_basename_, query_.cend());
  }

  template <typename InputIt1, typename InputIt2>
  void get_match_positions_greedy(std::vector<std::size_t>& posns,
                                  InputIt1 const item_first, InputIt1 item_it,
                                  InputIt2 query_it,
                                  InputIt2 const query_last) const {
    auto const item_last = item_.cend();
    while (item_it != item_last && query_it != query_last) {
      if (*item_it == *query_it) {
        ++query_it;
        posns.push_back(item_it - item_first);
      }
      ++item_it;
    }
  }

  static constexpr Score mask_to(Score const x, unsigned const bits) {
    return x & ((std::uint64_t(1) << bits) - 1);
  }

  template <typename T>
  static constexpr T penalty(T const x) {
    return std::numeric_limits<T>::max() - x;
  }

  // Internal state of an in-progress match on an item. Note that many of these
  // fields are set conditionally; see the implementation for details.

  // Decoded copy of the item being matched.
  Vec item_;

  // Iterator into `item_` at the beginning of the item's basename.
  Iterator item_basename_;

  // Properties of characters in the item.
  struct CharProperties {
    // If true, the character is uppercase.
    bool uppercase;

    // If true, the character is alphanumeric.
    bool alphanumeric;

    // If true, the character is the start of a word.
    bool word_start;
  };
  std::vector<CharProperties> props_;

  // Iterator into `query_` at the first character matching in the item's
  // basename.
  Iterator qit_basename_;

  // Iterators into `query_` before matching each word in the item's basename.
  // Note that if both are set, then `qit_basename_ ==
  // qit_basename_words_[0]`.
  std::vector<Iterator> qit_basename_words_;

  // Metrics used to compute score, in order of descending significance.

  // Incrementally stronger statements about the quality of the match. Find
  // locations where this field is assigned for details. Higher numeric value
  // is better.
  enum class PrefixLevel {
    NONE,
    COMPONENT,
    BASENAME_WORD,
  } prefix_level_;

  // If true, the basename of the query matches entirely in the basename of the
  // item. True is better.
  bool whole_basename_match_;

  // The length of the longest substring matched in the item's basename. Higher
  // is better.
  CharCount basename_longest_submatch_;

  // The number of characters matched in the item's basename. Higher is better.
  CharCount basename_match_count_;

  // The number of words without any matches between the first and last words
  // with matches in the basename. Lower is better.
  CharCount basename_word_gaps_;

  // The number of consecutive words shared between the beginning of the item's
  // basename and the beginning of the current file's basename. Higher is
  // better.
  CharCount crfile_basename_shared_words_;

  // The number of path components that must be traversed between the item's
  // path and the current file's path. Lower is better.
  CharCount crfile_path_distance_;

  // The number of consecutive unmatched characters at the end of the item's
  // basename. Since it's easy to add characters at the end of a query to
  // refine a search for a longer item, lower values are weakly preferred.
  CharCount unmatched_suffix_len_;

  // Matcher state that is persistent between matches.

  // Decoded copy of the query.
  Vec const query_;

  // Iterator into `query_` at the beginning of the query's basename.
  Iterator const query_basename_;

  // If true, the match is case-sensitive.
  bool const case_sensitive_;

  // Decoded copy of the currently open filename.
  Vec const crfile_;

  // Iterator into `crfile_` at the beginning of the currently open file's
  // basename.
  Iterator const crfile_basename_;

  // Iterator into `crfile_` at the successor to the currently open file's
  // rightmost extension separator.
  Iterator const crfile_ext_;

  // Iterators into `crfile_` at the last character of each word in the
  // currently open file's basename.
  std::vector<Iterator> const crfile_basename_word_ends_;

  // If false, reject `crfile_` if it appears as an item.
  bool const match_crfile_;
};

}  // namespace cpsm

#endif /* CPSM_MATCHER_H_ */
