#include "dfs_catalog.h"

#include <algorithm>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>

#include "abstractio.h"
#include "dfs.h"
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
  sector_count_type catalog_sectors_for_format(const Format& f)
  {
    return f == Format::WDFS ? 4 : 2;
  }

  CatalogEntry::CatalogEntry(const DFS::byte* name, const DFS::byte* metadata)
  {
    std::copy(name, name+8, raw_name_.begin());
    std::copy(metadata, metadata+8, raw_metadata_.begin());
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
    const auto start = start_sector();
    const auto len = file_length();
    if (0 == len)
      return start;
    ldiv_t division = ldiv(len, DFS::SECTOR_BYTES);
    assert(division.quot < std::numeric_limits<int>::max());
    const int sectors_for_this_file = static_cast<int>(division.quot)
      + (division.rem ? 1 : 0);
    return sector_count(start + sectors_for_this_file - 1);
  }

  bool CatalogEntry::visit_file_body_piecewise
  (DataAccess& media,
   std::function<bool(const byte* begin, const byte* end)> visitor) const
  {
    const sector_count_type start = start_sector(), end=last_sector();
    unsigned long len = file_length();
    for (sector_count_type sec = start; sec <= end; ++sec)
      {
	assert(sec <= end);
	auto buf = media.read_block(sec);
	if (!buf)
	  throw BadFileSystem("end of media during body of file");
	unsigned long visit_len = len > SECTOR_BYTES ? SECTOR_BYTES : len;
	if (!visitor(buf->begin(), buf->begin() + visit_len))
	  return false;
	len -= visit_len;
      }
    return true;
  }



CatalogFragment::CatalogFragment(DFS::Format format,
				 const DFS::SectorBuffer& names,
				 const DFS::SectorBuffer& metadata)
    : disc_format_(format), title_(convert_title(names, metadata))
  {
    const DFS::byte title_initial(names[0]);
    sequence_number_ = metadata[4];
    position_of_last_catalog_entry_ = metadata[5];
    switch ((metadata[6] >> 4) & 0x03)
      {
      case 0: boot_ = BootSetting::None; break;
      case 1: boot_ = BootSetting::Load; break;
      case 2: boot_ = BootSetting::Run;  break;
      case 3: boot_ = BootSetting::Exec; break;
      }

    total_sectors_ = sector_count(metadata[7]	// bits 0-7
				  | ((metadata[6] & 3) << 8));	// bits 8-9
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
    for (int pos = 8; pos <= position_of_last_catalog_entry_; pos += 8)
      {
	entries_.push_back(CatalogEntry(names.data() + pos,
					metadata.data() + pos));
      }
  }

  std::string CatalogFragment::title() const
  {
    return title_;
  }

  CatalogEntry CatalogFragment::get_entry_at_offset(unsigned int offset) const
  {
    assert(offset % 8 == 0);
    assert(offset >= 8);
    return entries_[offset / 8 - 1];
  }

  std::optional<CatalogEntry> CatalogFragment::find_catalog_entry_for_name(const ParsedFileName& name) const
  {
    auto it = std::find_if(entries_.cbegin(), entries_.cend(),
			   [&name](const CatalogEntry& entry) -> bool
			   {
			     return entry.has_name(name);
			   });
    if (it == entries_.cend())
      return std::nullopt;
    else
      return *it;
  }

  std::vector<CatalogEntry> CatalogFragment::entries() const
  {
    return entries_;
  }

  bool CatalogFragment::valid(std::string& error) const
  {
    std::ostringstream os;
    unsigned short last = position_of_last_catalog_entry();
    if  (last % 8)
      {
	os << "position of last catalog entry is " << last
	   << " but it is supposed to be a multiple of 8";
	error = os.str();
	return false;
      }
    if (last > 31 * 8)
      {
	error = "position of last catalog entry is beyond the end of the catalog";
	return false;
      }
    // An Acorn DFS catalog takes up 2 sectors, so a catalog whose
    // total sector count is less than 3 is definitely not valid, as
    // the disc would not be able to contain any files.
    if (total_sectors_ <= catalog_sectors_for_format(disc_format_))
      {
	os << "total sector count for catalog is only " << total_sectors_;
	error = os.str();;
	return false;
      }
    std::optional<DFS::sector_count_type> last_file_start;
    for (unsigned short pos = 8;
	 pos <= last;
	 pos = static_cast<unsigned short>(pos + 8))
      {
	auto entry = get_entry_at_offset(pos);
	if (last_file_start)
	  {
	    if (entry.last_sector() >= total_sectors())
	      {
		os << "catalog entry " << pos
		   << " indicates a file body ending at sector "
		   << entry.last_sector()
		   << " but the device only has " << total_sectors()
		   << " sectors in total";
		error = os.str();
		return false;
	      }
	    if (entry.last_sector() >= *last_file_start)
	      {
		os << "catalog entries " << pos << " and " << (pos-8)
		   << " indicate files overlapping at sector "
		   << *last_file_start;
		error = os.str();
		return false;
	      }
	  }
	last_file_start = entry.start_sector();
      }
    return true;
  }

  Catalog::Catalog(DFS::Format format, DataAccess& media)
    : disc_format_(format), media_(media)
  {
    // All DFS formats have two sectors of catalog data, at sectors
    // 0 and 1.  WDFS also at 2 and 3.
    const int frag_count = disc_format_ == Format::WDFS ? 2 : 1;
    fragments_.reserve(frag_count);
    for (DFS::sector_count_type base = 0 ; base < frag_count*2 ; /* empty */)
      {
	std::optional<DFS::SectorBuffer> names = media.read_block(base++);
	std::optional<DFS::SectorBuffer> metadata = media.read_block(base++);
	if (!names || !metadata)
	  {
	    std::ostringstream os;
	    os << "to contain a valid " << format_name(disc_format_)
	       << " catalog, the file system must contain at least "
	       << (frag_count * 2) << " sectors";
	    throw new BadFileSystem(os.str());
	  }
	fragments_.push_back(CatalogFragment(format, *names, *metadata));
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


namespace std
{
  ostream& operator<<(ostream& os, const DFS::CatalogFragment& f)
  {
    auto entries = f.entries();
    os << "Title " << f.title() << "\n"
       << "Boot setting " << f.boot_setting() << "\n"
       << "Total sectors " << f.total_sectors() << "\n"
       << entries.size() << " entries"
       << (entries.size() ? ":" : "") << "\n";
    for (const DFS::CatalogEntry& entry : entries)
      {
	os << entry << "\n";
      }
    return os;
  }

  ostream& operator<<(ostream& os, const DFS::CatalogEntry& entry)
  {
    unsigned long load_addr, exec_addr;
    load_addr = DFS::sign_extend(entry.load_address());
    exec_addr = DFS::sign_extend(entry.exec_address());
    return os << entry.directory() << "."
	      << left
	      << setw(8) << setfill(' ')
	      << entry.name() << " "
	      << setw(3)
	      << (entry.is_locked() ? "L" : "")
	      << std::right
	      << setw(6) << setfill('0') << load_addr << " "
	      << setw(6) << setfill('0') << exec_addr << " "
	      << setw(6) << setfill('0') << entry.file_length() << " "
	      << setw(3) << setfill('0') << entry.start_sector();
  }
}
