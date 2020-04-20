#include "afsp.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <iomanip>
#include <set>
#include <vector>

#include <assert.h>
#include <regex.h>

#include "fsp.h"
#include "dfscontext.h"

using std::string;
using std::vector;

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

#if defined(VERBOSE_FOR_TESTS)
  void display_matches(const char *reg_pattern, const string& input,
		       const std::vector<string>& matches)
  {
    std::cerr << "Matches of regex " << reg_pattern
	      << " for input " << input << ", "
	      << matches.size() << " groups:\n";
    decltype (matches.size()) i;
    for (i = 0; i < matches.size(); ++i)
      {
	std::cerr << "Group " << std::setw(3) << i << ": ";
	if (!matches[i].empty())
	  std::cerr << "matched " << matches[i];
	else
	  std::cerr << "did not match";
	std::cerr << "\n";
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

  class RegularExpression
  {
  public:
    explicit RegularExpression(const string& pattern)
      : pattern_(pattern) , called_regcomp_(false), error_message_("you must call Compile()"),
	max_matches_(CountGroups(pattern))
    {
    }

    ~RegularExpression()
    {
      if (called_regcomp_)
	{
	  regfree(&compiled_);
	  called_regcomp_ = false;
	}
    }


    bool Valid() const
    {
      return error_message_.empty();
    }

    bool Compile()
    {
      int regex_result;
      called_regcomp_ = true;
      if (0 != (regex_result=regcomp(&compiled_, pattern_.c_str(), REG_EXTENDED)))
	error_message_ = render_regex_error(regex_result, &compiled_);
      else
	  error_message_.clear();
      return Valid();
    }

    const string& ErrorMessage() const
    {
      return error_message_;
    }

    vector<string> Match(const char* s)
    {
      vector<regmatch_t> matches;
      matches.resize(max_matches_);
      const int regex_result = regexec(&compiled_, s, max_matches_, matches.data(), 0);
      vector<string> groups;

      if (0 == regex_result)
	{
	  error_message_.clear();
	  ExtractMatches(s, matches, &groups);
	}
      else if (REG_NOMATCH == regex_result)
	{
	  error_message_.clear();
	}
      else
	{
	  error_message_.assign(render_regex_error(regex_result, &compiled_));
	}
      return groups;
    }

  // Disable copying and assignment.
  RegularExpression& operator=(const RegularExpression&) = delete;
  RegularExpression(const RegularExpression&) = delete;

  private:
    static size_t CountGroups(const string& s)
    {
      // This function does not understand escaping, so it
      // over-estimates, but that's OK.
      return 1 + std::count(s.begin(), s.end(), '(');
    }

    static void ExtractMatches(const char* input, const vector<regmatch_t>& matches, vector<string>* groups)
    {
      vector<string> result;
      result.reserve(matches.size());
      size_t last_nonempty = 0;
      for (size_t i = 0; i < matches.size(); ++i)
	{
	  if (matches[i].rm_so == -1)
	    {
	      result.emplace_back();
	    }
	  else
	    {
	      last_nonempty = i;
	      result.emplace_back(input + matches[i].rm_so, input + matches[i].rm_eo);
	    }
	}
      result.resize(last_nonempty + 1);
      std::swap(result, *groups);
    }

    string pattern_;
    bool called_regcomp_;
    string error_message_;
    regex_t compiled_;
    size_t max_matches_;
  };

  bool transform_string_with_regex(const DFSContext& ctx,
				   const string& regex_pattern,
				   const string& input,
				   const char * invalid,
				   string* out, string* error_message)
  {
    RegularExpression rx(regex_pattern);
    if (!rx.Compile())
      {
	std::cerr << "Failed to compile Regex: " << rx.ErrorMessage();
      }
    assert(rx.Valid());
    auto groups = rx.Match(input.c_str());
    if (!rx.Valid())
      {
	error_message->assign(rx.ErrorMessage());
	return false;
      }
    if (groups.empty())
      {
	error_message->assign(invalid);
	return false;
      }

#if defined(VERBOSE_FOR_TESTS)
    display_matches(regex_pattern.c_str(), input, groups);
#endif

    string drive, directory, name;
    if (groups.size() > 1 && !groups[1].empty())
      drive = groups[1];
    else
      drive = drive_prefix(ctx.current_drive);
    if (groups.size() > 2 && !groups[2].empty())
      directory = groups[2];
    else
      directory = directory_prefix(ctx.current_directory);
    if (groups.size() > 3 && !groups[3].empty())
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

  bool qualify(const DFSContext& ctx, const string& filename,
	       string* out, string* error_message)
  {
    static const char invalid[] = "not a valid file name";
    static const char *ddn_pat = "^"
      "(:[0-9][.])?"		// drive
      "([^.:#*][.])?"		// directory
      "([^.:#*]+)$";		// file name (with trailing blanks trimmed)
    bool result = transform_string_with_regex(ctx, ddn_pat, rtrim(filename), invalid,
					      out, error_message);
#if VERBOSE_FOR_TESTS
    std::cerr << "qualify: '" << filename << "' -> '" << *out << "'\n";
#endif
    return result;
  }

  bool extend_wildcard(const DFSContext& ctx, const string& wild,
		       string* out, string* error_message)
  {
    static const char *ddn_pat = "^"
      "(:[^.][.])?"		// drive
      "([^.][.])?"		// directory
      "([^.]+)$";		// file name
    return transform_string_with_regex(ctx, ddn_pat, wild, "bad name", out, error_message);
    return true;
  }

  /* Convert a DFS ambiguous file specification into a POSIX regular
     expression.   The mapping is:
     DFS       Extended Regex
     :         :     (matches only itself)
     #         [^.]  (matches any single character except .)
     *         [^.]* (matches any sequence of characters other than .)
     .         [.]   (matches only itself)
     x         [xX]  (alphabetic characters match their upper or lower case selves)
     4         [4]   (other characters match only themselves)
  */
  bool convert_wildcard_into_extended_regex(const DFSContext& ctx, const string& wild,
					    string* ere, string* error_message)
  {
    string full_wildcard;
    if (!extend_wildcard(ctx, wild, &full_wildcard, error_message))
      return false;
    assert(full_wildcard.size() > 0 && full_wildcard[0] == ':');
    if (!isdigit(full_wildcard[1]))
      {
	error_message->assign("Bad drive");
	return false;
      }
    vector<char> parts = {'^'};
    for (auto w : full_wildcard)
      {
	switch (w)
	  {
	  case ':':
	    parts.push_back(':');
	    break;

	  case '#':
	    parts.push_back('[');
	    parts.push_back('^');
	    parts.push_back('.');
	    parts.push_back(']');
	    break;

	  case '*':
	    parts.push_back('[');
	    parts.push_back('^');
	    parts.push_back('.');
	    parts.push_back(']');
	    parts.push_back('*');
	    break;

	  case '.':
	  default:
	    if (toupper(w) != tolower(w))
	      {
		parts.push_back('[');
		parts.push_back(toupper(w));
		parts.push_back(tolower(w));
		parts.push_back(']');
	      }
	    else
	      {
		parts.push_back('[');
		parts.push_back(w);
		parts.push_back(']');
	      }
	    break;
	  }
      }
    parts.push_back('$');
    ere->assign(parts.cbegin(), parts.cend());
    return true;
  }

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
    return one_xfrm_test(ctx, "extend_wildcard", extend_wildcard, wildcard,
			 expected_return, expected_output, expected_error);
  }

  bool one_qualify_test(const DFSContext& ctx,
			const string& filename,
			bool expected_return,
			const string& expected_output,
			const string& expected_error)
  {
    return one_xfrm_test(ctx, "qualify", qualify, filename,
			 expected_return, expected_output, expected_error);
  }

  bool match_test(const DFSContext& ctx,
		  const string& pattern,
		  const vector<string>& inputs,
		  const vector<string>& expected_outputs)
  {
    string err;
    std::unique_ptr<AFSPMatcher> m = AFSPMatcher::MakeUnique(ctx, pattern, &err);
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
	if (m->Matches(name))
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

  bool test_rtrim()
  {
    bool outcome = true;
    using std::make_pair;
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

bool AFSPMatcher::SelfTest()
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

  if (!test_rtrim())
    return false;

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


AFSPMatcher::AFSPMatcher(const AFSPMatcher::FactoryKey&,
			 const DFSContext& ctx, const string& wildcard,
			 string *error_message)
  : valid_(false), context_(ctx), implementation_(0)
{
  string ere;
  valid_ = convert_wildcard_into_extended_regex(ctx, wildcard,
						&ere, error_message);
  if (valid_)
    {
#if defined(VERBOSE_FOR_TESTS)
      std::cerr << "AFSPMatcher: wildcard " << wildcard
		<< " translates to ERE pattern " << ere << "\n";
#endif
      RegularExpression* re = new RegularExpression(ere);
      if (!re->Compile() || !re->Valid())
	{
	  valid_ = false;
	  error_message->assign(re->ErrorMessage());
	  delete re;
	  return;
	}
      implementation_ = re;
    }
}

bool AFSPMatcher::Matches(const std::string& name)
{
  if (!Valid())
    return false;
  assert(implementation_);
  string full_name;
  string error_message;
  if (!qualify(context_, name, &full_name, &error_message))
    {
#if VERBOSE_FOR_TESTS
      std::cerr << "not matched (because " << error_message << "): " << full_name
		<< " (canonicalised from " << name << ")\n";
#endif
      return false;
    }
  RegularExpression* re = static_cast<RegularExpression*>(implementation_);
  vector<string> matches = re->Match(full_name.c_str());
  if (matches.empty())
    {
#if VERBOSE_FOR_TESTS
      std::cerr << "not matched: " << full_name
		<< " (canonicalised from " << name << ")\n";
#endif
      return false;
    }
  return true;
}


AFSPMatcher::~AFSPMatcher()
{
  if (implementation_)
    {
      RegularExpression* re = static_cast<RegularExpression*>(implementation_);
      delete re;
    }
}


std::unique_ptr<AFSPMatcher>
AFSPMatcher::MakeUnique(const DFSContext& ctx, const string& pattern, string *error_message)
{
  AFSPMatcher::FactoryKey k;
  std::unique_ptr<AFSPMatcher> result = std::make_unique<AFSPMatcher>(k, ctx, pattern,
								      error_message);
  if (!result)
    return std::move(result);
  if (!result->Valid())
    result.reset();
  return std::move(result);
}

