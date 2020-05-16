#ifndef INC_DFSIMAGE_H
#define INC_DFSIMAGE_H 1

#include <assert.h>

#include <algorithm>
#include <exception>
#include <functional>
#include <optional>
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
enum class BootSetting
  {
   None, Load, Run, Exec
  };
  std::string description(const BootSetting& opt);
  int value(const BootSetting& opt);

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



class CatalogEntry
{
public:
  // Create a catalog entry object from a catalog on the specified
  // media.  catalog_instance is the ordianal number of the catalog in
  // which the entry can be found (so for Acorn DFS filesystems, this
  // is always 0).  position is the offset within the catalog sectors
  // at which we can find the item.  The initial catlog entry has
  // position==1.  This position value is literally an offset.  For
  // example if a Watford DFS disc has 8 entries in the first catalog
  // and 4 entries in the second, the following constructors are
  // valid:
  // CatalogEntry(m, 0,  1) // first entry in catalog 0
  // CatalogEntry(m, 0, 64) // last entry in catalog 0
  // CatalogEntry(m, 1,  1) // first entry in catalog 1
  // CatalogEntry(m, 1, 32) // last entry in catalog 1
  //
  // This example file system has a total of 12 entries, but this
  // constructor is invalid:
  // CatalogEntry(m, 0, 12) // invalid
  CatalogEntry(AbstractDrive* media,
	       unsigned catalog_instance, unsigned position);
  CatalogEntry(const CatalogEntry& other) = default;
  bool has_name(const ParsedFileName&) const;

  // The name of a file is not space-padded.  So we return
  // "FOO" instead of "FOO    ".
  std::string name() const;
  char directory() const
  {
    return 0x7F & raw_name_[0x07];
  }

  // The "full name" includes the directory, for example "$.FOO".
  std::string full_name() const;

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
};


class FileSystemMetadata
{
public:
  static Format identify_format(AbstractDrive* drive);
  explicit FileSystemMetadata(AbstractDrive *drive);

  Format format() const { return format_; }
  std::string title() const { return title_; }
  std::optional<byte> sequence_number() const
  {
    return sequence_number_;
  }
  unsigned int catalog_count() const
  {
    return position_of_last_catalog_entry_.size();
  }

  unsigned int position_of_last_catalog_entry(int catalog) const
  {
    return position_of_last_catalog_entry_.at(catalog);
  }
  BootSetting boot_setting() const { return boot_; }
  unsigned int total_sectors() const { return total_sectors_; };

private:
  Format format_;
  std::string title_;		// s0 0-7 + s1 0-3 incl.
  std::optional<byte> sequence_number_;	// s1[4]
  std::vector<int> position_of_last_catalog_entry_; // s1[5]
  BootSetting boot_;		// (s1[6] >> 3) & 3
  unsigned total_sectors_;	// s1[7] | (s1[6] & 3) << 8
};

// FileSystem is an image of a single file system (as opposed to a wrapper
// around a disk image file.  For DFS file systems, FileSystem usually
// represents a side of a disk.
class FileSystem
{
public:
 explicit FileSystem(AbstractDrive* drive);

  std::string title() const;

  inline BootSetting opt_value() const
  {
    return metadata_.boot_setting();
  }

  inline std::optional<int> cycle_count() const
  {
    return metadata_.sequence_number();
  }

  inline Format disc_format() const
  {
    return metadata_.format();
  }

  // Get the total number of catalogs.  Acorn DFS has 1,
  // in sectors 0 and 1.  Watford DFS has 2, the second
  // of which lives in sectors 2 and 3.
  int get_number_of_catalogs() const;

  // Get the total number of entries in all catalogs.
  int global_catalog_entry_count() const;
  // Get a catalog entry using a numbering scheme starting with 1 and
  // ending at global_catalog_entry_count().
  CatalogEntry get_global_catalog_entry(int index) const;

  // Return catalog entries in on-disc order.  The outermost vector is
  // the order in which the datalog is stored.  In the case of a
  // Watford DFS sidc for example, entry 0 is the catalog in sectors 0
  // and 1 (i.e. the one also visible to Acorn DFS) and entry 1 is the
  // catalog in sectors 2 and 3 (if it is present).
  //
  // The innermost vector simply stores the catalog entries in the
  // order they occur in the relevant sector.
  std::vector<std::vector<CatalogEntry>>
  get_catalog_in_disc_order() const;
  
  sector_count_type catalog_sectors() const
  {
    return disc_format() == Format::WDFS ? 4 : 2;
  }

  sector_count_type disc_sector_count() const
  {
    return metadata_.total_sectors();
  }

  int max_file_count() const
  {
    return disc_format() == Format::WDFS ? 62 : 31;
  }

  int find_catalog_slot_for_name(const ParsedFileName& name) const;
  std::pair<const byte*, const byte*> file_body(int slot) const;
  bool visit_file_body_piecewise(int slot,
				 std::function<bool(const byte* begin, const byte *end)> visitor) const;

  // sector_to_catalog_entry_mapping uses special values to represent the
  // catalog itself (0) and free sectors (-1).
  static constexpr int sector_unused = -1;
  static constexpr int sector_catalogue = 0;
  std::vector<int> sector_to_catalog_entry_mapping() const;

private:
  byte get_byte(sector_count_type sector, unsigned offset) const;
  AbstractDrive* media_;
  FileSystemMetadata metadata_;
};

}  // namespace DFS

namespace std
{
  std::ostream& operator<<(std::ostream& os, const DFS::Format& f);
  std::ostream& operator<<(std::ostream& os, const DFS::BootSetting& opt);
}

#endif
