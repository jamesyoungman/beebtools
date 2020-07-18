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
#ifndef INC_STRINGUTIL_H
#define INC_STRINGUTIL_H 1

#include <deque>       // for deque
#include <string>      // for string

#include "dfstypes.h"  // for byte

namespace DFS {

namespace stringutil
{

bool case_insensitive_less(const std::string& left, const std::string& right);
bool case_insensitive_equal(const std::string& left, const std::string& right);
std::string rtrim(const std::string& input);
bool ends_with(const std::string& s, const std::string& suffix);
bool remove_suffix(std::string* s, const std::string& suffix);
std::deque<std::string> split(const std::string& s, char delim);

inline char byte_to_ascii7(DFS::byte b)
{
  return char(b & 0x7F);
}


}  // namespace stringutil

}  // namespace DFS

#endif
