#include "commands.h"

#include <getopt.h>
#include <string.h>

#include <iostream>
#include <map>
#include <string>

#include "dfscontext.h"

namespace
{
  using std::vector;
  using std::string;

  std::pair<std::vector<string>, vector<string>> parse_args(const vector<string> in)
  {
    std::vector<string> options, non_options;
    bool could_be_option = true;
    bool first = true;
    for (const std::string& arg : in)
      {
	if (first)
	  {
	    non_options.push_back(arg); // argv[0] is never an option
	    first = false;
	  }
	else if (!could_be_option || arg.empty())
	  {
	    could_be_option = false;
	    non_options.push_back(arg);
	  }
	else if (arg == "--")
	  {
	    could_be_option = false;
	  }
	else if (arg[0] == '-')
	  {
	    // Does not cope well with --foo=bar.  But we don't have
	    // an option like that.
	    options.push_back(arg);
	  }
	else
	  {
	    could_be_option = false;
	    non_options.push_back(arg);
	  }
      }
    return std::make_pair(options, non_options);
  }
}

namespace DFS
{
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

    const std::string description() const override
    {
      return "display the contents of a file as text";
    }

    bool operator()(const DFS::StorageConfiguration& storage,
		    const DFS::DFSContext& ctx,
		    const std::vector<std::string>& args) override
    {
      bool binary = false;
      std::vector<string> options, non_options;
      std::tie(options, non_options) = parse_args(args);
      for (const auto& opt : options)
	{
	  if (opt == "--binary")
	    {
	      binary = true;
	    }
	  else
	    {
	      std::cerr << "unknown option " << opt << "\n";
	      return false;
	    }
	}

      file_body_logic display_contents =
	[binary](const byte* body_start,
		 const byte *body_end,
		 const std::vector<std::string>&)
	{
	  if (binary)
	    {
	      return std::cout.write(reinterpret_cast<const char*>(body_start),
				     body_end - body_start).good();
	    }
	  else
	    {
	      std::vector<byte> data(body_start, body_end);
	      for (byte& ch : data)
		{
		  if (ch == '\r')
		    ch = '\n';
		}
	      return std::cout.write(reinterpret_cast<const char*>(data.data()), data.size())
		.good();
	    }
	};
      return body_command(storage, ctx, non_options, display_contents);
    }
  };
  REGISTER_COMMAND(CommandType);
}  // namespace DFS
