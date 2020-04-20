#include "fsp.h"

using std::string;

std::pair<char, std::string> directory_and_name_of(const DFSContext& ctx,
						   const std::string& filename) 
{
  char dir = ctx.current_directory;
  string::size_type pos = 0;
  if (filename.size() > 1 && filename[1] == '.') 
    {
      dir = filename[0];
      pos = 2;
    }
  return std::make_pair(dir, filename.substr(pos));
}

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
      // Equal, so left cannot be less.
      return false;
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

