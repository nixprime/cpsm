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

#ifndef CPSM_STR_UTIL_H_
#define CPSM_STR_UTIL_H_

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <boost/utility/string_ref.hpp>

#if CPSM_CONFIG_ICU
#include <unicode/uchar.h>
#endif

namespace cpsm {

inline void str_cat_impl(std::stringstream& ss) {}

template <typename T, typename... Args>
void str_cat_impl(std::stringstream& ss, T const& x, Args... args) {
  ss << x;
  str_cat_impl(ss, args...);
}

// Concatenates an arbitrary number of arguments that can be stringifed through
// a stringstream.
template <typename... Args>
std::string str_cat(Args... args) {
  std::stringstream ss;
  str_cat_impl(ss, args...);
  return ss.str();
}

// Splits a string into substrings separated by a delimiter.
std::vector<boost::string_ref> str_split(boost::string_ref str,
                                         char const delimiter);

// Joins an iterable over a type that can be stringified through a stringstream
// with the given separator.
template <typename T>
std::string str_join(T const& xs, boost::string_ref const sep) {
  std::stringstream ss;
  boost::string_ref s;
  for (auto const& x : xs) {
    ss << s << x;
    s = sep;
  }
  return ss.str();
}

// Exception type used by this package.
class Error : public std::exception {
 public:
  Error() : msg_("(unknown error)") {}

  template <typename... Args>
  explicit Error(Args... args)
      : msg_(str_cat(args...)) {}

  char const* what() const noexcept override { return msg_.c_str(); }

 private:
  std::string msg_;
};

// Returns a new `std::string` that is a copy of the data viewed by the given
// `boost::string_ref`.
inline std::string copy_string_ref(boost::string_ref const sref) {
  return std::string(sref.data(), sref.size());
}

// Constructs a copy of the range defined by the given iterators over a char[].
template <typename It>
boost::string_ref ref_str_iters(It first, It last) {
  return boost::string_ref(&*first, last - first);
}

// StringTraits type for paths that are 7-bit clean, which is the common case
// for source code.
struct SimpleStringTraits {
  typedef char Char;

  // For each character `c` in `str`, invokes `f(c, pos, len)` where `pos` is
  // the offset in bytes of the first byte corresponding to `c` in `str` and
  // `len` is its length in bytes.
  template <typename F>
  static void for_each_char(boost::string_ref const str, F const& f) {
    for (std::size_t i = 0, end = str.size(); i < end; i++) {
      f(str[i], i, 1);
    }
  }

  // Returns true if the given character represents a letter or number.
  static constexpr bool is_alphanumeric(Char const c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z');
  }

  // Returns true if the given character represents an uppercase letter.
  static constexpr bool is_uppercase(Char const c) {
    return c >= 'A' && c <= 'Z';
  }

  // Returns the lowercase version of the given uppercase letter.
  static constexpr Char uppercase_to_lowercase(Char const c) {
    return c + ('a' - 'A');
  }
};

template <typename StringTraits>
void decode_to(boost::string_ref const str,
               std::vector<typename StringTraits::Char>& chars) {
  chars.reserve(str.size());
  StringTraits::for_each_char(str, [&](typename StringTraits::Char c, int,
                                       int) { chars.push_back(c); });
}

template <typename StringTraits>
std::vector<typename StringTraits::Char> decode(boost::string_ref const str) {
  std::vector<typename StringTraits::Char> vec;
  decode_to<StringTraits>(str, vec);
  return vec;
}

#if CPSM_CONFIG_ICU

// StringTraits type for UTF-8-encoded strings. Non-UTF-8 bytes are decoded as
// the low surrogate 0xdc00+(byte) so that a match can still be attempted for
// malformed strings.
struct Utf8StringTraits {
  typedef char32_t Char;

  template <typename F>
  static void for_each_char(boost::string_ref str, F const& f) {
    std::size_t pos = 0;
    char32_t b0 = 0;
    // Even though most of this function deals with byte-sized quantities, use
    // char32_t throughout to avoid casting.
    auto const lookahead = [&](size_t n) -> char32_t {
      if (n >= str.size()) {
        return 0;
      }
      return str[n];
    };
    auto const decode_as = [&](char32_t c, std::size_t len) {
      f(c, pos, len);
      str.remove_prefix(len);
      pos += len;
    };
    auto const invalid = [&]() { decode_as(0xdc00 + b0, 1); };
    auto const is_continuation =
        [](char32_t b) -> bool { return (b & 0xc0) == 0x80; };
    while (!str.empty()) {
      auto const b0 = lookahead(0);
      if (b0 == 0x00) {
        // Input is a string_ref, not a null-terminated string - premature null?
        invalid();
      } else if (b0 < 0x80) {
        // 1-byte character
        decode_as(b0, 1);
      } else if (b0 < 0xc2) {
        // Continuation or overlong encoding
        invalid();
      } else if (b0 < 0xe0) {
        // 2-byte sequence
        auto const b1 = lookahead(1);
        if (!is_continuation(b1)) {
          invalid();
        } else {
          decode_as(((b0 & 0x1f) << 6) | (b1 & 0x3f), 2);
        }
      } else if (b0 < 0xf0) {
        // 3-byte sequence
        auto const b1 = lookahead(1), b2 = lookahead(2);
        if (!is_continuation(b1) || !is_continuation(b2)) {
          invalid();
        } else if (b0 == 0xe0 && b1 < 0xa0) {
          // Overlong encoding
          invalid();
        } else {
          decode_as(((b0 & 0x0f) << 12) | ((b1 & 0x3f) << 6) | (b2 & 0x3f), 3);
        }
      } else if (b0 < 0xf5) {
        // 4-byte sequence
        auto const b1 = lookahead(1), b2 = lookahead(2), b3 = lookahead(3);
        if (!is_continuation(b1) || !is_continuation(b2) ||
            !is_continuation(b3)) {
          invalid();
        } else if (b0 == 0xf0 && b1 < 0x90) {
          // Overlong encoding
          invalid();
        } else if (b0 == 0xf4 && b1 >= 0x90) {
          // > U+10FFFF
          invalid();
        } else {
          decode_as(((b0 & 0x07) << 18) | ((b1 & 0x3f) << 12) |
                        ((b2 & 0x3f) << 6) | (b3 & 0x3f),
                    4);
        }
      } else {
        // > U+10FFFF
        invalid();
      }
    }
  }

  static bool is_alphanumeric(Char const c) {
    return u_hasBinaryProperty(c, UCHAR_POSIX_ALNUM);
  }

  static bool is_uppercase(Char const c) {
    return u_hasBinaryProperty(c, UCHAR_UPPERCASE);
  }

  static Char uppercase_to_lowercase(Char const c) {
    return u_tolower(c);
  }
};

#else  // CPSM_CONFIG_ICU

struct Utf8StringTraits {
  typedef char32_t Char;

  [[noreturn]] static void unimplemented() {
    throw Error("cpsm built without Unicode support");
  }

  template <typename F>
  static void for_each_char(boost::string_ref str, F const& f) {
    unimplemented();
  }

  static bool is_alphanumeric(Char const c) {
    unimplemented();
  }

  static bool is_uppercase(Char const c) {
    unimplemented();
  }

  static Char uppercase_to_lowercase(Char const c) {
    unimplemented();
  }
};

#endif // CPSM_CONFIG_ICU

}  // namespace cpsm

#endif /* CPSM_STR_UTIL_H_ */
