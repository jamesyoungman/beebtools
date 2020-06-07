#include "fsp.h"

#include <iostream>

using std::string;

namespace DFS
{

bool parse_filename(const DFSContext& ctx, const std::string& fsp, ParsedFileName* p)
{
  std::string name(fsp);
  ParsedFileName result;
  result.drive = ctx.current_drive;
  result.dir = ctx.current_directory;
  // If there is a drive specification, parse and remove it.
  if (fsp[0] == ':')
    {
      if (fsp.size() < 3 || !isdigit(fsp[1]) || fsp[2] != '.')
	{
	  std::cerr << "File name " << fsp << " has a bad drive specification\n";
	  return false;
	}
      result.drive = fsp[1] - '0';
      name = std::string(name, 3);
    }
  // name is now an optional directory part followed by a file name.
  if (name.size() > 2)
    {
      if (name[1] == '.')
	{
	  result.dir = name[0];
	  result.name = std::string(name, 2);
	}
      else
	{
	  result.name = name;
	}
    }
  else
    {
      // XXX: this will accept file names like '$.' which don't look
      // valid.  However, Watford DFS accepts these for *TYPE and
      // says "Not found".  We follow this example.
      result.name = name;
    }
  std::swap(result, *p);
  return true;
}

}  // namespace DFS
