//
//   Copyright 2020 James Youngman
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
#ifndef INC_DFS_H
#define INC_DFS_H 1

#include <stdexcept>
#include <exception>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>

#include "dfstypes.h"

namespace DFS {
  extern bool verbose;
  unsigned long sign_extend(unsigned long address);
  unsigned long compute_crc(const byte* start, const byte *end);
  const std::map<std::string, std::string>& get_option_help();

  template <typename T> T safe_unsigned_multiply(T a, T b)
  {
    // Ensure T is unsigned.
    static_assert(std::numeric_limits<T>::is_integer
		  && !std::numeric_limits<T>::is_signed);
    if (a == 0 || b == 0)
      return T(0);
    if (std::numeric_limits<T>::max() / a < b)
      throw std::range_error("overflow in safe_unsigned_multiply");
    return T(a * b);
  }


}  // namespace DFS

#endif
