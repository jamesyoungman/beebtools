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
}


int main() 
{
  const int rv = test_rtrim() ? 0 : 1;
  if (rv != 0) 
    {
      std::cerr << "TEST FAILURE\n";
    }
  return rv;
}
