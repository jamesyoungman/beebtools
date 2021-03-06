//
//   Copyright 2020 James Youngman
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
#ifndef INC_DFS_CATALOG_H
#define INC_DFS_CATALOG_H

#include <array>         // for array
#include <functional>    // for function
#include <iosfwd>        // for ostream
#include <optional>      // for optional
#include <string>        // for string
#include <utility>       // for pair
#include <vector>        // for vector

#include "abstractio.h"  // for SectorBuffer
#include "dfs_format.h"  // for Format
#include "dfstypes.h"    // for byte, sector_count_type
#include "fsp.h"	 // for ParsedFileName

namespace DFS
{
  struct ParsedFileName;
  class SectorMap;
  class VolumeSelector;

  sector_count_type catalog_sectors_for_format(const Format&);
  sector_count_type data_sectors_reserved_for_catalog(const Format&);

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
  CatalogEntry(const DFS::byte* name, const DFS::byte* metadata);
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
    return static_cast<unsigned short>((raw_metadata_[offset+1] << 8)
				       | raw_metadata_[offset]);
  }

  unsigned long load_address() const
  {
    unsigned long address = metadata_word(0);
    address |= ((metadata_byte(6) >> 2) & 3uL) << 16;
    // On Solidisk there is apparently a second copy of bits 16 and 17
    // of the load address, but we only need one copy.
    return address;
  }

  unsigned long exec_address() const
  {
    return metadata_word(0x02) | ((metadata_byte(0x06) >> 6) & 3uL) << 16;
  }

  unsigned long file_length() const
  {
    return metadata_word(4) | ((metadata_byte(6) >> 4) & 3uL) << 16;
  }

  sector_count_type start_sector() const
  {
    return static_cast<sector_count_type>(metadata_byte(7)
					  | ((metadata_byte(6) & 3) << 8));
  }

  sector_count_type last_sector() const;

  std::pair<const byte*, const byte*> file_body(int slot) const;
  bool visit_file_body_piecewise(DataAccess& media,
				 std::function<bool(const byte* begin,
						    const byte *end)> visitor) const;

private:
  std::array<byte, 8> raw_name_;
  std::array<byte, 8> raw_metadata_;
};

enum class BootSetting
  {
   None, Load, Run, Exec
  };
std::string description(const BootSetting& opt);
int value(const BootSetting& opt);


// CatalogFragment is a 2-sector catalog (i.e. equivalent to an HDFS
// directory or an Acorn DFS root catalog; two of these are needed for
// Watford DFS.
class CatalogFragment
{
public:
  explicit CatalogFragment(DFS::Format format,
			   const DFS::SectorBuffer& names,
			   const DFS::SectorBuffer& metadata);

  bool valid(std::string& error) const;
  std::string title() const;

  std::optional<byte> sequence_number() const
  {
    return sequence_number_;
  }

  std::vector<CatalogEntry> entries() const;

  std::optional<CatalogEntry> find_catalog_entry_for_name(const ParsedFileName& name) const;
  BootSetting boot_setting() const { return boot_; }
  sector_count_type total_sectors() const { return total_sectors_; };
  void read_sector(DataAccess&, sector_count_type n,
		   DFS::SectorBuffer* buf, bool& beyond_eof) const;

 private:
  friend class Catalog;
  CatalogEntry get_entry_at_offset(unsigned) const;
  unsigned short position_of_last_catalog_entry() const
  {
    return position_of_last_catalog_entry_;
  }

  DFS::Format disc_format_;
  // class invariant: drive_ != 0
  sector_count_type location_;
  std::string title_;		// s0 0-7 + s1 0-3 incl.
  std::optional<byte> sequence_number_;	// s1[4]
  unsigned short position_of_last_catalog_entry_; // s1[5]
  BootSetting boot_;		// (s1[6] >> 3) & 3
  sector_count_type total_sectors_; // s1[7] | (s1[6] & 3) << 8
  std::vector<CatalogEntry> entries_;
};


class Catalog
{
 public:
  explicit Catalog(DFS::Format format, DFS::sector_count_type loc, DataAccess&);

  const CatalogFragment& primary() const
  {
    return fragments_.front();
  }

  bool valid(std::string& error) const;
  std::string title() const;
  std::optional<byte> sequence_number() const;
  BootSetting boot_setting() const;
  sector_count_type total_sectors() const;
  Format disc_format() const;
  int max_file_count() const;

  std::optional<CatalogEntry> find_catalog_entry_for_name(const ParsedFileName& name) const;

  // Return all the catalog entries.  This is normally the best way to
  // iterate over entries.  The entries are returned in the same order
  // as "*INFO".
  std::vector<CatalogEntry> entries() const;

  // Return catalog entries in on-disc order.  The outermost vector is
  // the order in which the datalog is stored.  In the case of a
  // Watford DFS sidc for example, entry 0 is the catalog in sectors 0
  // and 1 (i.e. the one also visible to Acorn DFS) and entry 1 is the
  // catalog in sectors 2 and 3 (if it is present).
  //
  // The innermost vector simply stores the catalog entries in the
  // order they occur in the relevant sector.
  std::vector<std::vector<CatalogEntry>> get_catalog_in_disc_order() const;

  sector_count_type catalog_sectors() const
  {
    return catalog_sectors_for_format(disc_format());
  }

  void map_sectors(const VolumeSelector& vol,
		   unsigned long catalog_origin_lba,
		   unsigned long data_origin_lba,
		   SectorMap* out) const;

private:
  DFS::Format disc_format_;
  // class invariant: drive_ is non-null.
  // class invariant: fragments_ is non-empty.
  // class invariant: for all items f in fragments_, f.drive_ == drive.
  std::vector<CatalogFragment> fragments_;
};

}  // namespace DFS

namespace std
{
  std::ostream& operator<<(std::ostream& os, const DFS::BootSetting&);
  std::ostream& operator<<(std::ostream& os, const DFS::CatalogFragment&);
  std::ostream& operator<<(std::ostream& os, const DFS::CatalogEntry&);
}  // namespace std

#endif
