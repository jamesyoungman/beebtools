#include "afsp.h"

#include <assert.h>

#include <functional>
#include <iostream>
#include <set>
#include <vector>

using std::string;
using std::vector;

using DFS::DFSContext;

namespace 
{
  bool one_xfrm_test(const DFSContext& ctx,
		     const string& transform_name,
		     std::function<bool(const DFSContext&, const string&, string*, string*)> transformer,
		     const string& input,
		     bool expected_return,
		     const string& expected_output,
		     const string& expected_error)
  {
    string actual_output, actual_error;
    bool actual_return = transformer(ctx, input, &actual_output, &actual_error);
    auto describe_call = [&]() -> string
			 {
			   return transform_name + "(ctx, \"" + input +
			     "\", &actual_output, &actual_error)";
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
    return one_xfrm_test(ctx, "extend_wildcard", DFS::internal::extend_wildcard, wildcard,
			 expected_return, expected_output, expected_error);
  }

  bool one_qualify_test(const DFSContext& ctx,
			const string& filename,
			bool expected_return,
			const string& expected_output,
			const string& expected_error)
  {
    return one_xfrm_test(ctx, "qualify", DFS::internal::qualify, filename,
			 expected_return, expected_output, expected_error);
  }

  bool match_test(const DFSContext& ctx,
		  const string& pattern,
		  const vector<string>& inputs,
		  const vector<string>& expected_outputs)
  {
    string err;
    std::unique_ptr<DFS::AFSPMatcher> m = DFS::AFSPMatcher::make_unique(ctx, pattern, &err);
    if (!m)
      {
	std::cerr << "match_test: FAIL: pattern " << pattern << " is invalid: " << err << "\n";
	return false;
      }
    std::cerr << "match_test: pattern " << pattern << " is valid.\n";
    std::set<string> expected_accepts(expected_outputs.begin(), expected_outputs.end());
    std::set<string> actual_accepts;
    for (const string& name : inputs)
      {
	if (m->matches(name))
	  {
	    std::cerr << "match_test: " << pattern << " matches " << name << "\n";
	    actual_accepts.insert(name);
	  }
	else
	  {
	    std::cerr << "match_test: " << pattern << " does not match " << name << "\n";
	  }
      }
    for (const string& expected : expected_accepts)
      {
	if (actual_accepts.find(expected) == actual_accepts.end())
	  {
	    std::cerr << "match_test: FAIL: expected pattern " << pattern
		      << " to match " << expected << " but it did not\n";
	    return false;
	  }
      }
    for (const string& actual : actual_accepts)
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

  bool self_test()
  {
    DFSContext ctx;
    std::vector<bool> results;

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

    // Checks for trailing blanks (which are present in the catalog but not part of
    // the file name).
    record_test(one_qualify_test(ctx, ":2.B.SPC   ", true, ":2.B.SPC", ""));

    // Invalid file names.
    record_test(one_qualify_test(ctx,   "",   false, "", "not a valid file name"));
    record_test(one_qualify_test(ctx, ":0",   false, "", "not a valid file name"));
    record_test(one_qualify_test(ctx, ":0.",  false, "", "not a valid file name"));
    // Metacharacters are not valid in file names.
    record_test(one_qualify_test(ctx, "#",   false, "", "not a valid file name"));
    record_test(one_qualify_test(ctx, "*",   false, "", "not a valid file name"));
    record_test(one_qualify_test(ctx, ":",   false, "", "not a valid file name"));
    record_test(one_qualify_test(ctx, ".",   false, "", "not a valid file name"));


    record_test(one_wild_test(ctx, ":0.$.*", true, ":0.$.*", ""));
    record_test(one_wild_test(ctx, ":0.$.NAME", true, ":0.$.NAME", ""));
    record_test(one_wild_test(ctx, "$.NAME", true, ":0.$.NAME", ""));
    record_test(one_wild_test(ctx, "#.*", true, ":0.#.*", ""));
    record_test(one_wild_test(ctx, "*.#", true, ":0.*.#", ""));
    record_test(one_wild_test(ctx, "#.##", true, ":0.#.##", ""));
    record_test(one_wild_test(ctx, "I.*", true, ":0.I.*", ""));

    // Some of these expected results rely on the fact that the current
    // working directory ("cwd") is $.
    record_test(match_test(ctx, "Q.*", {}, {}));
    record_test(match_test(ctx, "Q.*", {":0.Q.FLUE"}, {":0.Q.FLUE"}));
    record_test(match_test(ctx, "Q.*", {":0.T.BLUE"}, {}));
    record_test(match_test(ctx, ":0.$.*", {":0.Q.GLUE", ":0.$.GLEAN"}, {":0.$.GLEAN"}));
    record_test(match_test(ctx,    "$.*", {":0.Q.GRUE", ":0.$.GREAT"}, {":0.$.GREAT"}));
    record_test(match_test(ctx,      "*", {":0.Q.TRUE", ":0.$.TREAD"}, {":0.$.TREAD"}));
    record_test(match_test(ctx, "*",
			   {
			    ":0.Q.TRUG", // no match: * only matches in cwd
			    ":1.$.TREAD" // should not be matched because wrong drive
			   }, {}));
    // Tests that verify that matches are case-folded.
    record_test(match_test(ctx, "P*",
			   {":0.$.Price", ":0.$.price"},
			   {":0.$.Price", ":0.$.price"}));
    record_test(match_test(ctx, "P.*",
			   {":0.P.Trice", ":0.p.Trice", ":0.$.Trice"},
			   {":0.P.Trice", ":0.p.Trice"}));

    return std::find(results.cbegin(), results.cend(), false) == results.cend();
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
