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
#include "stringutil.h"

#include <algorithm>
#include <string>

namespace DFS
{

namespace stringutil
{

using std::string;

bool case_insensitive_less(const string& left,
			   const string& right)
{
  auto comp =
    [](const unsigned char lhs,
       const unsigned char rhs)
    {
      return tolower(lhs) == tolower(rhs);
    };
  const auto result = std::mismatch(left.cbegin(), left.cend(),
				    right.cbegin(), right.cend(),
				    comp);
  if (result.second == right.cend())
    {
      // Mismatch because right is at end-of-string.  Therefore either
      // the strings are the same length or right is shorter.  So left
      // cannot be less.
      return false;
    }
  if (result.first == left.cend())
    {
      // Cannot be equal as that was the previous case.
      return true;
    }
  return tolower(*result.first) < tolower(*result.second);
}

bool case_insensitive_equal(const string& left,
			    const string& right)
{
  return !case_insensitive_less(left, right) && !case_insensitive_less(right, left);
}

std::string rtrim(const std::string& input)
{
  const static std::string space(" ");
  auto endpos = input.find_last_not_of(space);
  if(string::npos != endpos)
    {
      return input.substr(0, endpos+1);
    }
  return "";
}

bool ends_with(const std::string & s, const std::string& suffix)
{
  if (suffix.size() > s.size())
    return false;
  return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
}

bool remove_suffix(std::string* s, const std::string& suffix)
{
  if (suffix.size() <= s->size() &&
      std::equal(suffix.rbegin(), suffix.rend(), s->rbegin()))
    {
      s->erase(s->size()-suffix.size(), suffix.size());
      return true;
    }
  return false;
}

std::deque<string> split(const std::string& s, char delim)
{
  std::deque<std::string> result;
  auto here = s.begin();
  while (here != s.end())
    {
      auto next = std::find(here, s.end(), delim);
      if (next == s.end())
	break;
      result.emplace_back(here, next);
      ++next;
      here = next;
    }
  result.emplace_back(here, s.end());
  return result;
}

}  // namespace stringutil

}  // namespace DFS
