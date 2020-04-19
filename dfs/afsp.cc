#include "afsp.h"
#include "dfscontext.h"

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <vector>

#include <assert.h>
#include <regex.h>

using std::string;

namespace 
{
  string render_regex_error(int code, regex_t* preg) 
  {
    string result;
    size_t msg_len = regerror(code, preg, NULL, 0);
    if (msg_len < 2)  // space only for the terminating NUL
      return "regex error message (from regerror) was empty";
    result.resize(msg_len);
    msg_len = regerror(code, preg, result.data(), msg_len);
    result.resize(msg_len - 1);  // Drop terminating NUL.
    return result;
  }

  std::vector<string> match_groups(const char *input,
				   int max_match_count,
				   regmatch_t matches[]) 
  {
    std::vector<string> result;
    result.reserve(max_match_count);
    for (int i = 0; i < max_match_count; ++i) 
      {
	if (matches[i].rm_so != -1)
	  {
	    result.emplace_back(input + matches[i].rm_so, input + matches[i].rm_eo);
	  }
	else
	  {
	    result.emplace_back();
	  }
      }
    return result;
  }

#if defined(VERBOSE_FOR_TESTS)
  void display_matches(const char *reg_pattern, const string& input,
		       const std::vector<string>& matches) 
  {
    std::cout << "Matches of regex " << reg_pattern
	      << " for input " << input << ", "
	      << matches.size() << " groups:\n";
    decltype (matches.size()) i;
    for (i = 0; i < matches.size(); ++i) 
      {
	std::cout << "Group " << std::setw(3) << i << ": ";
	if (!matches[i].empty()) 
	  std::cout << "matched " << matches[i];
	else
	  std::cout << "did not match";
	std::cout << "\n";
      }
  }
#endif

  string drive_prefix(int drive) 
  {
    string result;
    result.reserve(3);
    result.push_back(':');
    result.push_back('0' + drive);
    result.push_back('.');
    return result;
  }

  string directory_prefix(char directory)
  {
    string result(2, '.');
    result[0] = directory;
    return result;
  }
  
  bool qualify(const DFSContext& ctx, const string& filename,
	       string* out, string* error_message)
  {
    static const char invalid[] = "not a valid file name";
    regex_t drive_dir_name;
    auto regex_fail =
      [error_message, out, &drive_dir_name](int code) -> bool 
      {
	error_message->assign(render_regex_error(code, &drive_dir_name));
	out->clear();
	return false;
      };
			
    static const char *ddn_pat = "^"
      "(:[0-9][.])?"		// drive
      "([^.:#*][.])?"		// directory
      "([^.:#*]+)$";		// file name
    int regex_result;
    if (0 != (regex_result=regcomp(&drive_dir_name, ddn_pat, REG_EXTENDED)))
      return regex_fail(regex_result);
    enum { NMATCHES = 4 };
    regmatch_t matches[NMATCHES];
    regex_result = regexec(&drive_dir_name, filename.c_str(), NMATCHES, matches, 0);
    switch (regex_result)
      {
      case 0:			// OK
	break;
      case REG_NOMATCH:
	error_message->assign(invalid);
	return false;
      default:
	return regex_fail(regex_result);
      }
    auto groups = match_groups(filename.c_str(), NMATCHES, matches);
#if defined(VERBOSE_FOR_TESTS)
    display_matches(ddn_pat, filename, groups);
#endif    
    string drive, directory, name;
    if (!groups[1].empty())  
      drive = groups[1];
    else
      drive = drive_prefix(ctx.current_drive);
    if (!groups[2].empty())
      directory = groups[2];
    else
      directory = directory_prefix(ctx.current_directory);
    if (!groups[3].empty())
      {
	name = groups[3];
      }
    else
      {
	error_message->assign(invalid);
	return false;
      }
    out->assign(drive + directory + name);
    return true;
  }

  
  bool one_qualify_test(const DFSContext& ctx,
			const string& filename,
			bool expected_return,
			const string& expected_output,
			const string& expected_error)
  {
    string actual_output, actual_error;
    bool actual_return = qualify(ctx, filename, &actual_output, &actual_error);
    auto describe_call = [&]() -> string
		{
		  return "qualify(ctx, \"" + filename + 
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
	std::cerr << "test failure: " << describe_call() << " result; expected "
		  << expected_output << ", got " << actual_output << "\n";
	show_result();
	return false;
      }
    return true;
  }
}


bool AFSPMatcher::SelfTest() 
{
  DFSContext ctx;
  std::vector<bool> results;
  // Positive cases.
  results.push_back(one_qualify_test(ctx,      "INPUT", true, ":0.$.INPUT", ""));
  results.push_back(one_qualify_test(ctx,    "$.INPUT", true, ":0.$.INPUT", ""));
  results.push_back(one_qualify_test(ctx, ":0.$.INPUT", true, ":0.$.INPUT", ""));
  results.push_back(one_qualify_test(ctx,   ":0.INPUT", true, ":0.$.INPUT", ""));
  results.push_back(one_qualify_test(ctx, "W.Welcome", true, ":0.W.Welcome", ""));
  results.push_back(one_qualify_test(ctx, ":2.&.WHAP", true, ":2.&.WHAP", ""));
  results.push_back(one_qualify_test(ctx,      ":0.$", true, ":0.$.$", ""));
  // Invalid file names.
  results.push_back(one_qualify_test(ctx,   "",   false, "", "not a valid file name"));
  results.push_back(one_qualify_test(ctx, ":0",   false, "", "not a valid file name"));
  results.push_back(one_qualify_test(ctx, ":0.",  false, "", "not a valid file name"));
  // Metacharacters are not valid in file names.
  results.push_back(one_qualify_test(ctx, "#",   false, "", "not a valid file name"));
  results.push_back(one_qualify_test(ctx, "*",   false, "", "not a valid file name"));
  results.push_back(one_qualify_test(ctx, ":",   false, "", "not a valid file name"));
  results.push_back(one_qualify_test(ctx, ".",   false, "", "not a valid file name"));
  return std::find(results.cbegin(), results.cend(), false) == results.cend();
}


AFSPMatcher::AFSPMatcher(const AFSPMatcher::FactoryKey&, const string&)
  : valid_(false)
{
  valid_ = true;
}

std::unique_ptr<AFSPMatcher>
AFSPMatcher::MakeUnique(const string& pattern)
{
  AFSPMatcher::FactoryKey k;
  std::unique_ptr<AFSPMatcher> result = std::make_unique<AFSPMatcher>(k, pattern);
  if (!result)
    return std::move(result);
  if (!result->Valid()) 
    result.reset();
  return std::move(result);
}

