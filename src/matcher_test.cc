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
#include <cinttypes>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

#include "api.h"
#include "str_util.h"

namespace cpsm {
namespace testing {

class TestAssertionFailure : public std::exception {
 public:
  TestAssertionFailure() : msg_("test assertion failed") {}

  template <typename... Args>
  explicit TestAssertionFailure(Args... args)
      : msg_(str_cat("test assertion failed: ", args...)) {}

  char const* what() const noexcept override { return msg_.c_str(); }

 private:
  std::string msg_;
};

struct Matches {
  using Vec = std::vector<std::string>;
  using size_type = typename Vec::size_type;
  Vec matches;

  typename Vec::const_iterator find(boost::string_ref const item) const {
    return std::find(matches.cbegin(), matches.cend(), item);
  }

  bool matched(boost::string_ref const item) const {
    return find(item) != matches.cend();
  }

  void assert_matched(boost::string_ref const item) const {
    if (!matched(item)) {
      throw TestAssertionFailure("incorrectly failed to match '", item, "'");
    }
  }

  void assert_not_matched(boost::string_ref const item) const {
    if (matched(item)) {
      throw TestAssertionFailure("incorrectly matched '", item, "'");
    }
  }

  size_type match_index(boost::string_ref const item) const {
    return find(item) - matches.cbegin();
  }

  void assert_match_index(boost::string_ref const item,
                          size_type const expected_index) const {
    auto const index = match_index(item);
    if (index != expected_index) {
      throw TestAssertionFailure("expected '", item, "' (index ", index,
                                 ") to have index ", expected_index);
    }
  }

  void assert_better_match(boost::string_ref const better_item,
                           boost::string_ref const worse_item) const {
    auto const better_index = match_index(better_item);
    auto const worse_index = match_index(worse_item);
    if (better_index >= worse_index) {
      throw TestAssertionFailure(
          "expected '", better_item, "' (index ", better_index,
          ") to be ranked higher (have a lower index) than '", worse_item,
          "' (index ", worse_index, ")");
    }
  }
};

Matches match_and_log(std::initializer_list<boost::string_ref> items,
                      boost::string_ref const query) {
  Matches m;
  for_each_match<StringRefItem>(
      query, Options().set_want_match_info(true),
      source_from_range<StringRefItem>(begin(items), end(items)),
      [&](StringRefItem item, MatchInfo const* info) {
        std::printf("Matched %s (%s)\n", item.item().data(),
                    info->score_debug_string().c_str());
        m.matches.push_back(copy_string_ref(item.item()));
      });
  return m;
}

void test_match_order() {
  auto m = match_and_log({"barfoo", "fbar", "foo/bar", "foo/fbar", "foo/foobar",
                          "foo/foo_bar", "foo/foo_bar_test", "foo/foo_test_bar",
                          "foo/FooBar", "foo/abar", "foo/qux", "foob/ar"},
                         "fb");

  m.assert_not_matched("barfoo");
  m.assert_matched("fbar");
  m.assert_matched("foo/bar");
  m.assert_matched("foo/fbar");
  m.assert_matched("foo/foobar");
  m.assert_matched("foo/foo_bar");
  m.assert_matched("foo/foo_bar_test");
  m.assert_matched("foo/foo_test_bar");
  m.assert_matched("foo/FooBar");
  m.assert_matched("foo/abar");
  m.assert_not_matched("foo/qux");
  m.assert_matched("foob/ar");

  // "fbar" should rank highest due to the query being a full prefix.
  m.assert_match_index("fbar", 0);
  // "foo/fbar" should rank next highest due to the query being a full prefix,
  // but further away from cur_file (the empty string).
  m.assert_match_index("foo/fbar", 1);
  // "foo/foo_bar" and "foo/FooBar" should both rank next highest due to being
  // detectable word boundary matches, though it's unspecified which of the two
  // is higher.
  m.assert_better_match("foo/fbar", "foo/foo_bar");
  m.assert_better_match("foo/fbar", "foo/FooBar");
  // "foo/foo_bar_test" should rank below either of the above since there are
  // more trailing unmatched characters.
  m.assert_better_match("foo/foo_bar", "foo/foo_bar_test");
  m.assert_better_match("foo/FooBar", "foo/foo_bar_test");
  // "foo/foo_bar_test" should rank above "foo/foo_test_bar" since its matched
  // characters are in consecutive words.
  m.assert_better_match("foo/foo_bar_test", "foo/foo_test_bar");
  // "foo/bar" should rank below all of the above since it breaks the match
  // across multiple path components.
  m.assert_better_match("foo/foo_test_bar", "foo/bar");
  // "foo/foobar" should rank below all of the above since the 'b' is not a
  // detectable word boundary match.
  m.assert_better_match("foo/bar", "foo/foobar");
  // "foo/abar" and "foob/ar" should rank lowest since the matched 'b' isn't
  // even at the beginning of the filename in either case, though it's
  // unspecified which of the two is higher.
  m.assert_better_match("foo/bar", "foo/abar");
  m.assert_better_match("foo/bar", "foob/ar");
}

void test_special_paths() {
  auto m = match_and_log({"", "/", "a/", "/a"}, "a");

  m.assert_not_matched("");
  m.assert_not_matched("/");
  m.assert_matched("a/");
  m.assert_matched("/a");
}

template <typename F>
size_t run_test(F const& f) {
  try {
    std::printf("*** Test started\n");
    f();
    std::printf("*** Test passed\n");
    return 0;
  } catch (std::exception const& ex) {
    std::printf("*** Test failed: %s\n", ex.what());
    return 1;
  }
}

int run_all_tests() {
  size_t failed_tests = 0;
  failed_tests += run_test(test_match_order);
  failed_tests += run_test(test_special_paths);
  if (failed_tests == 0) {
    std::printf("*** All tests passed\n");
  } else {
    std::printf("*** %zu tests failed\n", failed_tests);
  }
  return failed_tests == 0 ? 0 : 1;
}

}  // namespace testing
}  // namespace cpsm

int main(int argc, char** argv) {
  return cpsm::testing::run_all_tests();
}
