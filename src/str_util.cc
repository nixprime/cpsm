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

#include "str_util.h"

#if CPSM_CONFIG_ICU
#include <unicode/uchar.h>
#endif

#include <utility>

namespace cpsm {

namespace {

#if CPSM_CONFIG_ICU

void decode_utf8_string(boost::string_ref str, std::vector<char32_t>& chars,
                        std::vector<CharCount>* const char_positions) {
  CharCount pos = 0;
  char32_t b0 = 0;
  // Even though most of this function deals with byte-sized quantities, use
  // char32_t throughout to avoid casting.
  auto const lookahead = [&](size_t n) -> char32_t {
    if (n >= str.size()) {
      return 0;
    }
    return str[n];
  };
  auto const decode_as = [&](char32_t c, CharCount len) {
    chars.push_back(c);
    str.remove_prefix(len);
    if (char_positions) {
      char_positions->push_back(pos);
      pos += len;
    }
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

#endif /* CPSM_CONFIG_ICU */

}  // namespace

std::vector<boost::string_ref> str_split(boost::string_ref str,
                                         char const delimiter) {
  std::vector<boost::string_ref> splits;
  while (true) {
    auto const dpos = str.find_first_of(delimiter);
    if (dpos == boost::string_ref::npos) {
      break;
    }
    splits.push_back(str.substr(0, dpos));
    str.remove_prefix(dpos+1);
  }
  splits.push_back(str);
  return splits;
}


StringHandler::StringHandler(StringHandlerOpts opts) : opts_(std::move(opts)) {
#if !CPSM_CONFIG_ICU
  if (opts_.unicode) {
    throw Error("cpsm built without Unicode support");
  }
#endif
}

void StringHandler::decode(boost::string_ref const str,
                           std::vector<char32_t>& chars,
                           std::vector<CharCount>* const char_positions) const {
#if CPSM_CONFIG_ICU
  if (opts_.unicode) {
    decode_utf8_string(str, chars, char_positions);
    return;
  }
#endif
  chars.reserve(str.size());
  CharCount pos = 0;
  for (char const c : str) {
    chars.push_back(c);
    if (char_positions) {
      char_positions->push_back(pos);
      pos++;
    }
  }
}

bool StringHandler::is_alphanumeric(char32_t const c) const {
#if CPSM_CONFIG_ICU
  if (opts_.unicode) {
    return u_hasBinaryProperty(c, UCHAR_POSIX_ALNUM);
  }
#endif
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z');
}

bool StringHandler::is_uppercase(char32_t const c) const {
#if CPSM_CONFIG_ICU
  if (opts_.unicode) {
    return u_hasBinaryProperty(c, UCHAR_UPPERCASE);
  }
#endif
  return c >= 'A' && c <= 'Z';
}

char32_t StringHandler::to_lowercase(char32_t const c) const {
#if CPSM_CONFIG_ICU
  if (opts_.unicode) {
    return u_tolower(c);
  }
#endif
  return c + ('a' - 'A');
}

}  // namespace cpsm
