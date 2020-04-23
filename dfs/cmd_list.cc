#include "commands.h"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "dfscontext.h"
#include "dfsimage.h"

namespace DFS
{

bool cmd_list(const StorageConfiguration& config, const DFSContext& ctx,
	      const std::vector<std::string>& args)
{
  file_body_logic display_numbered_lines =
    [](const byte* body_start,
       const byte *body_end,
       const std::vector<std::string>&) -> bool
    {
      int line_number = 1;
      bool start_of_line = true;
      std::cout << std::setfill(' ') << std::setbase(10);
      for (const byte *p = body_start; p < body_end; ++p)
	{
	  if (start_of_line)
	    {
	      std::cout << std::setw(4) << line_number++ << ' ';
	      start_of_line = false;
	    }
	  if (*p == 0x0D)
	    {
	      std::cout << '\n';
	      start_of_line = true;
	    }
	  else
	    {
	      std::cout << static_cast<char>(*p);
	    }
	}
      return true;
    };
  return body_command(config, ctx, args, display_numbered_lines);
}

}  // namespace DFS
