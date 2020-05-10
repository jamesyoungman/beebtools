#ifndef INC_DFSIMAGE_H
#define INC_DFSIMAGE_H 1

#include <algorithm>
#include <exception>
#include <functional>
#include <vector>
#include <string>
#include <utility>

#include "dfscontext.h"
#include "dfstypes.h"
#include "media.h"
#include "stringutil.h"
#include "fsp.h"
#include "storage.h"

namespace DFS
{
enum class Format
  {
   HDFS,
   DFS,
   WDFS,
   Solidisk,
   // I have no documentation for Opus's format.
  };
  Format identify_format(AbstractDrive* device);
  std::string format_name(Format f);

class BadFileSystem : public std::exception
{
 public:
 BadFileSystem(const std::string& msg)
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
  CatalogEntry(AbstractDrive* media, int slot, Format fmt);
  CatalogEntry(const CatalogEntry& other) = default;
  bool has_name(const ParsedFileName&) const;

  std::string name() const;
  char directory() const;

  bool is_locked() const
  {
    return (1 << 7) & raw_name_[0x07];
  }

  unsigned short metadata_byte(unsigned offset) const
  {
    return raw_metadata_[offset];
  }

  unsigned short metadata_word(unsigned offset) const
  {
    return (raw_metadata_[offset+1] << 8) | raw_metadata_[offset];
  }
  
  unsigned long load_address() const
  {
    unsigned long address = metadata_word(0);
    address |= ((metadata_byte(6) >> 2) & 3) << 16;
    // On Solidisk there is apparently a second copy of bits 16 and 17
    // of the load address, but we only need one copy.
    return address;
  }

  unsigned long exec_address() const
  {
    return metadata_word(0x02) | ((metadata_byte(0x06) >> 6) & 3) << 16;
  }

  unsigned long file_length() const
  {
    return metadata_word(4) | ((metadata_byte(6) >> 4) & 3) << 16;
  }

  unsigned short start_sector() const
  {
    return metadata_byte(7) | ((metadata_byte(6) & 3) << 8);
  }

  unsigned short last_sector() const;
  
private:
  std::array<byte, 8> raw_name_;
  std::array<byte, 8> raw_metadata_;
  Format fmt_;
};

// FileSystem is an image of a single file system (as opposed to a wrapper
// around a disk image file.  For DFS file systems, FileSystem usually
// represents a side of a disk.
class FileSystem
{
public:
 explicit FileSystem(AbstractDrive* drive)
    : media_(drive),
      disc_format_(identify_format(drive))
  {
  }

  std::string title() const;

  inline int opt_value() const
  {
    return (get_byte(1, 0x06) >> 4) & 0x03;
  }

  inline int cycle_count() const
  {
    if (disc_format_ != Format::HDFS)
      {
	return int(get_byte(1, 0x04));
      }
    return -1;				  // signals an error
  }

  inline Format disc_format() const { return disc_format_; }

  offset end_of_catalog() const;
  int catalog_entry_count() const;

  CatalogEntry get_catalog_entry(int index) const
  {
    return CatalogEntry(media_, index, disc_format());
  }

  sector_count_type disc_sector_count() const
  {
    const auto title_initial = get_byte(0, 0);
    sector_count_type result = get_byte(1, 0x07);
    result |= (get_byte(1, 0x06) & 3) << 8;
    if (disc_format_ == Format::HDFS)
      {
	// http://mdfs.net/Docs/Comp/Disk/Format/DFS disagrees with
	// the HDFS manual on this (the former states both that this
	// bit is b10 of the total sector count and that it is b10 of
	// the start sector).  We go with what the HDFS manual says.
	result |= title_initial & (1 << 7);
      }
    return result;
  }

  int max_file_count() const
  {
    return disc_format_ == Format::WDFS ? 62 : 31;
  }

  int find_catalog_slot_for_name(const DFSContext& ctx, const ParsedFileName& name) const;
  std::pair<const byte*, const byte*> file_body(int slot) const;
  bool visit_file_body_piecewise(int slot,
				 std::function<bool(const byte* begin, const byte *end)> visitor) const;

private:
  byte get_byte(sector_count_type sector, unsigned offset) const;
  AbstractDrive* media_;
  Format disc_format_;
};

}  // namespace DFS

namespace std
{
  std::ostream& operator<<(std::ostream& os, const DFS::Format& f);
}


#endif

