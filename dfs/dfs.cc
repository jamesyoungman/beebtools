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
#include <locale>
#include <map>
#include <set>
#include <vector>

#include "afsp.h"
#include "commands.h"
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
  const long int HexdumpStride = 8;

  enum OptSignifier
    {
     OPT_IMAGE_FILE = SCHAR_MIN,
     OPT_CWD,
     OPT_DRIVE,
     OPT_SHOW_CONFIG,
     OPT_HELP,
    };

  const int max_command_name_len = 11;

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
  const std::map<string,string> option_help =
    {
     {"file", "the name of the DFS image file to read"},
     {"dir", "the default directory (if unspecified, use $)"},
     {"drive", "the default drive (if unspecified, use 0)"},
     {"show-config", "show the storage configuraiton before performing the operation"},
     {"help", "print a brief explanation of how to use the program"},
    };

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

  bool hexdump_bytes(size_t pos, size_t len, const byte* data)
  {
    cout << std::setw(6) << std::setfill('0') << pos;
    for (size_t i = 0; i < HexdumpStride; ++i)
      {
	if (i < len)
	  cout << ' ' << std::setw(2) << std::setfill('0') << unsigned(data[pos + i]);
	else
	  cout << std::setw(3) << std::setfill(' ') << ' ';
      }
    cout << ' ';
    for (size_t i = 0; i < len; ++i)
      {
	const char ch = data[pos + i];
	if (isgraph(ch))
	  cout << ch;
	else
	  cout << '.';
      }
    cout << '\n';
    return true;
  }

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

  bool create_inf_file(const string& name,
		       unsigned long crc,
		       const DFS::CatalogEntry& entry)
  {
    unsigned long load_addr = sign_extend(entry.load_address());
    unsigned long exec_addr = sign_extend(entry.exec_address());
    std::ofstream inf_file(name, std::ofstream::out);
    inf_file << std::hex << std::uppercase;
    // The NEXT field is missing because our source is not tape.
    using std::setw;
    using std::setfill;
    inf_file << entry.directory() << '.' << entry.name()
	     << setw(6) << setfill('0') << load_addr << " "
	     << setw(6) << setfill('0') << exec_addr << " "
	     << setw(6) << setfill('0') << entry.file_length() << " " // no sign-extend
	     << (entry.is_locked() ? "Locked " : "")
	     << "CRC=" << setw(4) << crc
	     << "\n";
    inf_file.close();
    return inf_file.good();
  }

  inline unsigned long cycle(unsigned long crc)
  {
    if (crc & 32768)
      return  (((crc ^ 0x0810) & 32767) << 1) + 1;
    else
      return crc << 1;
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
}  // namespace

namespace DFS
{
using stringutil::rtrim;
using stringutil::case_insensitive_equal;
using stringutil::case_insensitive_less;

bool body_command(const StorageConfiguration& storage, const DFSContext& ctx,
		  const vector<string>& args,
		  file_body_logic logic)
{
  if (args.size() < 2)
    {
      cerr << "please give a file name.\n";
      return false;
    }
  if (args.size() > 2)
    {
      // The Beeb ignores subsequent arguments.
      cerr << "warning: ignoring additional arguments.\n";
    }
  const FileSystemImage *image;
  if (!storage.select_drive_by_afsp(args[1], &image, ctx.current_drive))
    return false;
  assert(image != 0);

  const int slot = image->find_catalog_slot_for_name(ctx, args[1]);
  if (-1 == slot)
    {
      std::cerr << args[1] << ": not found\n";
      return false;
    }
  auto [start, end] = image->file_body(slot);
  const vector<string> tail(args.begin() + 1, args.end());
  return logic(start, end, tail);
}



class CommandDump : public CommandInterface // *DUMP
{
public:
  const std::string name() const override
    {
      return "dump";
    }

    const std::string usage() const override
    {
      return "usage: dump filename\n";
    }

    const std::string description() const override
    {
      return "displays the contents of a file in both hex and printable characters";
    }

    bool operator()(const DFS::StorageConfiguration& storage,
		    const DFS::DFSContext& ctx,
		    const std::vector<std::string>& args) override
    {
      return body_command(storage, ctx, args,
			  [](const byte* body_start,
			     const byte *body_end,
			     const vector<string>&)
			  {
			    std::cout << std::hex;
			    assert(body_end > body_start);
			    size_t len = body_end - body_start;
			    for (size_t pos = 0; pos < len; pos += HexdumpStride)
			      {
				auto avail = (pos + HexdumpStride > len) ? (len - pos) : HexdumpStride;

				if (!hexdump_bytes(pos, avail, body_start))
				  return false;
			      }
			    return true;
			  });
    }
};
REGISTER_COMMAND(CommandDump);

class CommandExtractAll : public CommandInterface
{
public:
  const std::string name() const override
  {
    return "extract-all";
  }

  const std::string usage() const override
  {
    return "usage: " + name() + " destination-directory\n"
      "All files from the selected drive (see the --drive global option) are\n"
      "extracted into the destination directory.\n"
      "\n"
      "If the DFS directory of the file is not the same as the current\n"
      "directory (selected with --dir) then the output file has a prefix\n"
      "D. where D is the file's DFS directory.  Either way, the DFS directory\n"
      "is given in the .inf file.  If you want the generated files to always\n"
      "contain a the DFS directory prefix, use --dir=. (this works because\n"
      ". is not a valid DFS directory name, so none of the DFS files will\n"
      "have that as their directory.\n"
      "\n"
      "The destination directory must exist already.  An archive .inf file is\n"
      "generated for each file.\n";
  }

  const std::string description() const override
  {
    return "extract all the files from the disc";
  }

  bool operator()(const DFS::StorageConfiguration& storage,
		  const DFS::DFSContext& ctx,
		  const std::vector<std::string>& args) override
  {
    // Use the --drive option to select which drive to extract files from.
    if (args.size() < 2)
      {
	cerr << "extract-all: please specify the destination directory.\n";
	return false;
      }
    if (args.size() > 2)
      {
	cerr << "extract-all: just one argument (the destination directory) is needed.\n";
	return false;
      }
    string dest_dir(args[1]);
    if (dest_dir.back() != '/')
      dest_dir.push_back('/');
    const FileSystemImage* image;
    if (!storage.select_drive(ctx.current_drive, &image))
      {
	cerr << "failed to select current drive " << ctx.current_drive << "\n";
	return false;
      }

    const int entries = image->catalog_entry_count();
    for (int i = 1; i <= entries; ++i)
      {
	const auto& entry = image->get_catalog_entry(i);
	auto [start, end] = image->file_body(i);

	const string output_origname(string(1, entry.directory()) + "." + rtrim(entry.name()));
	string output_basename;
	if (entry.directory() == ctx.current_directory)
	  {
	    output_basename = rtrim(entry.name());
	  }
	else
	  {
	    output_basename = string(1, entry.directory()) + "." + rtrim(entry.name());
	  }
	const string output_body_file = dest_dir + output_basename;

	std::ofstream outfile(output_body_file, std::ofstream::out);
	outfile.write(reinterpret_cast<const char*>(start),
		      end - start);
	outfile.close();
	if (!outfile.good())
	  {
	    std::cerr << output_body_file << ": " << strerror(errno) << "\n";
	    return false;
	  }

	unsigned long crc = compute_crc(start, end);
	const string inf_file_name = output_body_file + ".inf";
	if (!create_inf_file(inf_file_name, crc, entry))
	  {
	    std::cerr << inf_file_name << ": " << strerror(errno) << "\n";
	    return false;
	  }
      }
    return true;
  }
};
REGISTER_COMMAND(CommandExtractAll);


class CommandInfo : public CommandInterface // *INFO
{
public:
  const std::string name() const override
  {
    return "info";
  }

  const std::string usage() const override
  {
    return "usage: " + name() + " wildcard\n"
      "The wildcard specifies which files information should be shown for.\n"
      "To specify all files, use the wildcard #.*\n"
      "Numeric values are shown in hexadecimal (base 16).\n"
      "\n"
      "The output fields are:\n"
      "  file name\n"
      "  'L' if the file is locked, otherwise spaces\n"
      "  load address (in hex)\n"
      "  execution address (in hex)\n"
      "  file length (in hex)\n"
      "  sector within the disc at which the file is stored (in hex)\n"
      "\n"
      "Load and execution addresses are sign-extended from their actual\n"
      "18 bit length (as stored in the disc catalogue) to 24 bits.\n"
      "For example, 3F1900 becomes FF1900.\n"
      "We do this for consistency with the Acorn DFS implementation.\n"
      "When the top bits (i.e. 30000) are set, this signifies that the\n"
      "file was saved from the Tube co-processor rather than the I/O\n"
      "processor.\n";
  }

  const std::string description() const override
  {
    return "display information about a file (for example load address)";
  }

  bool operator()(const DFS::StorageConfiguration& storage,
		  const DFS::DFSContext& ctx,
		  const std::vector<std::string>& args) override
  {
    if (args.size() < 2)
      {
	cerr << "info: please give a file name or wildcard specifying which files "
	     << "you want to see information about.\n";
	return false;
      }
    if (args.size() > 2)
      {
	cerr << "info: please specify no more than one argument (you specified "
	     << (args.size() - 1) << ")\n";
	return false;
      }
    const FileSystemImage *image;
    if (!storage.select_drive_by_afsp(args[1], &image, ctx.current_drive))
      return false;
    assert(image != 0);

    string error_message;
    std::unique_ptr<AFSPMatcher> matcher = AFSPMatcher::make_unique(ctx, args[1], &error_message);
    if (!matcher)
      {
	cerr << "Not a valid pattern (" << error_message << "): " << args[1] << "\n";
	return false;
      }

    const int entries = image->catalog_entry_count();
    cout << std::hex;
    cout << std::uppercase;
    using std::setw;
    using std::setfill;
    for (int i = 1; i <= entries; ++i)
      {
	const auto& entry = image->get_catalog_entry(i);
	const string full_name = string(1, entry.directory()) + "." + entry.name();
#if VERBOSE_FOR_TESTS
	std::cerr << "info: directory is '" << entry.directory() << "'\n";
	std::cerr << "info: item is '" << full_name << "'\n";
#endif
	if (!matcher->matches(full_name))
	  continue;
	unsigned long load_addr, exec_addr;
	load_addr = sign_extend(entry.load_address());
	exec_addr = sign_extend(entry.exec_address());
	cout << entry.directory() << "." << entry.name() << " "
	     << (entry.is_locked() ? " L " : "   ")
	     << setw(6) << setfill('0') << load_addr << " "
	     << setw(6) << setfill('0') << exec_addr << " "
	     << setw(6) << setfill('0') << entry.file_length() << " "
	     << setw(3) << setfill('0') << entry.start_sector()
	     << "\n";
      }
    return true;
  }
};
REGISTER_COMMAND(CommandInfo);

struct comma_thousands : std::numpunct<char>
{
  char do_thousands_sep() const
  {
    return ',';
  }

  std::string do_grouping() const
  {
    return "\3";		// groups of 3 digits
  }
};

class CommandFree : public CommandInterface // *FREE
{
public:
  const std::string name() const override
  {
    return "free";
  }

  const std::string usage() const override
  {
    return name() + " [drive]\n"
      "The used/free space shown reflects the position of the last file on the disc.\n"
      "Using *COMPACT or a similar tool on the disc may free up additional space.\n";
  }

  const std::string description() const override
  {
    return "display information about a disc's free space";
  }

  bool operator()(const DFS::StorageConfiguration& storage,
		  const DFS::DFSContext& ctx,
		  const std::vector<std::string>& args) override
  {
    const FileSystemImage *image;
    if (args.size() > 2)
      {
	cerr << "at most one command-line argument is needed.\n";
	return false;
      }
    if (args.size() < 2)
      {
	if (!storage.select_drive(ctx.current_drive, &image))
	  return false;
      }
    else
      {
	if (!storage.select_drive_by_number(args[1], &image))
	  return false;
      }
    assert(image != 0);

    int sectors_used = 2;
    const int entries = image->catalog_entry_count();
    for (int i = 1; i <= entries; ++i)
      {
	const auto& entry = image->get_catalog_entry(i);
	ldiv_t division = ldiv(entry.file_length(), DFS::SECTOR_BYTES);
	const int sectors_for_this_file = division.quot + (division.rem ? 1 : 0);
	const int last_sector_of_file = entry.start_sector() + sectors_for_this_file;
	if (last_sector_of_file > sectors_used)
	  {
	    sectors_used = last_sector_of_file;
	  }
      }
    int files_free = image->max_file_count() - image->catalog_entry_count();
    int sectors_free = image->disc_sector_count() - sectors_used;
    cout << std::uppercase;
    auto show = [](int files, int sectors, const string& desc)
		{
		  cout << std::setw(2) << std::dec << files << " Files "
		       << std::setw(3) << std::hex << sectors << " Sectors "
		       << std::setw(7) << std::dec << (sectors * SECTOR_BYTES)
		       << " Bytes " << desc << "\n";
		};

    auto prevlocale = cout.imbue(std::locale(cout.getloc(),
					     new comma_thousands)); // takes ownership
    show(files_free, sectors_free, "Free");
    show(image->catalog_entry_count(), sectors_used, "Used");
    cout.imbue(prevlocale);
    return true;
  }
};
REGISTER_COMMAND(CommandFree);

class CommandHelp : public CommandInterface
{
public:
  const std::string name() const override
  {
    return "help";
  }

  const std::string usage() const override
  {
    return name() + " [command]...\n";
  }

  const std::string description() const override
  {
    return "explain how to use one or more commands";
  }

  static bool check_consistency()
  {
    bool ok = true;
    std::set<string> global_opt_names;
    // Check that all global options have a help string.
    for (const auto& opt : global_opts)
      {
	if (opt.name == 0)
	  break;
	global_opt_names.insert(opt.name);
	auto it = option_help.find(opt.name);
	if (it == option_help.end())
	  {
	    std::cerr << "option_help lacks entry for --" << opt.name << "\n";
	    ok = false;
	  }
      }
    // Check that all option help strings match a global option.
    for (const auto& h : option_help)
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

  bool operator()(const DFS::StorageConfiguration&,
		  const DFS::DFSContext&,
		  const std::vector<std::string>& args) override
  {
    const int max_command_name_len = 11;
    if (args.size() < 2)
      {
	cout << "usage: dfs [global-options] command [command-options] [command-arguments]\n"
	     << "\n"
	     << "This is a program for extracting information from Acorn DFS disc images.\n"
	     << "\n"
	     << "The global options affect almost all commands.  See below for details.\n"
	     << "The command is a single word (for example dump, info) specifying what\n"
	     << "action should be performed on one or more of the files within the DFS\n"
	     << "disc image.  The command options modify the way the command works.\n"
	     << "Most commands take no options.  The command-arguments typically specify\n"
	     << "which files within the disc image will be selected.\n"
	     << "\n"
	     << "Global options:\n";
	string::size_type max_option_len = 0;
	for (const auto& h : option_help)
	  {
	    max_option_len = std::max(max_option_len, h.first.size());
	  }
	for (const auto& h : option_help)
	  {
	    cout << "--" << std::left << std::setw(max_option_len)
		 << h.first << ": " << h.second << "\n";
	  }
	cout << "\n";

	const string prefix = "      ";
	cout << "Commands:\n";
	auto show = [prefix, max_command_name_len](CommandInterface* c) -> bool
		    {
		      cout << prefix << std::setw(max_command_name_len)
			   << std::left << c->name() << ": "
			   << c->description() << "\n";
		      return cout.good();
		    };
	auto ok = CIReg::visit_all_commands(show);
	cout << "For help on any individual command, use \"help command-name\"\n";
	return ok && cout.good();
      }
    else
      {
	for (unsigned int i = 1; i < args.size(); ++i)
	  {
	    auto instance = CIReg::get_command(args[i]);
	    if (instance)
	      {
		cout << std::setw(args.size() > 2 ? max_command_name_len : 0)
		     << std::left << args[i]
		     << ": " << instance->description()
		     << "\n" << instance->usage() << "\n";
		if (!cout.good())
		  return false;
	      }
	    else
	      {
		cerr << args[i] << " is not a known command.\n";
		return false;
	      }
	  }
	return true;
      }
  }
};
REGISTER_COMMAND(CommandHelp);

}  // namespace DFS

int main (int argc, char *argv[])
{
  if (!DFS::CommandHelp::check_consistency())
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
	  storage.connect_drive(optarg);
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
  if (show_config) 
    {
      storage.show_drive_configuration(std::cerr);
    }
  return (*instance)(storage, ctx, extra_args) ? 1 : 0;
};

