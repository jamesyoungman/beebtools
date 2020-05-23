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

}  // namespace stringutil

}  // namespace DFS
