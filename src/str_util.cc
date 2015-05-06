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

#if CPSM_CONFIG_ICU
#include <unicode/uchar.h>
#endif

#include "str_util.h"

namespace cpsm {

void decompose_utf8_string(boost::string_ref str,
                           std::vector<char32_t>& chars) {
  // Even though most of this function deals with byte-sized quantities, use
  // char32_t throughout to avoid casting.
  auto const lookahead = [&](size_t n) -> char32_t {
    if (n >= str.size()) {
      return 0;
    }
    return str[n];
  };
  auto const invalid = [&](char32_t b) {
    chars.push_back(0xdc00 + b);
    str.remove_prefix(1);
  };
  auto const is_continuation = [](char32_t b) -> bool {
    return (b & 0xc0) == 0x80;
  };
  while (!str.empty()) {
    auto const b0 = lookahead(0);
    if (b0 == 0x00) {
      // Input is a string_ref, not a null-terminated string - premature null?
      invalid(b0);
    } else if (b0 < 0x80) {
      // 1-byte character
      chars.push_back(b0);
      str.remove_prefix(1);
    } else if (b0 < 0xc2) {
      // Continuation or overlong encoding
      invalid(b0);
    } else if (b0 < 0xe0) {
      // 2-byte sequence
      auto const b1 = lookahead(1);
      if (!is_continuation(b1)) {
        invalid(b0);
      } else {
        chars.push_back(((b0 & 0x1f) << 6) | (b1 & 0x3f));
        str.remove_prefix(2);
      }
    } else if (b0 < 0xf0) {
      // 3-byte sequence
      auto const b1 = lookahead(1), b2 = lookahead(2);
      if (!is_continuation(b1) || !is_continuation(b2)) {
        invalid(b0);
      } else if (b0 == 0xe0 && b1 < 0xa0) {
        // Overlong encoding
        invalid(b0);
      } else {
        chars.push_back(((b0 & 0x0f) << 12) | ((b1 & 0x3f) << 6) | (b2 & 0x3f));
        str.remove_prefix(3);
      }
    } else if (b0 < 0xf5) {
      // 4-byte sequence
      auto const b1 = lookahead(1), b2 = lookahead(2), b3 = lookahead(3);
      if (!is_continuation(b1) || !is_continuation(b2) ||
          !is_continuation(b3)) {
        invalid(b0);
      } else if (b0 == 0xf0 && b1 < 0x90) {
        // Overlong encoding
        invalid(b0);
      } else if (b0 == 0xf4 && b1 >= 0x90) {
        // > U+10FFFF
        invalid(b0);
      } else {
        chars.push_back(((b0 & 0x07) << 18) | ((b1 & 0x3f) << 12) |
                        ((b2 & 0x3f) << 6) | (b3 & 0x3f));
        str.remove_prefix(4);
      }
    } else {
      // > U+10FFFF
      invalid(b0);
    }
  }
}

#if CPSM_CONFIG_ICU

bool is_alphanumeric(char32_t const c) {
  return u_hasBinaryProperty(c, UCHAR_POSIX_ALNUM);
}

bool is_uppercase(char32_t const c) {
  return u_hasBinaryProperty(c, UCHAR_UPPERCASE);
}

char32_t to_lowercase(char32_t const c) {
  return u_tolower(c);
}

#else // CPSM_CONFIG_ICU

bool is_alphanumeric(char32_t const c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z');
}

bool is_uppercase(char32_t const c) {
  return c >= 'A' && c <= 'Z';
}

char32_t to_lowercase(char32_t c) {
  return c + ('a' - 'A');
}

#endif // CPSM_CONFIG_ICU

} // namespace cpsm
