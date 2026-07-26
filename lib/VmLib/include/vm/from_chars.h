/*
 Copyright (C) 2025 Kristian Duske

 Permission is hereby granted, free of charge, to any person obtaining a copy of this
 software and associated documentation files (the "Software"), to deal in the Software
 without restriction, including without limitation the rights to use, copy, modify, merge,
 publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 persons to whom the Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 DEALINGS IN THE SOFTWARE.
*/

#pragma once

#if defined(__APPLE__)
#include "fast_float/fast_float.h"
#else
#endif

#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <concepts>

namespace vm
{

template <typename C, std::integral I>
constexpr auto from_chars(const C* first, const C* last, I& value, const int base = 10)
{
  return std::from_chars(first, last, value, base);
}

template <
  typename C,
  std::floating_point F,
  typename Fmt =
#if defined(__APPLE__)
    fast_float::chars_format
#else
    std::chars_format
#endif
  >
auto from_chars(
  const C* first, const C* last, F& value, const Fmt fmt = Fmt::general)
{
#if defined(__ANDROID__)
  static_cast<void>(fmt);
  const auto string = std::string{first, last};
  char* end = nullptr;
  errno = 0;
  if constexpr (std::is_same_v<F, float>)
  {
    value = std::strtof(string.c_str(), &end);
  }
  else
  {
    value = std::strtod(string.c_str(), &end);
  }
  const auto offset = end - string.c_str();
  const auto ec = end == string.c_str()      ? std::errc::invalid_argument
                  : errno == ERANGE         ? std::errc::result_out_of_range
                                            : std::errc{};
  return std::from_chars_result{first + offset, ec};
#elif defined(__APPLE__)
  return fast_float::from_chars(first, last, value, fmt);
#else
  return std::from_chars(first, last, value, fmt);
#endif
}

} // namespace vm