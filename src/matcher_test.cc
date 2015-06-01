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

#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

#include "match.h"
#include "matcher.h"
#include "str_util.h"

namespace cpsm {

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

void test_match_order() {
  std::vector<std::string> items({
      "barfoo", "fbar", "foo/bar", "foo/fbar", "foo/foobar", "foo/foo_bar",
      "foo/foo_bar_test", "foo/FooBar", "foo/abar", "foo/qux",
  });

  Matcher matcher("fb");
  std::vector<Match<std::string>> match_objs;
  std::vector<std::string> matches;
  for (auto const& item : items) {
    Match<std::string> match(item);
    if (matcher.match(item, match)) {
      match_objs.emplace_back(std::move(match));
    }
  }
  sort_limit(match_objs);
  std::printf("matches:");
  for (auto& m : match_objs) {
    std::printf(" %s", m.item.c_str());
    matches.emplace_back(std::move(m.item));
   }
   std::printf("\n");

  auto const match_it =
      [&](boost::string_ref const item) -> std::vector<std::string>::iterator {
        return std::find_if(
            matches.begin(), matches.end(),
            [item](std::string const& match) -> bool { return item == match; });
      };
  auto const matched = [&](boost::string_ref const item)
                           -> bool { return match_it(item) != matches.end(); };
  auto const assert_matched = [&](boost::string_ref const item) {
    if (!matched(item)) {
      throw TestAssertionFailure("incorrectly matched '", item, "'");
    }
  };
  auto const assert_not_matched = [&](boost::string_ref const item) {
    if (matched(item)) {
      throw TestAssertionFailure("incorrectly failed to match '", item, "'");
    }
  };
  assert_not_matched("barfoo");
  assert_matched("fbar");
  assert_matched("foo/bar");
  assert_matched("foo/fbar");
  assert_matched("foo/foobar");
  assert_matched("foo/foo_bar");
  assert_matched("foo/foo_bar_test");
  assert_matched("foo/FooBar");
  assert_matched("foo/abar");
  assert_not_matched("foo/qux");

  auto const match_index = [&](boost::string_ref const item) -> std::size_t {
    return match_it(item) - matches.begin();
  };
  auto const assert_match_index =
      [&](boost::string_ref const item, std::size_t expected_index) {
        auto const index = match_index(item);
        if (index != expected_index) {
          throw TestAssertionFailure("expected '", item, "' (index ", index,
                                     ") to have index ", expected_index);
        }
      };
  auto const assert_better_match = [&](boost::string_ref const better_item,
                                       boost::string_ref const worse_item) {
    auto const better_index = match_index(better_item);
    auto const worse_index = match_index(worse_item);
    if (better_index >= worse_index) {
      throw TestAssertionFailure(
          "expected '", better_item, "' (index ", better_index,
          ") to be ranked higher (have a lower index) than '", worse_item,
          "' (index ", worse_index, ")");
    }
  };
  // "fbar" should rank highest due to the query being a full prefix.
  assert_match_index("fbar", 0);
  // "foo/fbar" should rank next highest due to the query being a full prefix,
  // but further away from cur_file (the empty string).
  assert_match_index("foo/fbar", 1);
  // "foo/foo_bar" and "foo/FooBar" should both rank next highest due to being
  // detectable word boundary matches, though it's unspecified which of the two
  // is higher.
  assert_better_match("foo/fbar", "foo/foo_bar");
  assert_better_match("foo/fbar", "foo/FooBar");
  // "foo/foobar" should rank below either of the above since the 'b' is not a
  // detectable word boundary match.
  assert_better_match("foo/foo_bar", "foo/foobar");
  assert_better_match("foo/FooBar", "foo/foobar");
  // "foo/foo_bar_test" should rank below "foo/foo_bar" since there are more
  // trailing unmatched characters, although it's unspecified whether it should
  // be higher or lower than "foo/foobar".
  assert_better_match("foo/foo_bar", "foo/foo_bar_test");
  // "foo/bar" should rank lower than any of the above despite matching closer
  // to the left side of the item since it breaks the match across multiple
  // path components.
  assert_better_match("foo/foo_bar_test", "foo/bar");
  assert_better_match("foo/foobar", "foo/bar");
  // "foo/abar" should rank lowest since the matched 'b' isn't even at the
  // beginning of the filename.
  assert_better_match("foo/bar", "foo/abar");
}

}  // namespace cpsm

int main(int argc, char** argv) {
  try {
    cpsm::test_match_order();
    std::printf("PASS\n");
    return 0;
  } catch (std::exception const& ex) {
    std::fprintf(stderr, "FAIL: %s\n", ex.what());
    return 1;
  }
}
