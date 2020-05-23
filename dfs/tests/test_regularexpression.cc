#include "regularexpression.h"

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

  void assert_same_matches(const vector<string>& left,
			   const vector<string>& right)
  {
    const string& intro = "supposed to be the same but ";
    if (left.size() != right.size())
      {
	std::cerr << intro << "they're different lengths:\n"
		  << left << "\nvs.\n" <<  right << "\n";
	abort();
      }
    auto bzzt = std::mismatch(left.begin(), left.end(), right.begin());
    if (bzzt.first != left.end())
      {
	std::cerr << intro << "these items are different: "
		  << *bzzt.first << " vs " << *bzzt.second << "\n";
	abort();
      }
  }
  void check_match(const string& pattern,
		   const string& input,
		   const vector<string>& matches)
  {
    RegularExpression r(pattern);
    assert(r.compile());
    auto result = r.match(input.c_str());
    assert_same_matches(result, matches);
  }

  bool test_match()
  {
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


int main()
{
  const int rv = self_test() ? 0 : 1;
  if (rv != 0)
    {
      std::cerr << "TEST FAILURE\n";
    }
  return rv;
}
