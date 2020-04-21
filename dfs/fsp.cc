#include "fsp.h"

using std::string;

namespace DFS
{

std::pair<char, std::string> directory_and_name_of(const DFSContext& ctx,
						   const std::string& filename) 
{
  char dir = ctx.current_directory;
  string::size_type pos = 0;
  if (filename.size() > 1 && filename[1] == '.') 
    {
      dir = filename[0];
      pos = 2;
    }
  return std::make_pair(dir, filename.substr(pos));
}

}  // namespace DFS
