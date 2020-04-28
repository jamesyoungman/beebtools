#include "commands.h"

#include <iostream>

#include "dfscontext.h"

namespace DFS
{

bool cmd_type_binary(const StorageConfiguration& config, const DFSContext& ctx,
		     const std::vector<std::string>& args)
{
  file_body_logic display_contents =
    [](const byte* body_start,
       const byte *body_end,
       const std::vector<std::string>&)
    {
      std::vector<byte> data(body_start, body_end);
      return std::cout.write(reinterpret_cast<const char*>(body_start),
			     body_end-body_start).good();
    };
  return body_command(config, ctx, args, display_contents);
}


bool cmd_type(const StorageConfiguration& config, const DFSContext& ctx,
	      const std::vector<std::string>& args)
{
  file_body_logic display_contents =
    [](const byte* body_start,
       const byte *body_end,
       const std::vector<std::string>&)
    {
      std::vector<byte> data(body_start, body_end);
      for (byte& ch : data)
	{
	  if (ch == '\r')
	    ch = '\n';
	}
      return std::cout.write(reinterpret_cast<const char*>(data.data()), data.size()).good();
    };
  return body_command(config, ctx, args, display_contents);
}

}  // namespace DFS
