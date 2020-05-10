#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <exception>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <vector>

#include "afsp.h"
#include "commands.h"
#include "dfs.h"
#include "dfscontext.h"
#include "dfsimage.h"
#include "fsp.h"
#include "stringutil.h"

using std::cerr;
using std::cout;
using std::string;
using std::vector;

using DFS::byte;
using DFS::offset;

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

  const int max_command_name_len = 11;

  inline char byte_to_char(byte b)
  {
    return char(b);
  }

  inline string extract_string(const vector<byte>& image,
			       offset position, offset size)
  {
    std::string result;
    std::transform(image.begin()+position, image.begin()+position+size,
		   std::back_inserter(result), byte_to_char);
    return result;
  }

  inline offset sector(int sector_num)
  {
    return DFS::SECTOR_BYTES * sector_num;
  }

  inline unsigned long cycle(unsigned long crc)
  {
    if (crc & 32768)
      return  (((crc ^ 0x0810) & 32767) << 1) + 1;
    else
      return crc << 1;
  }

  std::pair<bool, int> get_drive_number(const char *s)
  {
    long v = 0;
    bool ok = [s, &v]() {
		char *end;
		errno = 0;
		v = strtol(optarg, &end, 10);
		if ((v == LONG_MIN || v == LONG_MAX) && errno)
		  {
		    cerr << "Value " << optarg << " is out of range.\n";
		    return false;
		  }
		if (v == 0 && end == optarg)
		  {
		    // No digit at optarg[0].
		    cerr << "Please specify a decimal number as"
			 << "the argument for --drive\n";
		    return false;
		  }
		if (*end)
		  {
		    cerr << "Unexpected non-decimal suffix after "
			 << "argument to --drive: " << end << "\n";
		    return false;
		  }
		if (v < 0 || v > 2)
		  {
		    cerr << "Drive number should be between 0 and 2.\n ";
		    return false;
		  }
		return true;
	      }();
    return std::make_pair(ok, static_cast<int>(v));
  }

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

}  // namespace

namespace DFS
{
using stringutil::rtrim;
using stringutil::case_insensitive_equal;
using stringutil::case_insensitive_less;

  unsigned long sign_extend(unsigned long address)
  {
    /*
      The load and execute addresses are 18 bits.  The largest unsigned
      18-bit value is 0x3FFFF (or &3FFFF if you prefer).  However, the
      DFS *INFO command prints the address &3F1900 as FF1900.  This is
      because, per pages K.3-1 to K.3-2 of the BBC Master Reference
      manual part 2,

      > BASIC sets the high-order bits of the load address to the
      > high-order address of the processor it is running on.  This
      > enables you to tell if a file was saved from the I/O processor
      > or a co-processor.  For example if there was a BASIC file
      > called prog1, its information might look like this:
      >
      > prog1 FFFF0E00 FFFF8023 00000777 000023
      >
      > This indicates that prog1 was saved on an I/O processor-only
      > machine with PAGE set to &E00.  The execution address
      > (FFFF8023) is not significant for BASIC programs.
    */
    if (address & 0x20000)
      {
	// We sign-extend just two digits (unlike the example above) ,
	// as this is what the BBC model B DFS does.
	return 0xFF0000 | address;
      }
    else
      {
	return address;
      }
  }

  unsigned long compute_crc(const byte* start, const byte *end)
  {
    unsigned long crc = 0;
    for (const byte* p = start; p < end; ++p)
      {
	crc ^= *p++ << 8;
	for(int k = 0; k < 8; k++)
	  crc = cycle(crc);
	assert((crc & ~0xFFFF) == 0);
      }
    return crc;
  }


  const std::map<string,string> option_help =
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
  vector<string> extra_args;
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
	      cerr << "cannot use image file " << optarg << ": "
		   << e.what() << "\n";
	      return 1;
	    }
	  break;

	case OPT_CWD:
	  if (strlen(optarg) != 1)
	    {
	      cerr << "Argument to --" << global_opts[longindex].name
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
      cerr << "Please specify a command (try \"help\")\n";
      return 1;
    }
  const string cmd_name = argv[optind];
  if (optind < argc)
    extra_args.assign(&argv[optind], &argv[argc]);

  auto instance = DFS::CIReg::get_command(cmd_name);
  if (0 == instance)
    {
      cerr << "unknown command " << cmd_name << "\n";
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
      cerr << "error: " << e.what() << "\n";
      return 1;
    }
};

