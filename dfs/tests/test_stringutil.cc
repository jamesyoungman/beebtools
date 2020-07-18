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
#include <algorithm>     // for mismatch
#include <deque>         // for deque<>::const_iterator, deque, operator!=
#include <iostream>      // for operator<<, basic_ostream, ostream, cerr
#include <string>        // for string, allocator, operator<<, basic_string
#include <type_traits>   // for __decay_and_strip<>::__type
#include <utility>       // for make_pair, pair
#include <vector>        // for vector, vector<>::const_iterator

#include "stringutil.h"  // for case_insensitive_equal, case_insensitive_less

using std::cerr;
using std::make_pair;
using std::string;
using std::vector;

using DFS::stringutil::rtrim;
using DFS::stringutil::case_insensitive_less;
using DFS::stringutil::case_insensitive_equal;
using DFS::stringutil::split;


namespace std
{
  bool same(const deque<string>& left,
	    const vector<string>& right)
  {
    if (left.size() != right.size())
      return false;
    auto diff = std::mismatch(left.cbegin(), left.cend(),
			      right.cbegin());
    return diff.first == left.cend();
  }

  template <class It> void print(ostream& os, It begin, It end)
  {
    bool first = true;
    os << '{';
    while (begin != end)
      {
	if (first)
	  first = false;
	else
	  os << ',';
	os << '"' << *begin++ << '"';
      }
    os << '}';
  }
}

namespace
{
  bool one_split_test(const std::string& input,
		      char delim,
		      const std::vector<std::string>& expected)
  {
    const std::deque<std::string> actual = split(input, delim);
    if (same(actual, expected))
      return true;
    std::cerr << "split_test: split(\"" << input << "\",'" << delim << "') -> ";
    print(std::cerr, actual.begin(), actual.end());
    std::cerr << " but expected ";
    print(std::cerr, expected.begin(), expected.end());
    std::cerr << "\n";
    return false;
  }

  bool test_split()
  {
    struct test_case
    {
      test_case(int n, const std::string& s,
		char del,
		const std::vector<std::string>& want)
	: id(n), input(s), delim(del), expected(want)
      {
      }
      const int id;
      const std::string input;
      const char delim;
      const std::vector<std::string> expected;
    };
    const vector<test_case> cases =
      {
       test_case(10, "aa", '.', {"aa"}),
       test_case(20, "aa.bb", '.', {"aa", "bb"}),
       test_case(30, "aa.bb", 's', {"aa.bb"}),
       test_case(40, ".aa", '.', {"", "aa"}),
       test_case(50, "", '.', {""}),
       test_case(60, "aa.", '.', {"aa", ""}),
       test_case(70, ".", '.', {"", ""}),
       test_case(80, "foo.ssd", '.', {"foo", "ssd"}),
       test_case(90, "foo.ssd.gz", '.', {"foo", "ssd", "gz"}),
       test_case(95, "foo", '.', {"foo"}),
      };

    bool ok = true;
    for (const auto& testcase : cases)
      {
	if (!one_split_test(testcase.input, testcase.delim, testcase.expected))
	  {
	    std::cerr << "one_split_test: case " << testcase.id << " failed\n";
	    ok = false;
	  }
      }
    return ok;
  }

  bool remove_suffix_test(const std::string& input,
			  const std::string& suffix,
			  bool expected_return,
			  const std::string& expected_yield)
  {
    std::string work = input;
    bool removed = DFS::stringutil::remove_suffix(&work, suffix);
    if (removed != expected_return)
      {
	std::cerr << std::boolalpha
		  << "expected remove_suffix(\"" << input << "\",\""
		  << suffix << "\") to return " << expected_return
		  << " but it returned " << removed;
	return false;
      }

    if (work != expected_yield)
      {
	std::cerr << "expected remove_suffix(\"" << input << "\",\""
		  << suffix << "\") to yield " << expected_yield
		  << " but it yielded " << work;
	return false;
      }
    return true;
  }

  bool test_remove_suffix()
  {
    struct test_case
    {
      std::string input;
      std::string suffix;
      bool expected_return;
      std::string expected_yield;
    };
    const vector<test_case> cases =
      {
       {"x", "x", true, "" },
       {"a", "", true, "a" },
       {"", "b", false, "" },
       {"foo.ssd", ".ssh", false, "foo.ssd"},
       {"foo.ssd.gz", ".ssh", false, "foo.ssd.gz"},
       {"foo.ssd.gz", ".gz", true, "foo.ssd"},
       {"foo.ssd.gz", ".ss.gz", false, "foo.ssd.gz"},
      };
    for (const auto& testcase : cases)
      {
	if (!remove_suffix_test(testcase.input, testcase.suffix,
				testcase.expected_return, testcase.expected_yield))
	  return false;
      }
    return true;
  }

  bool endswith_test(const std::string& s,
		     const std::string& suffix,
		     bool expected)
  {
    const bool actual = DFS::stringutil::ends_with(s, suffix);
    if (actual != expected)
      {
	std::cerr << std::boolalpha
		  << "expected ends_with(\"" << s << "\",\""
		  << suffix << "\") to return " << expected
		  << " but it returned " << actual;
      }
    return actual == expected;
  }

  bool test_endswith()
  {
    struct test_case
    {
      std::string input;
      std::string suffix;
      bool expected;
    };
    const vector<test_case> cases =
      {
       {"x", "x", true },
       {"a", "", true },
       {"", "b", false },
       {"foo.ssd", ".ssh", false},
       {"foo.ssd.gz", ".ssh", false},
       {"foo.ssd.gz", ".gz", true},
       {"foo.ssd.gz", ".ssd.gz", true},
       {"foo.ssd.gz", ".ss.gz", false},
      };
    for (const auto& testcase : cases)
      {
	if (!endswith_test(testcase.input, testcase.suffix, testcase.expected))
	  return false;
      }
    return true;
  }

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
  if (!test_split()) rv = 1;
  if (!test_endswith()) rv = 1;
  if (!test_remove_suffix()) rv = 1;
  if (!test_rtrim()) rv = 1;
  if (!test_case_insensitive_less()) rv = 1;
  if (!test_case_insensitive_equal()) rv = 1;
  if (rv != 0)
    {
      std::cerr << "TEST FAILURE\n";
    }
  return rv;
}
