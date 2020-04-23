#ifndef INC_DFSIMAGE_H
#define INC_DFSIMAGE_H 1

#include <algorithm>
#include <exception>
#include <vector>
#include <string>
#include <utility>

#include "dfscontext.h"
#include "dfstypes.h"
#include "stringutil.h"

namespace DFS
{
  enum
  {
   SECTOR_BYTES = 256
  };

enum class Format
  {
   HDFS,
   DFS,
   WDFS,
   Solidisk,
  };
 Format identify_format(const std::vector<byte>& image);

class BadFileSystemImage : public std::exception
{
 public:
 BadFileSystemImage(const std::string& msg)
   : error_message_(std::string("bad disk image: ") + msg)
    {
    }

  const char *what() const throw()
  {
    return error_message_.c_str();
  }
 private:
  std::string error_message_;
};



// Note, a CatalogEntry holds an iterator into the disc image data so
// it must not outlive the image data.
class CatalogEntry
{
public:
 CatalogEntry(std::vector<byte>::const_iterator image_data, int slot, Format fmt)
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

  std::string name() const;
  char directory() const;

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
  std::vector<byte>::const_iterator data_;
  offset cat_offset_;
  Format fmt_;
};

// FileSystemImage is an image of a single file system (as opposed to a wrapper
// around a disk image file.  For DFS file systems, FileSystemImage usually
// represents a side of a disk.
class FileSystemImage
{
public:
 explicit FileSystemImage(const std::vector<byte>& disc_image)
    : img_(disc_image), disc_format_(identify_format(disc_image))
  {
  }

  std::string title() const;

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

  int find_catalog_slot_for_name(const DFSContext& ctx, const std::string& arg) const;
  std::pair<const byte*, const byte*> file_body(int slot) const;

private:
  std::vector<byte> img_;
  Format disc_format_;
};



}  // namespace DFS

#endif

