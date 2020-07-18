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
#include <iomanip>     // for operator<<, setbase, setfill, setw
#include <iostream>    // for operator<<, cout, ostream, ...
#include <string>      // for string, allocator
#include <vector>      // for vector

#include "commands.h"  // for body_command, file_body_logic, CommandInterface
#include "dfstypes.h"  // for byte

namespace DFS { class StorageConfiguration; }
namespace DFS { struct DFSContext; }

namespace DFS
{

  class CommandList : public DFS::CommandInterface
  {
  public:
    const std::string name() const override
    {
      return "list";
    }

    const std::string usage() const override
    {
      return "usage: list filename\n";
    }

    const std::string description() const override
    {
      return "display the contents of a file as text, with line numbers";
    }

    bool invoke(const DFS::StorageConfiguration& storage,
		const DFS::DFSContext& ctx,
		const std::vector<std::string>& args) override
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
      return body_command(storage, ctx, args, display_numbered_lines);
    }
  };
  REGISTER_COMMAND(CommandList);

}  // namespace DFS
