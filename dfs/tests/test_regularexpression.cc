#include "regularexpression.h"

// The whole point of this program is to test assertsions,
// so enable assertsions in all curcumstances.
#undef NDEBUG
#include <assert.h>

#include <iostream>

using DFS::RegularExpression;
using std::string;
using std::vector;

namespace std
{
  ostream& operator<<(ostream& os, const vector<string>& v)
  {
    bool first = true;
    os << "{";
    for (const string& s : v)
      {
	if (!first)
	  os << ", ";
	else
	  first = false;
	os << s;
      }
    os << "}";
    return os;
  }
}


namespace
{
  void assert_is_valid(const string& pattern)
  {
    DFS::RegularExpression r(pattern);
    if (!r.compile())
      {
	std::cerr << "regular expression " << pattern << " is not valid: "
		  << r.error_message() << "\n";
	abort();
      }
  }

  void assert_is_invalid(const string& pattern)
  {
    RegularExpression r(pattern);
    if (r.compile())
      {
	std::cerr << "regular expression " << pattern
		  << " is (unexpectedly) valid\n";
	abort();
      }
  }
  bool test_invalid_regexes()
  {
    assert_is_valid("");
    assert_is_valid("x");
    assert_is_valid(".");
    assert_is_valid("[[:punct:]]");
    assert_is_invalid("\\1");
    assert_is_invalid("x{1");
    assert_is_invalid("[[:Funct:]]");
    // ")" is only valid without a preceding unmatched "(" due to an
    // accident of wording of the POSIX spec and the committee
    // suggests not relying on it.
    return true;
  }

  void assert_same_matches(const string& pattern,
			   const string& input,
			   const vector<string>& expected_matches,
			   const vector<string>& got)
  {
    std::cerr << "Pattern " << pattern << ", input " << input << ": ";
    const string& intro = "matches are supposed to be the same but ";
    if (expected_matches.size() != got.size())
      {
	std::cerr << intro << "they're different lengths, expected "
		  << expected_matches.size() << ":\n"
		  << expected_matches << "\ngot " << got.size()
		  << ":\n" <<  got << "\n";
	abort();
      }
    auto bzzt = std::mismatch(expected_matches.begin(), expected_matches.end(), got.begin());
    if (bzzt.first != expected_matches.end())
      {
	std::cerr << intro << "these items are different: expected "
		  << *bzzt.first << " but got " << *bzzt.second << "\n";
	abort();
      }
    std::cerr << "ok\n";
  }
  void check_match(const string& pattern,
		   const string& input,
		   const vector<string>& expected_matches)
  {
    RegularExpression r(pattern);
    assert(r.compile());
    auto got = r.match(input.c_str());
    assert_same_matches(pattern, input, expected_matches, got);
  }

  bool test_match()
  {
    check_match("m", "mellow yellow ",
		vector<string>({"m"}));
    check_match("^.ell", "mellow yellow ",
		vector<string>({"mell"}));
    check_match("(.ell)", "mellow yellow ",
		vector<string>({"mell", "mell"}));
    check_match("(.*)=(.*)[?]", "apples=oranges?",
		vector<string>({"apples=oranges?", "apples", "oranges"}));
    return true;
  }

  bool self_test()
  {
    return test_invalid_regexes() && test_match();
  }
}

bool raises_exception()
{
  throw 1;
}

void check_assertions_do_things()
{
  try
    {
      assert(raises_exception());
      std::cerr << "FAIL: assertions are turned off, this test cannot work\n";
      exit(1);
    }
  catch (int x)
    {
      // All is well.
      return;
    }
}

int main()
{
  check_assertions_do_things();
  const int rv = self_test() ? 0 : 1;
  if (rv != 0)
    {
      std::cerr << "TEST FAILURE\n";
    }
  return rv;
}
