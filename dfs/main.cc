#include <getopt.h>
#include <limits.h>
#include <string.h>

#include <set>
#include <string>
#include <vector>

#include "commands.h"
#include "dfs.h"

namespace
{
  enum OptSignifier
    {
     OPT_IMAGE_FILE = SCHAR_MIN,
     OPT_CWD,
     OPT_DRIVE,
     OPT_SHOW_CONFIG,
     OPT_HELP,
    };

  // struct option fields: name, has_arg, *flag, val
  const struct option global_opts[] =
    {
     // --file controls which disc image file we open
     { "file", 1, NULL, OPT_IMAGE_FILE },
     // --dir controls which directory the program should believe is
     // current (as for *DIR).
     { "dir", 1, NULL, OPT_CWD },
     // --drive controls which drive the program should believe is
     // associated with the disc image specified in --file (as for
     // *DRIVE).
     { "drive", 1, NULL, OPT_DRIVE },
     { "show-config", 0, NULL, OPT_SHOW_CONFIG },
     { "help", 0, NULL, OPT_HELP },
     { 0, 0, 0, 0 },
    };

  bool check_consistency()
  {
    bool ok = true;
    std::set<std::string> global_opt_names;
    // Check that all global options have a help string.
    for (const auto& opt : global_opts)
      {
	if (opt.name == 0)
	  break;
	global_opt_names.insert(opt.name);
	auto it = DFS::option_help.find(opt.name);
	if (it == DFS::option_help.end())
	  {
	    std::cerr << "option_help lacks entry for --" << opt.name << "\n";
	    ok = false;
	  }
      }
    // Check that all option help strings match a global option.
    for (const auto& h : DFS::option_help)
      {
	if (global_opt_names.find(h.first) == global_opt_names.end())
	  {
	    std::cerr << "option_help has entry for "
		      << h.first << " but that's not an actual option "
		      << "in global_opts.\n";
	    ok = false;
	  }
      }
    return ok;
  }

  std::pair<bool, int> get_drive_number(const char *s)
  {
    long v = 0;
    bool ok = [s, &v]() {
		char *end;
		errno = 0;
		v = strtol(s, &end, 10);
		if ((v == LONG_MIN || v == LONG_MAX) && errno)
		  {
		    std::cerr << "Value " << s << " is out of range.\n";
		    return false;
		  }
		if (v == 0 && end == s)
		  {
		    // No digit at optarg[0].
		    std::cerr << "Please specify a decimal number as"
			      << "the argument for --drive\n";
		    return false;
		  }
		if (*end)
		  {
		    std::cerr << "Unexpected non-decimal suffix after "
			      << "argument to --drive: " << end << "\n";
		    return false;
		  }
		if (v < 0 || v > 2)
		  {
		    std::cerr << "Drive number should be between 0 and 2.\n ";
		    return false;
		  }
		return true;
	      }();
    return std::make_pair(ok, static_cast<int>(v));
  }


}  // namespace

namespace DFS 
{
  const std::map<std::string, std::string> option_help =
    {
     {"file", "the name of the DFS image file to read"},
     {"dir", "the default directory (if unspecified, use $)"},
     {"drive", "the default drive (if unspecified, use 0)"},
     {"show-config", "show the storage configuraiton before performing the operation"},
     {"help", "print a brief explanation of how to use the program"},
    };
}  // namespace DFS

int main (int argc, char *argv[])
{
  if (!check_consistency())
    return 2;
  DFS::DFSContext ctx('$', 0);
  int longindex;
  std::vector<std::string> extra_args;
  DFS::StorageConfiguration storage;
  bool show_config = false;

  int opt;
  while ((opt=getopt_long(argc, argv, "+", global_opts, &longindex)) != -1)
    {
      switch (opt)
	{
	case '?':  // Unknown option or missing option argument.
	  // An error message was already issued.
	  return 1;

	case OPT_IMAGE_FILE:
	  try
	    {
	      storage.connect_drive(optarg);
	    }
	  catch (std::exception& e)
	    {
	      std::cerr << "cannot use image file " << optarg << ": "
		   << e.what() << "\n";
	      return 1;
	    }
	  break;

	case OPT_CWD:
	  if (strlen(optarg) != 1)
	    {
	      std::cerr << "Argument to --" << global_opts[longindex].name
			<< " should have one character only.\n";
	      return 1;
	    }
	  ctx.current_directory = optarg[0];
	  break;

	case OPT_DRIVE:
	  {
	    bool ok;
	    std::tie(ok, ctx.current_drive) = get_drive_number(optarg);
	    if (!ok)
	      return 1;
	  }
	  break;

	case OPT_SHOW_CONFIG:
	  show_config = true;
	  break;

	case OPT_HELP:
	  {
	    DFS::CommandHelp help;
	    return help(storage, ctx, extra_args) ? 0 : 1;
	  }
	}
    }
  if (optind == argc)
    {
      std::cerr << "Please specify a command (try \"help\")\n";
      return 1;
    }
  const std::string cmd_name = argv[optind];
  if (optind < argc)
    extra_args.assign(&argv[optind], &argv[argc]);

  auto instance = DFS::CIReg::get_command(cmd_name);
  if (0 == instance)
    {
      std::cerr << "unknown command " << cmd_name << "\n";
      return 1;
    }

  try
    {
      if (show_config)
	{
	  storage.show_drive_configuration(std::cerr);
	}
      return (*instance)(storage, ctx, extra_args) ? 0 : 1;
    }
  catch (std::exception& e)
    {
      std::cerr << "error: " << e.what() << "\n";
      return 1;
    }
};

