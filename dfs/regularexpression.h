#ifndef INC_REGULAREXPRESSION_H
#define INC_REGULAREXPRESSION_H

#include <regex.h>

#include <string>

namespace DFS 
{
  class RegularExpression
  {
  public:
    explicit RegularExpression(const std::string& pattern)
      : pattern_(pattern) , called_regcomp_(false), error_message_("you must call Compile()"),
	max_matches_(count_groups(pattern))
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

    bool valid() const
    {
      return error_message_.empty();
    }

    static std::string render_regex_error(int code, regex_t* preg)
    {
      std::string result;
      size_t msg_len = regerror(code, preg, NULL, 0);
      if (msg_len < 2)  // space only for the terminating NUL
	return "regex error message (from regerror) was empty";
      result.resize(msg_len);
      msg_len = regerror(code, preg, result.data(), msg_len);
      result.resize(msg_len - 1);  // Drop terminating NUL.
      return result;
    }

#if defined(VERBOSE_FOR_TESTS)
    static void display_matches(const char *reg_pattern, const std::string& input,
				const std::vector<std::string>& matches)
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

    bool compile()
    {
      int regex_result;
      called_regcomp_ = true;
      if (0 != (regex_result=regcomp(&compiled_, pattern_.c_str(), REG_EXTENDED)))
	error_message_ = render_regex_error(regex_result, &compiled_);
      else
	  error_message_.clear();
      return valid();
    }

    const std::string& error_message() const
    {
      return error_message_;
    }

    std::vector<std::string> match(const char* s)
    {
      std::vector<regmatch_t> matches;
      matches.resize(max_matches_);
      const int regex_result = regexec(&compiled_, s, max_matches_, matches.data(), 0);
      std::vector<std::string> groups;

      if (0 == regex_result)
	{
	  error_message_.clear();
	  extract_matches(s, matches, &groups);
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
  static size_t count_groups(const std::string& s)
    {
      // This function does not understand escaping, so it
      // over-estimates, but that's OK.
      return 1 + std::count(s.begin(), s.end(), '(');
    }

  static void extract_matches(const char* input, const std::vector<regmatch_t>& matches, std::vector<std::string>* groups)
    {
      std::vector<std::string> result;
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

  std::string pattern_;
  bool called_regcomp_;
  std::string error_message_;
  regex_t compiled_;
  size_t max_matches_;
  };

}  // namespace DFS  
#endif
