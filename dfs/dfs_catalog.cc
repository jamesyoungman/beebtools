#include "dfs_catalog.h"

#include <algorithm>
#include <string>
#include <vector>

#include "stringutil.h"

namespace
{
  using DFS::stringutil::byte_to_ascii7;

  std::string convert_title(const DFS::AbstractDrive::SectorBuffer& s0,
			    const DFS::AbstractDrive::SectorBuffer& s1)
  {
    std::string title;
    bool title_done = false;
    for (int offset = 0; offset < 8; ++offset)
      {
	if (!s0[offset])
	  {
	    title_done = true;
	    break;
	  }
	title.push_back(byte_to_ascii7(s0[offset]));
      }
    if (!title_done)
      {
	for (int offset = 0; offset < 4; ++offset)
	  {
	    if (!s1[offset])
	      {
		title_done = true;
		break;
	      }
	    title.push_back(byte_to_ascii7(s1[offset]));
	  }
      }
    return DFS::stringutil::rtrim(title);
  }
}


namespace DFS
{
  CatalogEntry::CatalogEntry(AbstractDrive* media,
			     unsigned short catalog_instance,
			     unsigned short position)
    : drive_(media)
  {
    if (position > 31*8 || position < 8)
      {
	throw std::range_error("request for impossible catalog slot");
      }
    const sector_count_type name_sec = sector_count(catalog_instance * 2u);
    const sector_count_type md_sec = sector_count(name_sec + 1u);
    DFS::AbstractDrive::SectorBuffer buf;
    bool beyond_eof = false;
    media->read_sector(name_sec, &buf, beyond_eof);
    if (beyond_eof)
      throw eof_in_catalog();
    std::copy(buf.cbegin() + position, buf.cbegin() + position + 8,
	      raw_name_.begin());
    media->read_sector(md_sec, &buf, beyond_eof);
    if (beyond_eof)
      throw eof_in_catalog();
    std::copy(buf.cbegin() + position, buf.cbegin() + position + 8,
	      raw_metadata_.begin());
  }

  std::string CatalogEntry::name() const
  {
    std::string result;
    result.reserve(7);
    for (auto it = raw_name_.cbegin(); it != raw_name_.cbegin() + 7; ++it)
      {
	const char ch = DFS::stringutil::byte_to_ascii7(*it);
	if (' ' == ch || '\0' == ch)
	  break;
	result.push_back(ch);
      }
    return result;
  }

  std::string CatalogEntry::full_name() const
  {
    std::string result(1, directory());
    result.push_back('.');
    for (auto ch : name())
      result.push_back(ch);
    return result;
  }

  bool CatalogEntry::has_name(const ParsedFileName& wanted) const
  {
    if (wanted.dir != directory())
      {
#if VERBOSE_FOR_TESTS
	std::cerr << "No match; " << wanted.dir << " != " << directory() << "\n";
#endif
	return false;
      }

    const std::string trimmed_name(stringutil::rtrim(name()));
    if (!stringutil::case_insensitive_equal(wanted.name, trimmed_name))
      {
#if VERBOSE_FOR_TESTS
	std::cerr << "No match; " << wanted.name << " != " << trimmed_name << "\n";
#endif
	return false;
      }
    return true;
  }

  sector_count_type CatalogEntry::last_sector() const
  {
    ldiv_t division = ldiv(file_length(), DFS::SECTOR_BYTES);
    assert(division.quot < std::numeric_limits<int>::max());
    const int sectors_for_this_file = static_cast<int>(division.quot)
      + (division.rem ? 1 : 0);
    return sector_count(start_sector() + sectors_for_this_file - 1);
  }

  bool CatalogEntry::visit_file_body_piecewise
  (std::function<bool(const byte* begin, const byte* end)> visitor) const
  {
    const sector_count_type total_sectors = drive_->get_total_sectors();
    const sector_count_type start = start_sector(), end=last_sector();
    if (start >= total_sectors)
      throw BadFileSystem("file begins beyond the end of the media");
    if (end >= total_sectors)
      throw BadFileSystem("file ends beyond the end of the media");
    unsigned long len = file_length();
    DFS::AbstractDrive::SectorBuffer buf;
    for (sector_count_type sec = start; sec <= end; ++sec)
      {
	assert(sec <= end);
	bool beyond_eof = false;
	drive_->read_sector(sec, &buf, beyond_eof);
	if (beyond_eof)
	  throw BadFileSystem("end of media during body of file");
	unsigned long visit_len = len > SECTOR_BYTES ? SECTOR_BYTES : len;
	if (!visitor(buf.begin(), buf.begin() + visit_len))
	  return false;
	len -= visit_len;
      }
    return true;
  }



  std::string CatalogFragment::read_title(AbstractDrive *drive, DFS::sector_count_type location)
{
  DFS::AbstractDrive::SectorBuffer s0, s1;
  bool beyond_eof = false;
  drive->read_sector(location, &s0, beyond_eof);
  if (beyond_eof)
    throw eof_in_catalog();
  ++location;
  drive->read_sector(location, &s1, beyond_eof);
  if (beyond_eof)
    throw eof_in_catalog();
  return convert_title(s0, s1);
}

  CatalogFragment::CatalogFragment(DFS::Format format, AbstractDrive *drive,
				   DFS::sector_count_type location)
    : disc_format_(format), drive_(drive), location_(location),
      title_(read_title(drive, location))
  {
    AbstractDrive::SectorBuffer s;
    bool beyond_eof = false;
    read_sector(0, &s, beyond_eof);
    if (beyond_eof)
      throw eof_in_catalog();

    const DFS::byte title_initial(s[0]);
    sequence_number_ = s[4];
    read_sector(1, &s, beyond_eof);
    if (location > 1)		// TODO: this check won't be right for Opus DDOS.
      {
	assert(disc_format_ == Format::WDFS);
	if (beyond_eof)
	  {
	    throw BadFileSystem("to be a valid Watford Electronics DFS file system, there must be at least 4 sectors");
	  }
      }
    else if (beyond_eof)
      {
	  throw eof_in_catalog;
      }

    position_of_last_catalog_entry_ = s[5];
    switch ((s[6] >> 4) & 0x03)
      {
      case 0: boot_ = BootSetting::None; break;
      case 1: boot_ = BootSetting::Load; break;
      case 2: boot_ = BootSetting::Run;  break;
      case 3: boot_ = BootSetting::Exec; break;
      }

    total_sectors_ = sector_count(s[7]	// bits 0-7
				  | ((s[6] & 3) << 8));	// bits 8-9
    switch (disc_format_)
      {
      case Format::HDFS:
	// http://mdfs.net/Docs/Comp/Disk/Format/DFS disagrees with
	// the HDFS manual on this (the former states both that this
	// bit is b10 of the total sector count and that it is b10 of
	// the start sector).  We go with what the HDFS manual says.
	if (title_initial & (1 << 7))
	  total_sectors_ |= (1 << 9);
	break;
      default:
	// no more bits
	break;
      }
  }

  std::string CatalogFragment::title() const
  {
    return title_;
  }

  CatalogEntry CatalogFragment::get_entry_at_offset(unsigned int offset) const
  {
    return CatalogEntry(drive_, location_/2, DFS::sector_count(offset));
  }

  std::optional<CatalogEntry> CatalogFragment::find_catalog_entry_for_name(const ParsedFileName& name) const
  {
    for (unsigned offset = 8; offset <= position_of_last_catalog_entry_; offset += 8u)
      {
	CatalogEntry entry = get_entry_at_offset(static_cast<unsigned short>(offset));
	if (entry.has_name(name))
	  return entry;
      }
    return std::nullopt;
  }

  std::vector<CatalogEntry> CatalogFragment::entries() const
  {
    std::vector<CatalogEntry> result;
    unsigned short last = position_of_last_catalog_entry();
    for (unsigned short pos = 8;
	 pos <= last;
	 pos = static_cast<unsigned short>(pos + 8))
      {
	result.push_back(get_entry_at_offset(pos));
      }
    return result;
  }

  void CatalogFragment::read_sector(sector_count_type i, DFS::AbstractDrive::SectorBuffer* buf, bool& beyond_eof) const
  {
    // TODO: this implementation won't work for the Opus format, where
    // the sector locations within the media don't have the same
    // origin for catalogs and files.  For example, in Opus volume C,
    // the catalog is in track 0, but the the initial sector of a file
    // is measured from the location of the first track of that volume
    // (which isn't track 0, because there are no volumes which use
    // track 0 as it's reserved for catalogs).
    return drive_->read_sector(DFS::sector_count(location_ + i), buf, beyond_eof);
  }

  Catalog::Catalog(DFS::Format format, AbstractDrive* drive)
    : disc_format_(format), drive_(drive)
  {
    assert(drive_ != 0);

    // All DFS formats have two sectors of catalog data, at sectors
    // 0 and 1.
    fragments_.push_back(CatalogFragment(format, drive, 0));
    if (disc_format_ == Format::WDFS)
      {
	// Watford DFS has an additional pair of catalog sectors in
	// sectors 2 and 3, providing an additional 31 entries.
	fragments_.push_back(CatalogFragment(format, drive, 2));
      }
  }

  std::optional<byte> Catalog::sequence_number() const
  {
    if (disc_format() != Format::HDFS)
      return primary().sequence_number();

    // In the root catalog, HDFS stores a checksum in this field
    // instead.
    return std::nullopt;
  }

  std::string Catalog::title() const
  {
    return primary().title();
  }

  BootSetting Catalog::boot_setting() const
  {
    return primary().boot_setting();
  }

  sector_count_type Catalog::total_sectors() const
  {
    return primary().total_sectors();
  }

  Format Catalog::disc_format() const
  {
    return disc_format_;
  }

  int Catalog::max_file_count() const
  {
    return disc_format() == Format::WDFS ? 62 : 31;
  }

  std::optional<CatalogEntry> Catalog::find_catalog_entry_for_name(const ParsedFileName& name) const
  {
    for (const auto& frag : fragments_)
      {
	auto result = frag.find_catalog_entry_for_name(name);
	if (result)
	  return result;
      }
    return std::nullopt;
  }

  std::vector<CatalogEntry> Catalog::entries() const
  {
    std::vector<CatalogEntry> result;
    for (const auto& frag : fragments_)
      {
	std::vector<CatalogEntry> frag_entries = frag.entries();
	std::copy(frag_entries.begin(), frag_entries.end(),
		  std::back_inserter(result));
      }
    return result;
  }

  std::vector<std::vector<CatalogEntry>>
  Catalog::get_catalog_in_disc_order() const
  {
    std::vector<std::vector<CatalogEntry>> result;
    result.reserve(fragments_.size());
    for (const auto& frag : fragments_)
      {
	result.push_back(std::vector<CatalogEntry>());
	unsigned short last = frag.position_of_last_catalog_entry();
	for (unsigned short pos = 8;
	     pos <= last;
	     pos = static_cast<unsigned short>(pos + 8))
	  {
	    result.back().push_back(frag.get_entry_at_offset(pos));
	  }
      }
    return result;
  }

  std::vector<sector_count_type> Catalog::get_sectors_occupied_by_catalog() const
  {
    std::vector<sector_count_type> result;
    for (int i = 0; i < catalog_sectors(); ++i)
      result.push_back(DFS::sector_count(i));
    return result;
  }

}  // namespace DFS
