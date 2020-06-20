#include "afsp.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <iomanip>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include <assert.h>

#include "fsp.h"
#include "stringutil.h"
#include "dfscontext.h"
#include "driveselector.h"
#include "regularexpression.h"
#include "storage.h"

using std::string;
using std::vector;

namespace
{
  using DFS::stringutil::rtrim;
  using DFS::AFSPMatcher;
  using DFS::DFSContext;
  using DFS::RegularExpression;
  using DFS::StorageConfiguration;

  inline char up(char ch)
  {
    return static_cast<char>(toupper(static_cast<unsigned char>(ch)));
  }

  inline char down(char ch)
  {
    return static_cast<char>(tolower(static_cast<unsigned char>(ch)));
  }

  string drive_prefix(DFS::drive_number drive)
  {
    string result;
    result.reserve(3); // common, but not an upper limit
    result.push_back(':');
    result.append(std::to_string(drive));
    result.push_back('.');
    return result;
  }

  string directory_prefix(char directory)
  {
    string result(2, '.');
    result[0] = directory;
    return result;
  }

  bool transform_string_with_regex(const DFS::drive_number drive_num,
				   const char dir,
				   const string& input,
				   const string& regex_pattern,
				   const char * invalid,
				   string* out, string* error_message)
  {
    RegularExpression rx(regex_pattern);
    if (!rx.compile())
      {
	std::ostringstream ss;
	ss << "failed to compile regular expression " << regex_pattern
	   << ": " << rx.error_message();
	error_message->assign(ss.str());
	return false;
      }
    assert(rx.valid());
    auto groups = rx.match(input.c_str());
    if (!rx.valid())
      {
	error_message->assign(rx.error_message());
	return false;
      }
    if (groups.empty())
      {
	error_message->assign(invalid);
	return false;
      }

#if defined(VERBOSE_FOR_TESTS)
    RegularExpression::display_matches(regex_pattern.c_str(), input, groups);
#endif

    string drive, directory, name;
    if (groups.size() > 1 && !groups[1].empty())
      drive = groups[1];
    else
      drive = drive_prefix(drive_num);
    if (groups.size() > 2 && !groups[2].empty())
      directory = groups[2];
    else
      directory = directory_prefix(dir);
    if (groups.size() > 3 && !groups[3].empty())
      {
	name = groups[3];
      }
    else
      {
	error_message->assign(invalid);
	return false;
      }
     // TODO: there is probably a more efficient way to perform this assignment.
    out->assign(drive + directory + name);
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

     Some documentation claims that the Acorn DFS does not allow * in
     a position other than at the end of the wildcard, but my testing
     shows that this varies:
     Acorn DFS 2.26 supports *INFO *2
     Acorn DFS 0.90 does not (giving the error "Bad filename")

     As for other vendors:
     Watford DDFS 1.53 does support it
     Opus DDOS 3.45 does not (giving the error "Bad drive")
     Solidisk DOS 2.1 does not (giving the error "Bad filename")
  */
  bool convert_wildcard_into_extended_regex(DFS::drive_number* drive_num, char dir, const string& wild,
					    string* ere, string* error_message)
  {
    string full_wildcard;
    if (!DFS::internal::extend_wildcard(*drive_num, dir, wild, &full_wildcard, error_message))
      return false;
    assert(full_wildcard.size() > 0 && full_wildcard[0] == ':');
    // We expect the wildcard to be of the form :NN.D.blah where DD is
    // the drive number.
    if (!isdigit(full_wildcard[1]))
      {
	error_message->assign("No drive number in " + full_wildcard);
	return false;
      }
    size_t end;
    try
      {
	// The base matters because we support drive numbers greater
	// than 3.
	unsigned long d = std::stoul(full_wildcard.substr(1), &end, 10);
	// To allow us to use end as an index into full_wildcard, we
	// increment it here, to compensate for the substr(1) above.
	if (end == std::numeric_limits<decltype(end)>::max())
	  {
	    error_message->assign("Drive number is too long");
	    return false;
	  }
	*drive_num = static_cast<DFS::drive_number>(d);
	++end;
      }
    catch (std::exception& e)
      {
	error_message->assign("Bad drive number in " + full_wildcard + ": " + e.what());
	return false;
      }
    if (full_wildcard[end] != '.')
      {
	error_message->assign("Non-digit after drive number in " + full_wildcard
			      + ", specifically " + full_wildcard.substr(end));
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
	    if (up(w) != down(w))
	      {
		parts.push_back('[');
		parts.push_back(up(w));
		parts.push_back(down(w));
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
}  // namespace

namespace DFS
{
  namespace internal
  {
    bool qualify(DFS::drive_number drive_num, char dir, const string& filename,
		 string* out, string* error_message)
    {
      static const char invalid[] = "not a valid file name";
      static const char *ddn_pat = "^"
	"(:[0-9]+[.])?"	// drive (more than one digit is OK)
	"([^.:#*][.])?"		// directory
	"([^.:#*]+)$";		// file name (with trailing blanks trimmed)
      bool result = transform_string_with_regex(drive_num, dir, rtrim(filename), ddn_pat, invalid,
						out, error_message);
#if VERBOSE_FOR_TESTS
      std::cerr << "qualify: '" << filename << "' -> '" << *out << "'\n";
#endif
      return result;
    }

    bool extend_wildcard(DFS::drive_number drive_num, char dir, const string& wild,
			 string* out, string* error_message)
    {
      static const char *ddn_pat = "^"
	"(:[0-9]+[.])?"		// drive (more than one digit is OK)
	"([^.][.])?"		// directory
	"([^.]+)$";		// file name
      return transform_string_with_regex(drive_num, dir, wild, ddn_pat, "bad name", out, error_message);
    }

  }  // namespace internal

AFSPMatcher::AFSPMatcher(const AFSPMatcher::FactoryKey&,
			 const DFSContext& ctx, const string& wildcard,
			 string *error_message)
  : valid_(false), drive_num_(ctx.current_drive), implementation_(0)
{
  string ere;
  valid_ = convert_wildcard_into_extended_regex(&drive_num_,
						ctx.current_directory,
						wildcard, &ere, error_message);
  if (valid_)
    {
#if defined(VERBOSE_FOR_TESTS)
      std::cerr << "AFSPMatcher: wildcard " << wildcard
		<< " translates to ERE pattern " << ere << "\n";
#endif
      RegularExpression* re = new RegularExpression(ere);
      if (!re->compile() || !re->valid())
	{
	  valid_ = false;
	  error_message->assign(re->error_message());
	  delete re;
	  return;
	}
      implementation_ = re;
    }
}

DFS::drive_number AFSPMatcher::get_drive_number() const
{
  return drive_num_;
}

bool AFSPMatcher::matches(DFS::drive_number drive_num, char directory, const std::string& name)
{
  if (!valid())
    return false;
  assert(implementation_);
  string full_name;
  string error_message;
  if (!DFS::internal::qualify(drive_num, directory, name, &full_name, &error_message))
    {
#if VERBOSE_FOR_TESTS
      std::cerr << "not matched (because " << error_message << "): " << full_name
		<< " (canonicalised from " << name << ")\n";
#endif
      return false;
    }
  RegularExpression* re = static_cast<RegularExpression*>(implementation_);
  vector<string> matches = re->match(full_name.c_str());
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
AFSPMatcher::make_unique(const DFSContext& ctx, const string& pattern, string *error_message)
{
  AFSPMatcher::FactoryKey k;
  std::unique_ptr<AFSPMatcher> result = std::make_unique<AFSPMatcher>(k, ctx, pattern,
								      error_message);
  if (!result->valid())
    result.reset();
  return std::move(result);
}

}  // namespace DFS
