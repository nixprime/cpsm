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

#ifndef CPSM_STR_UTIL_H_
#define CPSM_STR_UTIL_H_

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/utility/string_ref.hpp>

namespace cpsm {

// The type to use for character, etc. counts. This is usually uint16_t because
// cpsm is mostly used to match paths, and path lengths are not capable of
// exceeding the range of 16 bits on most major operating systems:
// - Linux: PATH_MAX = 4096
// - Mac OS X: PATH_MAX = 1024
// - Windows: MAX_PATH = 260; Unicode interfaces may support paths of up to
//   32767 characters
typedef std::uint16_t CharCount;

// A type twice the size of CharCount.
typedef std::uint32_t CharCount2;

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

// Returns a new std::string that is a copy of the data viewed by the given
// boost::string_ref.
inline std::string copy_string_ref(boost::string_ref const sref) {
  return std::string(sref.data(), sref.size());
}

struct StringHandlerOpts {
  bool unicode = false;
};

class StringHandler final {
 public:
  explicit StringHandler(StringHandlerOpts opts = StringHandlerOpts());

  // If opts.unicode is false, appends each byte in the given string to the
  // given vector.
  //
  // If opts.unicode is true, attempts to parse the given string as UTF-8,
  // appending each code point in the string to the given vector. Non-UTF-8
  // bytes are appended to the given vector as the invalid code point
  // 0xdc00+(byte) so that a match can still be attempted.
  void decompose(boost::string_ref str, std::vector<char32_t>& chars) const;

  // Returns true if the given code point represents a letter or number.
  bool is_alphanumeric(char32_t c) const;

  // Returns true if the given code point represents an uppercase letter.
  bool is_uppercase(char32_t c) const;

  // Returns the lowercase version of c. c must be an uppercase letter.
  char32_t to_lowercase(char32_t c) const;

 private:
  StringHandlerOpts opts_;
};

}  // namespace cpsm

#endif /* CPSM_STR_UTIL_H_ */
