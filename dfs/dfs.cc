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

namespace DFS
{

using stringutil::rtrim;
using stringutil::case_insensitive_equal;
using stringutil::case_insensitive_less;


std::map<std::string, Command> commands;
  
namespace
{
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
    return SECTOR_BYTES * sector_num;
  }
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



namespace {
  const long int HexdumpStride = 8;
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

bool cmd_dump(const StorageConfiguration& config, const DFSContext& ctx,
	      const vector<string>& args)
{
  return body_command(config, ctx, args,
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


bool create_inf_file(const string& name,
		     unsigned long crc,
		     const CatalogEntry& entry)
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

bool cmd_extract_all(const StorageConfiguration& config, const DFSContext& ctx,
		     const vector<string>& args)
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
  if (config.select_drive(ctx.current_drive, &image))
      return false;

  const int entries = image->catalog_entry_count();
  for (int i = 1; i <= entries; ++i)
    {
      const auto& entry = image->get_catalog_entry(i);
      auto [start, end] = image->file_body(i);

      const string output_basename(string(1, entry.directory()) + "." + rtrim(entry.name()));
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


bool cmd_info(const StorageConfiguration& storage, const DFSContext& ctx,
	      const vector<string>& args)
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


bool cmd_free(const StorageConfiguration& storage, const DFSContext& ctx,
	      const vector<string>& args)
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
      ldiv_t division = ldiv(entry.file_length(), SECTOR_BYTES);
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



bool cmd_help(const StorageConfiguration&, const DFSContext&,
	      const vector<string>&)

{
  cout << "Known commands:\n";
  for (const auto& c : commands)
      cout << "    " << c.first << "\n";
  return true;
}

}  // namespace DFS


namespace {

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

enum OptSignifier
  {
   OPT_IMAGE_FILE = SCHAR_MIN,
   OPT_CWD,
   OPT_DRIVE,
  };

}  // namespace


int main (int argc, char *argv[])
{
  DFS::DFSContext ctx('$', 0);
  int longindex;
  // struct option fields: name, has_arg, *flag, val
  static const struct option opts[] =
    {
     // --file controls which disk image file we open
     { "file", 1, NULL, OPT_IMAGE_FILE },
     // --dir controls which directory the program should believe is
     // current (as for *DIR).
     { "dir", 1, NULL, OPT_CWD },
     // --drive controls which drive the program should believe is
     // associated with the disk image specified in --file (as for
     // *DRIVE).
     { "drive", 1, NULL, OPT_DRIVE },
     { 0, 0, 0, 0 },
    };

  DFS::StorageConfiguration storage;
  int opt;
  while ((opt=getopt_long(argc, argv, "+", opts, &longindex)) != -1)
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
	      cerr << "Argument to --" << opts[longindex].name
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
	}
    }
  if (optind == argc)
    {
      cerr << "Please specify a command (try \"help\")\n";
      return 1;
    }
  using DFS::commands;
  commands["cat"] =  DFS::cmd_cat;  // *CAT
  commands["help"] = DFS::cmd_help;
  commands["info"] = DFS::cmd_info; // *INFO
  commands["type"] = DFS::cmd_type; // *TYPE
  commands["dump"] = DFS::cmd_dump; // *DUMP
  commands["list"] = DFS::cmd_list; // *LIST
  commands["free"] = DFS::cmd_free; // *FREE
  commands["extract-all"] = DFS::cmd_extract_all;
  const string cmd_name = argv[optind];
  vector<string> extra_args;
  if (optind < argc)
    extra_args.assign(&argv[optind], &argv[argc]);
  auto selected_command = commands.find(cmd_name);
  if (selected_command == commands.end())
    {
      cerr << "unknown command " << cmd_name << "\n";
      return 1;
    }

  storage.show_drive_configuration(std::cout);
  return selected_command->second(storage, ctx, extra_args) ? 1 : 0;
}
