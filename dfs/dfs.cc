#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <assert.h>

#include <algorithm>
#include <map>
#include <exception>
#include <locale>
#include <fstream>
#include <functional>
#include <iostream>
#include <iomanip>
#include <vector>

#include "dfscontext.h"
#include "afsp.h"
#include "fsp.h"
#include "stringutil.h"

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

namespace DFS
{

using stringutil::rtrim;
using stringutil::case_insensitive_equal;
using stringutil::case_insensitive_less;

enum class Format
  {
   HDFS,
   DFS,
   WDFS,
   Solidisk,
  };


typedef std::function<bool(int /* sector number */,
			   const byte data[],
			   unsigned short len)> FileSectorVisitor;

class ExceptionBase : public std::exception
{
public:
  explicit ExceptionBase(const string& msg)
    : error_message_(msg) {}

  const char *what() const throw()
  {
    return error_message_.c_str();
  }
protected:
  std::string error_message_;
};

// class InternalError : public std::logic_error
// {
// public:
//   explicit InternalError(const string& msg)
//     : std::logic_error("internal error: " + msg)
//   {
//   }
// };

class OsError : public ExceptionBase
{
public:
  explicit OsError(int errno_value)
    : ExceptionBase(strerror(errno_value))
  {
  }
};


class BadImage : public ExceptionBase
{
public:
  BadImage(const string& msg)
    : ExceptionBase("bad disk image: " + msg)
  {
  }
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

  int max_file_count() const
  {
    // We do not currently correctly support Watford DFS format discs.
    return 31;
  }


  int find_catalog_slot_for_name(const DFSContext& ctx, const string& arg) const
  {
    auto [dir, name] = directory_and_name_of(ctx, arg);
#if VERBOSE_FOR_TESTS
    std::cerr << "find_catalog_slot_for_name: dir=" << dir << ", name=" << name << "\n";
#endif
    const int entries = catalog_entry_count();
    for (int i = 1; i <= entries; ++i)
      {
	const auto& entry = get_catalog_entry(i);
#if VERBOSE_FOR_TESTS
	std::cerr << "Looking for " << arg << ", considering " << entry.directory()
		  << "." << entry.name() << "\n";
#endif
	const string trimmed_name(rtrim(entry.name()));

	if (dir != entry.directory())
	  {
#if VERBOSE_FOR_TESTS
	    std::cerr << "No match; " << dir << " != " << entry.directory() << "\n";
#endif
	    continue;
	  }

	if (!case_insensitive_equal(name, trimmed_name))
	  {
#if VERBOSE_FOR_TESTS
	    std::cerr << "No match; " << name << " != " << trimmed_name << "\n";
#endif
	    continue;
	  }

	return i;
      }
    return -1;
  }

  std::pair<const byte*, const byte*> file_body(int slot) const
  {
    if (slot < 1 || slot > catalog_entry_count())
      throw std::range_error("catalog slot is out of range");

    auto entry = get_catalog_entry(slot);
    auto offset = static_cast<unsigned long>(SECTOR_BYTES) * entry.start_sector();
    auto length = entry.file_length();
    if (length > img_.size())
      throw BadImage("file size for catalog entry is larger than the disk image");
    if (offset > (img_.size() - length))
      throw BadImage("file extends beyond the end of the disk image");
    const byte* start = img_.data() + offset;
    return std::make_pair(start, start + length);
  }

private:
  vector<byte> img_;
  Format disc_format_;
};

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

typedef std::function<bool(const unsigned char* body_start,
			   const unsigned char* body_end,
			   const vector<string>& args_tail)> file_body_logic;


bool body_command(const Image& image, const DFSContext& ctx,
		  const vector<string>& args,
		  file_body_logic logic)
{
  if (args.size() < 2)
    {
      cerr << "please ive a file name.\n";
      return false;
    }
  if (args.size() > 2)
    {
      // The Beeb ignores subsequent arguments.
      cerr << "warning: ignoring additional arguments.\n";
    }
  const int slot = image.find_catalog_slot_for_name(ctx, args[1]);
  if (-1 == slot)
    {
      std::cerr << args[1] << ": not found\n";
      return false;
    }
  auto [start, end] = image.file_body(slot);
  const vector<string> tail(args.begin() + 1, args.end());
  return logic(start, end, tail);
}



bool cmd_type(const Image& image, const DFSContext& ctx,
	      const vector<string>& args)
{
  file_body_logic display_contents =
    [](const byte* body_start,
       const byte *body_end,
       const vector<string>&)
    {
      vector<byte> data(body_start, body_end);
      for (byte& ch : data)
	{
	  if (ch == '\r')
	    ch = '\n';
	}
      return std::cout.write(reinterpret_cast<const char*>(data.data()), data.size()).good();
    };
  return body_command(image, ctx, args, display_contents);
}

bool cmd_list(const Image& image, const DFSContext& ctx,
	      const vector<string>& args)
{
  file_body_logic display_numbered_lines =
    [](const byte* body_start,
       const byte *body_end,
       const vector<string>&) -> bool
    {
      int line_number = 1;
      bool start_of_line = true;
      cout << std::setfill(' ') << std::setbase(10);
      for (const byte *p = body_start; p < body_end; ++p)
	{
	  if (start_of_line)
	    {
	      cout << std::setw(4) << line_number++ << ' ';
	      start_of_line = false;
	    }
	  if (*p == 0x0D)
	    {
	      cout << '\n';
	      start_of_line = true;
	    }
	  else
	    {
	      cout << static_cast<char>(*p);
	    }
	}
      return true;
    };
  return body_command(image, ctx, args, display_numbered_lines);
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

bool cmd_dump(const Image& image, const DFSContext& ctx,
	      const vector<string>& args)
{
  return body_command(image, ctx, args,
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

bool cmd_extract_all(const Image& image, const DFSContext&,
		     const vector<string>& args)
{
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
  const int entries = image.catalog_entry_count();
  for (int i = 1; i <= entries; ++i)
    {
      const auto& entry = image.get_catalog_entry(i);
      auto [start, end] = image.file_body(i);

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


bool cmd_info(const Image& image, const DFSContext& ctx,
	      const vector<string>& args)
{
  if (args.size() < 2)
    {
      cerr << "info: please give a file name of wildcard specifying which files "
	   << "you want to see information about.\n";
      return false;
    }
  if (args.size() > 2)
    {
      cerr << "info: please specify no more than one argument (you specified "
	   << (args.size() - 1) << ")\n";
      return false;
    }
  string error_message;
  std::unique_ptr<AFSPMatcher> matcher = AFSPMatcher::MakeUnique(ctx, args[1], &error_message);
  if (!matcher)
    {
      cerr << "Not a valid pattern (" << error_message << "): " << args[1] << "\n";
      return false;
    }

  const int entries = image.catalog_entry_count();
  cout << std::hex;
  cout << std::uppercase;
  using std::setw;
  using std::setfill;
  for (int i = 1; i <= entries; ++i)
    {
      const auto& entry = image.get_catalog_entry(i);
      const string full_name = string(1, entry.directory()) + "." + entry.name();
#if VERBOSE_FOR_TESTS
      std::cerr << "info: directory is '" << entry.directory() << "'\n";
      std::cerr << "info: item is '" << full_name << "'\n";
#endif
      if (!matcher->Matches(full_name))
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

bool cmd_cat(const Image& image, const DFSContext& ctx,
	     const vector<string>&)
{
  // TODO: sole argument is the drive number.
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

void load_image(const char *filename, vector<byte>* image)
{
  std::ifstream infile(filename, std::ifstream::in);
  int len;

  const bool ok = [&len, &infile] {
		    if (!infile.seekg(0, infile.end))
		      return false;
		    len = infile.tellg();
		    if (len == 0)
		      return false;
		    if (!infile.seekg(0, infile.beg))
		      return false;
		    return true;
		  }();
  if (!ok)
    throw OsError(errno);
  image->resize(len);
  if (len < SECTOR_BYTES * 2)
    throw BadImage("disk image is too short to contain a valid catalog");
  if (!infile.read(reinterpret_cast<char*>(image->data()), len).good())
    throw OsError(errno);
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


bool cmd_free(const Image& image, const DFSContext&,
	      const vector<string>& args)
{
  if (args.size() > 1)
    {
      cerr << "no additional command-line arguments are needed.\n";
      return false;
    }
  int sectors_used = 2;
  const int entries = image.catalog_entry_count();
  for (int i = 1; i <= entries; ++i)
    {
      const auto& entry = image.get_catalog_entry(i);
      ldiv_t division = ldiv(entry.file_length(), SECTOR_BYTES);
      const int sectors_for_this_file = division.quot + (division.rem ? 1 : 0);
      const int last_sector_of_file = entry.start_sector() + sectors_for_this_file;
      if (last_sector_of_file > sectors_used)
	{
	  sectors_used = last_sector_of_file;
	}
    }
  int files_free = image.max_file_count() - image.catalog_entry_count();
  int sectors_free = image.disc_sector_count() - sectors_used;
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
  show(image.catalog_entry_count(), sectors_used, "Used");
  cout.imbue(prevlocale);
  return true;
}



namespace
{
  typedef
  std::function<bool(const Image&,
		     const DFSContext& ctx,
		     const vector<string>& extra_args)> Command;
  std::map<string, Command> commands;
};

bool cmd_help(const Image&, const DFSContext&,
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
  const char *image_file = NULL;
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
  using DFS::commands;
  commands["cat"] =  DFS::cmd_cat;	// *CAT
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

  vector<byte> image;
  try
    {
      DFS::load_image(image_file, &image);
    }
  catch (DFS::ExceptionBase& e)
    {
      cerr << "failed to dump " << image_file << ": " << e.what() << "\n";
      return 1;
    }
  return selected_command->second(image, ctx, extra_args) ? 1 : 0;
}


