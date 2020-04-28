#include "commands.h"

#include <iostream>

#include "dfscontext.h"

namespace DFS
{

  class CommandTypeBinary : public DFS::CommandInterface
  {
    const std::string name() const override
    {
      return "type_binary";
    }

    const std::string usage() const override
    {
      return "type_binary filename\n";
    }

    bool operator()(const DFS::StorageConfiguration& storage,
		    const DFS::DFSContext& ctx,
		    const std::vector<std::string>& args) override
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
      return body_command(storage, ctx, args, display_contents);
    }
  };
  REGISTER_COMMAND(type_binary,CommandTypeBinary);


  class CommandType : public DFS::CommandInterface
  {
    const std::string name() const override
    {
      return "type";
    }

    const std::string usage() const override
    {
      return "type filename\n";
    }

    bool operator()(const DFS::StorageConfiguration& storage,
		    const DFS::DFSContext& ctx,
		    const std::vector<std::string>& args) override
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
	  return std::cout.write(reinterpret_cast<const char*>(data.data()), data.size())
	    .good();
	};
      return body_command(storage, ctx, args, display_contents);
    }
  };
  REGISTER_COMMAND(type,CommandType);
}  // namespace DFS
