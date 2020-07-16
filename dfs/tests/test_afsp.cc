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
#include "afsp.h"

#include <assert.h>

#include <functional>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using std::string;
using std::vector;

using DFS::DFSContext;

namespace
{
  class BadTestInput : public std::exception
  {
  public:
    explicit BadTestInput(const string& msg)
      : msg_(msg)
    {
    }

    const char *what() const noexcept
    {
      return msg_.c_str();
    }

  private:
    std::string msg_;
  };

  struct MatchInput
  {
    DFS::VolumeSelector vol;
    char directory;
    std::string name;

    MatchInput(unsigned int d, char dir, const std::string& n)
      : vol(d), directory(dir), name(n)
    {
    }

    MatchInput(unsigned int d, char dir, const char* n)
      : vol(d), directory(dir), name(n)
    {
    }

    MatchInput(const std::string& s)
      : vol(9),
	directory('\0'),
	name("__unset__")
    {
      if (s[0] != ':')
	throw BadTestInput("missing leading colon: " + s);
      assert(s.size() >= 6);
      assert(s[0] == ':');
      assert(s.size() > 1);
      assert(isdigit(s[1]));
      // find the end of the drive number
      size_t i;
      for (i = 1; i < s.size(); ++i)
	{
	  if (!isdigit(s[i]))
	    break;
	}
      assert(i != s.size());
      size_t end;
      std::string err;
      auto parsed = DFS::VolumeSelector::parse(s.substr(1), &end, err);
      if (!parsed)
	throw BadTestInput(err);
      ++end; // account for substr(1).
      vol = *parsed;
      assert(s[end] == '.');	// drive number should be followed by "."
      ++end;
      assert(s.size() > end);
      directory = s[end];	// end points at directory
      ++end;			// end points at . after directory
      assert(s.size() > end);
      ++end;			// end points at file name
      assert(s.size() > end);
      name = s.substr(end);
    }

    MatchInput(const MatchInput& m)
      : vol(m.vol), directory(m.directory), name(m.name)
    {
    }

    bool operator<(const MatchInput& other) const
    {
      if (vol < other.vol)
	return true;
      if (directory < other.directory)
	return true;
      if (name < other.name)
	return true;
      return false;
    }

    bool operator==(const MatchInput& other) const
    {
      if (*this < other)
	return false;
      if (other < *this)
	return false;
      return true;
    }

    std::string str() const
    {
      std::ostringstream ss;
      ss << ":" << vol << "." << directory << "." << name;
      return ss.str();
    }

    MatchInput& operator=(const MatchInput&) = default;
  };
}  // namespace

namespace std
{
  ostream& operator<<(ostream& os, const MatchInput& m)
  {
    return os << m.str();
  }
}  // namespace std

namespace
{
  bool one_xfrm_test(DFS::VolumeSelector vol, char dir,
		     const string& transform_name,
		     std::function<bool(DFS::VolumeSelector, char, const string&, string*, string*)> transformer,
		     const string& input,
		     bool expected_return,
		     const string& expected_output,
		     const string& expected_error)
  {
    string actual_output, actual_error;
    bool actual_return = transformer(vol, dir, input, &actual_output, &actual_error);
    auto describe_call = [&]() -> string
			 {
			   std::ostringstream ss;
			   ss << transform_name << "(" << vol << ", '" << dir << "', "
			      <<  '"' << input << "\", &actual_output, &actual_error)";
			   return ss.str();
		};
    auto show_result = [&]()
		       {
			 if (actual_return)
			   {
			     std::cerr << "output was "
				       << actual_output << "\n";
			   }
			 else
			   {
			     if (actual_error.empty())
				 std::cerr << "no error message was returned\n";
			       else
				 std::cerr << "error message was "
					   << actual_error << "\n";
			   }
		       };
    if (actual_return != expected_return)
      {
	std::cerr << "test failure: " << describe_call()
		  << " return value; expected " << std::boolalpha
		  << expected_return << ", got " << actual_return << "\n";
	show_result();
	return false;
      }
    if (!expected_return && (expected_error != actual_error))
      {
	std::cerr << "test failure: " << describe_call()
		  << " error message; expected "
		  << expected_error << ", got " << actual_error << "\n";
	show_result();
	return false;
      }
    if (expected_return && (expected_output != actual_output))
      {
	std::cerr << "test failure: " << describe_call() << " result; expected \""
		  << expected_output << "\", got \"" << actual_output << "\"\n";
	show_result();
	return false;
      }
    return true;
  }

  bool one_wild_test(const DFSContext& ctx,
		     const string& wildcard,
		     bool expected_return,
		     const string& expected_output,
		     const string& expected_error)
  {
    return one_xfrm_test(ctx.current_volume, ctx.current_directory, "extend_wildcard",
			 DFS::internal::extend_wildcard, wildcard,
			 expected_return, expected_output, expected_error);
  }

  bool one_qualify_test(const DFSContext& ctx,
			const string& filename,
			bool expected_return,
			const string& expected_output,
			const string& expected_error)
  {
    return one_xfrm_test(ctx.current_volume, ctx.current_directory, "qualify",
			 DFS::internal::qualify, filename,
			 expected_return, expected_output, expected_error);
  }

  bool match_test(const DFSContext& ctx,
		  const string& pattern,
		  const vector<MatchInput>& inputs,
		  const vector<MatchInput>& expected_outputs)
  {
    string err;
    std::unique_ptr<DFS::AFSPMatcher> m = DFS::AFSPMatcher::make_unique(ctx, pattern, &err);
    if (!m)
      {
	std::cerr << "match_test: FAIL: pattern " << pattern << " is invalid: " << err << "\n";
	return false;
      }
    std::cerr << "match_test: pattern " << pattern << " is valid.\n";
    std::set<MatchInput> expected_accepts(expected_outputs.begin(), expected_outputs.end());
    std::set<MatchInput> actual_accepts;
    for (const MatchInput& input : inputs)
      {
	if (m->matches(input.vol, input.directory, input.name))
	  {
	    std::cerr << "match_test: " << pattern << " matches " << input << "\n";
	    actual_accepts.insert(input);
	  }
	else
	  {
	    std::cerr << "match_test: " << pattern << " does not match " << input << "\n";
	  }
      }
    for (const MatchInput& expected : expected_accepts)
      {
	if (actual_accepts.find(expected) == actual_accepts.end())
	  {
	    std::cerr << "match_test: FAIL: expected pattern " << pattern
		      << " to match " << expected << " but it did not\n";
	    return false;
	  }
      }
    for (const MatchInput& actual : actual_accepts)
      {
	if (expected_accepts.find(actual) == expected_accepts.end())
	  {
	    std::cerr << "match_test: FAIL: expected pattern " << pattern
		      << " not to match name " << actual
		      << " but it did\n";
	    return false;
	  }
      }

    assert (expected_accepts == actual_accepts);
    return true;
  }

  bool inputs_same(const MatchInput& m, const MatchInput& n)
  {
    if (m == n)
      return true;
    std::cerr << "inputs are not the same: " << m << " versus " << n << "\n";
    return false;
  }

  bool self_test_matcher()
  {
    const DFS::VolumeSelector vol0(DFS::SurfaceSelector(0));
    MatchInput m1(0, '$', "TEST");
    assert(m1.vol == vol0);
    assert(m1.directory == '$');
    assert(m1.name == "TEST");

    MatchInput m2(":0.$.TEST");
    assert(m2.vol == vol0);
    assert(m2.directory == '$');
    assert(m2.name == "TEST");

    assert(inputs_same(MatchInput(":0.$.TEST"),
		       MatchInput(0, '$', "TEST")));
    assert(inputs_same(MatchInput(":1.Q.V"),
		       MatchInput(1, 'Q', "V")));
    assert(inputs_same(MatchInput(":41.P.Z"),
		       MatchInput(41, 'P', "Z")));
    return true;
  }

  bool self_test()
  {
    DFSContext ctx;
    std::vector<bool> results;

    assert(self_test_matcher());

    auto record_test =
      [&results](bool result) {
	if (!result)
	  {
	    std::cerr << "FAIL: test " << results.size() << "\n";
	  }
	results.push_back(result);
      };

    // Positive cases.
    record_test(one_qualify_test(ctx,      "INPUT", true, ":0.$.INPUT", ""));
    record_test(one_qualify_test(ctx,    "$.INPUT", true, ":0.$.INPUT", ""));
    record_test(one_qualify_test(ctx, ":0.$.INPUT", true, ":0.$.INPUT", ""));
    record_test(one_qualify_test(ctx,   ":0.INPUT", true, ":0.$.INPUT", ""));
    record_test(one_qualify_test(ctx, "W.Welcome", true, ":0.W.Welcome", ""));
    record_test(one_qualify_test(ctx, ":2.&.WHAP", true, ":2.&.WHAP", ""));
    record_test(one_qualify_test(ctx,      ":0.$", true, ":0.$.$", ""));

    // Positive cases for high (> 3) drive numbers.
    record_test(one_qualify_test(ctx, ":4.&.WHAP", true, ":4.&.WHAP", ""));
    record_test(one_qualify_test(ctx, ":10.&.WHAP", true, ":10.&.WHAP", ""));

    // Checks for trailing blanks (which are present in the catalog but not part of
    // the file name).
    record_test(one_qualify_test(ctx, ":2.B.SPC   ", true, ":2.B.SPC", ""));

    // Invalid file names.
    const std::string not_valid_file("not a valid file name");
    record_test(one_qualify_test(ctx,   "",   false, "", not_valid_file));
    record_test(one_qualify_test(ctx, ":0",   false, "", not_valid_file));
    record_test(one_qualify_test(ctx, ":0.",  false, "", not_valid_file));
    // Metacharacters are not valid in file names.
    record_test(one_qualify_test(ctx, "#",   false, "",  not_valid_file));
    record_test(one_qualify_test(ctx, "*",   false, "",  not_valid_file));
    record_test(one_qualify_test(ctx, ":",   false, "",  not_valid_file));
    record_test(one_qualify_test(ctx, ".",   false, "",  not_valid_file));

    // Negative cases for bad drive numbers (they must be decimal, >= 0)
    // Drive numbers must be made of digits.
    record_test(one_qualify_test(ctx, ":Z.&.WHAP", false, "", not_valid_file));
    // They must be >= 0.
    record_test(one_qualify_test(ctx, ":-1.&.WHAP", false, "", not_valid_file));
    // They must not contain trailing non-digits in the same field.
    record_test(one_qualify_test(ctx, ":2Z.&.WHAP", false, "", not_valid_file));

    record_test(one_wild_test(ctx, ":0.$.*", true, ":0.$.*", ""));
    record_test(one_wild_test(ctx, ":0.$.NAME", true, ":0.$.NAME", ""));
    record_test(one_wild_test(ctx, "$.NAME", true, ":0.$.NAME", ""));
    record_test(one_wild_test(ctx, "#.*", true, ":0.#.*", ""));
    record_test(one_wild_test(ctx, "*.#", true, ":0.*.#", ""));
    record_test(one_wild_test(ctx, "#.##", true, ":0.#.##", ""));
    record_test(one_wild_test(ctx, "I.*", true, ":0.I.*", ""));

    // Drive numbers which are not valid in file names are also not
    // valid in wildcards.
    record_test(one_wild_test(ctx, ":Z.&.WHAP", false, "", "bad name"));
    record_test(one_wild_test(ctx, ":-1.&.WHAP", false, "", "bad name"));
    record_test(one_wild_test(ctx, ":2Z.&.WHAP", false, "", "bad name"));

    // Some of these expected results rely on the fact that the current
    // working directory ("cwd") is $.
    record_test(match_test(ctx, "Q.*", {}, {}));
    record_test(match_test(ctx, "Q.*", {{0,'Q', "FLUE"}}, {{0, 'Q', "FLUE"}}));
    record_test(match_test(ctx, "Q.*", {{0,'T',"BLUE"}}, {}));
    record_test(match_test(ctx, ":0.$.*", {{0, 'Q', "GLUE"}, {0, '$', "GLEAN"}}, {{0, '$', "GLEAN"}}));
    record_test(match_test(ctx,    "$.*", {{0, 'Q', "GRUE"}, {0, '$', "GREAT"}}, {{0, '$', "GREAT"}}));
    record_test(match_test(ctx,      "*", {{0, 'Q', "TRUE"}, {0, '$', "TREAD"}}, {{0, '$', "TREAD"}}));
    record_test(match_test(ctx, "*",
			   {
			    {0, 'Q', "TRUG"}, // no match: * only matches in cwd
			    {1, '$', "TREAD"}, // should not be matched because wrong drive
			   }, {/* no mathces */}));

    // Tests that verify drive number handling.  cwd is $, drive is 0.
    record_test(match_test(ctx, ":0.Q.*", {{0, 'Q', "BLUE"}}, {{0, 'Q', "BLUE"}}));
    record_test(match_test(ctx, ":1.Q.*", {{0, 'T', "BLUE"}}, {}));
    record_test(match_test(ctx, ":1.Q.*", {{0, 'Q', "BLUE"}}, {}));

    record_test(match_test(ctx, ":1.Q.*", {{1, 'Q', "BLUE"}}, {{1, 'Q', "BLUE"}}));

    record_test(match_test(ctx, ":0.Q.*", {{2, 'Q', "BLUE"}, {0, 'Q', "BLUE"}}, {{0, 'Q', "BLUE"}}));
    record_test(match_test(ctx, ":2.Q.*", {{2, 'Q', "BLUE"}, {0, 'Q', "BLUE"}}, {{2, 'Q', "BLUE"}}));

#ifdef LARGE_DRIVE_NUMBERS
    // These tests verify that we can handle drive numbers > 3.
    record_test(match_test(ctx, ":1.Q.*",  {{1, 'Q', "BLUE"},  {12, 'Q', "BLUE"}}, {{1, 'Q', "BLUE"}}));
    record_test(match_test(ctx, ":12.Q.*", {{1, 'Q', "BLUE"},  {2, 'Q', "BLUE"}}, {}));
    record_test(match_test(ctx, ":41.Q.*", {{41, 'Q', "BLUE"}, {1, 'Q', "BLUE"}}, {{41, 'Q', "BLUE"}}));
#endif

    // Tests that verify that matches are case-folded.
    record_test(match_test(ctx, "P*",
			   {{0, '$', "Price"}, {0, '$', "price"}},
			   {{0, '$', "Price"}, {0, '$', "price"}}));
    record_test(match_test(ctx, "P.*",
			   {{0, 'P', "Trice"}, {0, 'p', "Trice"}, {0, '$', "Trice"}},
			   {{0, 'P', "Trice"}, {0, 'p', "Trice"}}));

    return std::find(results.cbegin(), results.cend(), false) == results.cend();
  }
}


int main()
{
  int rv = 1;
  try
    {
      if (self_test())
	rv = 0;
    }
  catch (BadTestInput& e)
    {
      std::cerr << "Unit test input was bad: " << e.what() << "\n";
      rv = 2;
    }
  if (rv != 0)
    {
      std::cerr << "TEST FAILURE\n";
    }
  return rv;
}
