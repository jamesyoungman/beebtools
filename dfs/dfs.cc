#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>

#include <algorithm>
#include <map>
#include <fstream>
#include <functional>
#include <iostream>
#include <iomanip>
#include <vector>

enum
  {
   SECTOR_BYTES = 256
  };


using std::cerr;
using std::cout;
using std::string;
using std::vector;

typedef unsigned char byte;
typedef vector<byte>::size_type offset;
typedef unsigned short sector_count_type; // needs 10 bits

struct Context
{
  Context(char dir, int drive)
    : current_directory(dir), current_drive(drive)
  {
  }
  char current_directory;
  int current_drive;
};


enum class Format
  {
   HDFS,
   DFS,
   WDFS,
   Solidisk,
  };

namespace
{
  inline static string decode_opt(byte opt)
  {
    switch (opt)
      {
      case 0: return "off";
      case 1: return "load";
      case 2: return "run";
      case 3: return "exec";
      }
    return "?";
  }


  inline char byte_to_char(byte b)
  {
    return char(b);
  }

  inline char byte_to_ascii7(byte b)
  {
    return char(b & 0x7F);
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

  inline Format identify_format(const vector<byte>& image)
  {
    const bool hdfs = image[0x106] & 8;
    if (hdfs)
      return Format::HDFS;

    // Look for the Watford DFS recognition string
    // in the initial entry in its extended catalog.
    if (std::all_of(image.begin()+0x200, image.begin()+0x208,
		    [](byte b) { return b = 0xAA; }))
      {
	return Format::WDFS;
      }

    return Format::DFS;
  }

  inline string ascii7_string(vector<byte>::const_iterator begin,
			      vector<byte>::size_type len)
  {
    string result;
    result.reserve(len);
    std::transform(begin, begin+len,
		   std::back_inserter(result), byte_to_ascii7);
    return result;
  }
}

// Note, a CatalogEntry holds an iterator into the disc image data so
// it must not outlive the image data.
class CatalogEntry
{
public:
  CatalogEntry(vector<byte>::const_iterator image_data, int slot, Format fmt)
    : data_(image_data), cat_offset_(slot * 8), fmt_(fmt)
  {
  }

  CatalogEntry(const CatalogEntry& other)
    : data_(other.data_), cat_offset_(other.cat_offset_), fmt_(other.fmt_)
  {
  }

  inline unsigned char getbyte(unsigned int sector, unsigned short record_off) const
  {
    return data_[sector * 0x100 + record_off + cat_offset_];
  }

  inline unsigned long getword(unsigned int sector, unsigned short record_off) const
  {
    return getbyte(sector, record_off) | (getbyte(sector, record_off + 1) << 8);
  }

  string name() const
  {
    return ascii7_string(data_ + cat_offset_, 0x07);
  }

  char directory() const
  {
    return 0x7F & (data_[cat_offset_ + 0x07]);
  }

  bool is_locked() const
  {
    return (1 << 7) & data_[cat_offset_ + 0x07];
  }

  unsigned long load_address() const
  {
    unsigned long address = getword(1, 0x00);
    address |= ((getbyte(1, 0x06) >> 2) & 3) << 16;
    // On Solidisk there is apparently a second copy of bits 16 and 17
    // of the load address, but we only need one copy.
    return address;
  }

  unsigned long exec_address() const
  {
    return getword(1, 0x02)
      | ((getbyte(1, 0x06) >> 6) & 3) << 16;
  }

  unsigned long file_length() const
  {
    return getword(1, 0x4)
      | ((getbyte(1, 0x06) >> 4) & 3) << 16;
  }

  unsigned short start_sector() const
  {
    return getbyte(1, 0x07) | ((getbyte(1, 0x06) & 3) << 8);
  }

private:
  vector<byte>::const_iterator data_;
  offset cat_offset_;
  Format fmt_;
};


class Image
{
public:
  Image(const vector<byte> disc_image)
    : img_(disc_image), disc_format_(identify_format(disc_image))
  {
  }

  string title() const
  {
    vector<byte> title_data;
    title_data.resize(12);
    std::copy(img_.begin(), img_.begin()+8, title_data.begin());
    std::copy(img_.begin() + 0x100, img_.begin()+0x104, title_data.begin() + 8);
    string result;
    result.resize(12);
    std::transform(title_data.begin(), title_data.end(), result.begin(), byte_to_ascii7);
    return result;
  }

  inline int opt_value() const
  {
    return (img_[0x106] >> 4) & 0x03;
  }

  inline int cycle_count() const
  {
    if (disc_format_ != Format::HDFS)
      {
	return int(img_[0x104]);
      }
    return -1;				  // signals an error
  }

  inline Format disc_format() const { return disc_format_; }

  offset end_of_catalog() const
  {
    return img_[0x105];
  }

  int catalog_entry_count() const
  {
    return end_of_catalog() / 8;
  }

  CatalogEntry get_catalog_entry(int index) const
  {
    // TODO: bounds checking
    return CatalogEntry(img_.cbegin(), index, disc_format());
  }

  sector_count_type disc_sector_count() const
  {
    sector_count_type result = img_[0x107];
    result |= (img_[0x106] & 3) << 8;
    if (disc_format_ == Format::HDFS)
      {
	// http://mdfs.net/Docs/Comp/Disk/Format/DFS disagrees with
	// the HDFS manual on this (the former states both that this
	// bit is b10 of the total sector count and that it is b10 of
	// the start sector).  We go with what the HDFS manual says.
	result |= img_[0] & (1 << 7);
      }
    return result;
  }

private:
  vector<byte> img_;
  Format disc_format_;
};

inline bool case_insensitive_less(const string& left,
				  const string& right)
{
  auto comp =
    [](const unsigned char lhs,
       const unsigned char rhs)
    {
      return tolower(lhs) == tolower(rhs);
    };
  const auto result = std::mismatch(left.cbegin(), left.cend(),
				    right.cbegin(), right.cend(),
				    comp);
  if (result.second == right.cend())
    {
      // Mismatch because right is at end-of-string.  Therefore either
      // the strings are the same length or right is shorter.  So left
      // cannot be less.
      return false;
    }
  if (result.first == left.cend())
    {
      // Equal, so left cannot be less.
      return false;
    }
  return tolower(*result.first) < tolower(*result.second);
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

bool cmd_info(const Image& image, const Context&,
	      const vector<string>&)
{
  const int entries = image.catalog_entry_count();
  cout << std::hex;
  cout << std::uppercase;
  using std::setw;
  using std::setfill;
  for (int i = 1; i <= entries; ++i)
    {
      const auto& entry = image.get_catalog_entry(i);
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

bool cmd_cat(const Image& image, const Context& ctx,
	     const vector<string>&)
{
  cout << image.title();
  if (image.disc_format() != Format::HDFS)
    {
      cout << " ("  << std::setbase(16) << image.cycle_count()
	   << std::setbase(10) << ") FM";
    }
  cout << "\n";
  const auto opt = image.opt_value();
  cout << "Drive "<< ctx.current_drive
       << "            Option "
       << opt << " (" << decode_opt(opt) << ")\n";
  cout << "Dir. :" << ctx.current_drive << "." << ctx.current_directory
       << "          "
       << "Lib. :0.$\n\n";

  const int entries = image.catalog_entry_count();
  vector<int> ordered_catalog_index;
  ordered_catalog_index.reserve(entries);
  ordered_catalog_index.push_back(0);	  // dummy for title
  for (int i = 1; i <= entries; ++i)
    ordered_catalog_index.push_back(i);


  auto compare_entries =
    [&image, &ctx](int left, int right) -> bool
    {
      const auto& l = image.get_catalog_entry(left);
      const auto& r = image.get_catalog_entry(right);
      // Ensure that entries in the current directory sort
      // first.
      auto mapdir =
	[&ctx] (char dir) -> char {
	  return dir == ctx.current_directory ? 0 : tolower(dir);
	};

      if (mapdir(l.directory()) < mapdir(r.directory()))
	return true;
      if (mapdir(r.directory()) < mapdir(l.directory()))
	return false;
      // Same directory, compare names.
      return case_insensitive_less
	(image.get_catalog_entry(left).name(),
	 image.get_catalog_entry(right).name());
    };

  std::sort(ordered_catalog_index.begin()+1,
	    ordered_catalog_index.end(),
	    compare_entries);

  bool left_column = true;
  bool printed_gap = false;
  for (int i = 1; i <= entries; ++i)
    {
      auto entry = image.get_catalog_entry(ordered_catalog_index[i]);
      if (entry.directory() != ctx.current_directory)
	{
	  if (!printed_gap)
	    {
	      if (i > 1)
		cout << (left_column ? "\n" : "\n\n");
	      left_column = true;
	      printed_gap = true;
	    }
	}

      if (!left_column)
	{
	  cout << std::setw(6) << "";
	}

      cout << " ";
      if (entry.directory() != ctx.current_directory)
	cout << " " << entry.directory() << ".";
      else
	cout << "   ";
      cout << entry.name();
      if (entry.is_locked())
	  cout << " L";
      else
	  cout << "  ";

      if (!left_column)
	{
	  cout << "\n";
	}
      left_column = !left_column;
    }
  cout << "\n";
  return true;
}

bool load_image(const char *filename, vector<byte>* image)
{
  std::ifstream infile(filename, std::ifstream::in);
  if (!infile.seekg(0, infile.end))
    return false;
  int len = infile.tellg();
  if (!infile.seekg(0, infile.beg))
    return false;
  image->resize(len);
  return infile.read(reinterpret_cast<char*>(image->data()), len).good();
}

namespace
{
  typedef
  std::function<bool(const Image&,
		     const Context& ctx,
		     const vector<string>& extra_args)> Command;
  std::map<string, Command> commands;
};

bool cmd_help(const Image&, const Context&,
	      const vector<string>&)

{
  cout << "Known commands:\n";
  for (const auto& c : commands)
      cout << "    " << c.first << "\n";
  return true;
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

enum OptSignifier
  {
   OPT_IMAGE_FILE = SCHAR_MIN,
   OPT_CWD,
   OPT_DRIVE,
  };

int main (int argc, char *argv[])
{
  const char *image_file = NULL;
  Context ctx('$', 0);
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

  int opt;
  while ((opt=getopt_long(argc, argv, "+", opts, &longindex)) != -1)
    {
      switch (opt)
	{
	case '?':  // Unknown option or missing option argument.
	  // An error message was already issued.
	  return 1;

	case OPT_IMAGE_FILE:
	  image_file = optarg;
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
  Command x = cmd_cat;
  commands["cat"] = cmd_cat;		  // *CAT
  commands["help"] = cmd_help;
  commands["info"] = cmd_info;		  // *INFO
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

  vector<byte> image;
  if (!load_image(image_file, &image))
    {
      cerr << "failed to dump " << image_file
	   << ": " << strerror(errno) << "\n";
      return 1;
    }
  return selected_command->second(image, ctx, extra_args) ? 1 : 0;
}


