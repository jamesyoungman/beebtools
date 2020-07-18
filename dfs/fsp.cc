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
#include "fsp.h"

#include <stddef.h>      // for size_t
#include <optional>      // for optional
#include <sstream>       // for operator<<, basic_ostream, ostringstream
#include <utility>       // for swap

#include "dfscontext.h"  // for DFSContext

using std::string;

namespace DFS
{

ParsedFileName::ParsedFileName()
  : vol(0), dir('\0')
{
}

bool parse_filename(const DFSContext& ctx, const std::string& fsp, ParsedFileName* p, std::string& error)
{
  std::string name(fsp);
  ParsedFileName result;
  result.vol = ctx.current_volume;
  result.dir = ctx.current_directory;
  // If there is a drive specification, parse and remove it.
  if (fsp[0] == ':')
    {
      size_t end;
      std::optional<DFS::VolumeSelector> got = DFS::VolumeSelector::parse(fsp.substr(1), &end, error);
      if (!got)
	return false;
      ++end;
      if (fsp[end] != '.')
	{
	  std::ostringstream ss;
	  ss << "file name " << fsp << " has a bad drive specification";
	  error = ss.str();
	  return false;
	}
      result.vol = *got;
      ++end;
      name = name.substr(end);
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
