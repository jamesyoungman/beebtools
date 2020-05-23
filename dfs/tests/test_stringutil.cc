#include "stringutil.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

using std::cerr;
using std::make_pair;
using std::string;
using std::vector;

using DFS::stringutil::rtrim;
using DFS::stringutil::case_insensitive_less;
using DFS::stringutil::case_insensitive_equal;

namespace
{

  bool test_rtrim()
  {
    bool outcome = true;
    const vector<std::pair<string, string>> cases =
      {
       make_pair("hello", "hello"),
       make_pair("", ""),
       make_pair("  hello", "  hello"),
       make_pair("hello ", "hello"),
       make_pair(" hello ", " hello"),
       make_pair("hello  ", "hello"),
       make_pair("hello\t  ", "hello\t"),
       make_pair("hello \t  ", "hello \t"),
      };
    for (const auto& testcase : cases)
      {
	string result = rtrim(testcase.first);
	if (result != testcase.second)
	  {
	    result = false;
	    std::cerr << "Expected '" << testcase.first << "' to rtrim to '"
		      << testcase.second << "' but got '" << result << "'\n";
	    outcome = false;
	  }
      }
    return outcome;
  }

  enum class Comparison
    {
     LeftIsSmaller = -1,
     Equal = 0,
     RightIsSmaller = +1
    };
  struct comparison_case
  {
    std::string left;
    std::string right;
    Comparison expected;
  };
  const vector<comparison_case> case_insensitive_comparisons =
    {
     /* Empty string is less than everything except itself */
     {"", "", Comparison::Equal},
     {"", "0", Comparison::LeftIsSmaller},
     {"0", "", Comparison::RightIsSmaller},
     // Lower case examples
     {"a", "b", Comparison::LeftIsSmaller},
     {"a", "a", Comparison::Equal},
     {"b", "a", Comparison::RightIsSmaller},
     {"aa", "b", Comparison::LeftIsSmaller},
     {"a", "bb", Comparison::LeftIsSmaller},
     // Upper case examples
     {"A", "B", Comparison::LeftIsSmaller},
     {"A", "A", Comparison::Equal},
     {"B", "A", Comparison::RightIsSmaller},
     {"AA", "B", Comparison::LeftIsSmaller},
     {"A", "BB", Comparison::LeftIsSmaller},
     // Mixed case examples
     {"A", "b", Comparison::LeftIsSmaller},
     {"A", "a", Comparison::Equal},
     {"a", "A", Comparison::Equal},
     {"a", "B", Comparison::LeftIsSmaller},
     {"B", "a", Comparison::RightIsSmaller},
     {"b", "A", Comparison::RightIsSmaller},
     {"AA", "b", Comparison::LeftIsSmaller},
     {"aa", "B", Comparison::LeftIsSmaller},
     {"A", "bb", Comparison::LeftIsSmaller},
     {"a", "BB", Comparison::LeftIsSmaller},
     // Longer examples
     {"womble", "Womble", Comparison::Equal},
     {"womble", "wombles", Comparison::LeftIsSmaller},
     {"womble", "Wombles", Comparison::LeftIsSmaller},
     {"Womble", "wombles", Comparison::LeftIsSmaller},
     {"Wombles", "womble", Comparison::RightIsSmaller},
     // Trickier examples. For two strings of which left is a prefix
     // of right, the right is greater.  This is illustrated in the
     // womble example above.  But this is also true where te suffix
     // is just nulls.
     {std::string(), std::string(1, '\0'), Comparison::LeftIsSmaller},
     {std::string(1, '\0'), std::string(), Comparison::RightIsSmaller},
     {std::string(1, '\0'), std::string(2, '\0'), Comparison::LeftIsSmaller},
    };

  bool test_case_insensitive_less()
  {
    bool outcome = true;
    for (const auto& testcase : case_insensitive_comparisons)
      {
	bool expected = (testcase.expected == Comparison::LeftIsSmaller) ? true : false;
	bool got = case_insensitive_less(testcase.left, testcase.right);
	if (expected != got)
	  {
	    outcome = false;
	    std::cerr << std::boolalpha
		      << "Expected case_insensitive_less(\""
		      << testcase.left << "\", \"" << testcase.right << "\")"
		      << " to return " << expected << " but it returned "
		      << got << "\n";
	    outcome = false;
	  }
      }
    return outcome;
  }

  bool test_case_insensitive_equal()
  {
    bool outcome = true;
    for (const auto& testcase : case_insensitive_comparisons)
      {
	bool expected = (testcase.expected == Comparison::Equal) ? true : false;
	bool got = case_insensitive_equal(testcase.left, testcase.right);
	if (expected != got)
	  {
	    outcome = false;
	    std::cerr << std::boolalpha
		      << "Expected case_insensitive_equal(\""
		      << testcase.left << "\", \"" << testcase.right << "\")"
		      << " to return " << expected << " but it returned "
		      << got << "\n";
	    outcome = false;
	  }
      }
    return outcome;
  }
}


int main()
{
  int rv = 0;
  if (!test_rtrim()) rv = 1;
  if (!test_case_insensitive_less()) rv = 1;
  if (!test_case_insensitive_equal()) rv = 1;
  if (rv != 0)
    {
      std::cerr << "TEST FAILURE\n";
    }
  return rv;
}
