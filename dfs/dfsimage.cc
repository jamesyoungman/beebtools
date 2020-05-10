#include "dfsimage.h"

#include <assert.h>

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>
#include <vector>

#include "fsp.h"

using std::string;

namespace
{
  inline char byte_to_ascii7(DFS::byte b)
  {
    return char(b & 0x7F);
  }

  template <typename Iter>
  std::string ascii7_string(Iter begin, Iter end)
  {
    string result;
    result.reserve(end-begin);
    std::transform(begin, end,
		   std::back_inserter(result), byte_to_ascii7);
    return result;
  }
}  // namespace

namespace DFS
{
  byte FileSystem::get_byte(sector_count_type sector, unsigned offset) const
  {
    assert(offset < DFS::SECTOR_BYTES);
    AbstractDrive::SectorBuffer buf;
    media_->read_sector(sector, &buf);
    return buf[offset];
  }

offset calc_cat_offset(int slot, Format fmt)
{
  if (fmt != Format::WDFS || slot <= 31)
    return slot * 8;
  /* In WDFS sectors 0 and 1 are as for DFS, and sectors
   * 2 and 3 are for the second 31 files.  The first 8 bytes
   * of sector 2 are recognition bytes.
   */
  return 0x200 + ((slot-31) * 8);
}


CatalogEntry::CatalogEntry(AbstractDrive* media, int slot,
			   Format fmt)
{
  unsigned name_sec = 0, md_sec = 1, pos;
  if (slot > 62)
    {
      throw std::range_error("request for impossible catalog slot");
    }
  if (slot > 31)
    {
      if (fmt != Format::WDFS)
	{
	  throw std::range_error("request for extended catalog slot in non-Watford disk");
	}
      name_sec = 2;
      md_sec = 3;
      pos = (slot - 31) * 8;
    }
  else
    {
      pos = slot * 8;
    }
  DFS::AbstractDrive::SectorBuffer buf;
  media->read_sector(name_sec, &buf);
  std::copy(buf.cbegin() + pos, buf.cbegin() + pos + 8, raw_name_.begin());
  media->read_sector(md_sec, &buf);
  std::copy(buf.cbegin() + pos, buf.cbegin() + pos + 8, raw_metadata_.begin());
}

std::string CatalogEntry::name() const
{
  return ascii7_string(raw_name_.cbegin(), raw_name_.cbegin() + 0x07);
}

char CatalogEntry::directory() const
{
  return 0x7F & raw_name_[0x07];
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

unsigned short CatalogEntry::last_sector() const
{
  unsigned long len = file_length();
  ldiv_t division = ldiv(file_length(), DFS::SECTOR_BYTES);
  const int sectors_for_this_file = division.quot + (division.rem ? 1 : 0);
  return start_sector() + sectors_for_this_file;
}
  
  
std::string FileSystem::title() const
{
  std::vector<byte> title_data;
  title_data.resize(12);

  DFS::AbstractDrive::SectorBuffer buf;
  media_->read_sector(0, &buf);
  std::copy(buf.cbegin(), buf.cbegin()+8, title_data.begin());
  media_->read_sector(1, &buf);
  std::copy(buf.cbegin(), buf.cbegin()+4, title_data.begin() + 8);
  std::string result;
  result.resize(12);
  std::transform(title_data.begin(), title_data.end(), result.begin(), byte_to_ascii7);
  // TODO: rtrim here?
  return result;
}

  offset FileSystem::end_of_catalog() const
  {
    return get_byte(1, 5);
  }

  int FileSystem::catalog_entry_count() const
  {
    auto end_of_catalog = get_byte(1, 5);
    int count = end_of_catalog / 8;
    if (disc_format_ == Format::WDFS)
      {
	count += get_byte(3, 5) / 8;
      }
    return count;
  }

int FileSystem::find_catalog_slot_for_name(const DFSContext& ctx, const ParsedFileName& name) const
{
  const int entries = catalog_entry_count();
  for (int i = 1; i <= entries; ++i)
    {
      if (get_catalog_entry(i).has_name(name))
	return i;
    }
  return -1;
}

  
bool FileSystem::visit_file_body_piecewise
(int slot,
 std::function<bool(const byte* begin, const byte* end)> visitor) const
{
  if (slot < 1 || slot > catalog_entry_count())
    throw std::range_error("catalog slot is out of range");

  const sector_count_type total_sectors = media_->get_total_sectors();
  const auto entry = get_catalog_entry(slot);
  const sector_count_type start=entry.start_sector(), end=entry.last_sector();
  if (start >= total_sectors)
    throw BadFileSystem("file begins beyond the end of the media");
  if (end >= total_sectors)
    throw BadFileSystem("file ends beyond the end of the media");
  unsigned long len = entry.file_length();
  for (sector_count_type sec = start; sec <= end; ++sec)
    {
      assert(sec <= end);
      DFS::AbstractDrive::SectorBuffer buf;
      media_->read_sector(sec, &buf);
      unsigned long visit_len = len > SECTOR_BYTES ? SECTOR_BYTES : len;
      if (!visitor(buf.begin(), buf.begin() + visit_len))
	return false;
      len -= visit_len;
    }
  return true;
}

Format identify_format(AbstractDrive* drive)
{
  AbstractDrive::SectorBuffer buf;
  drive->read_sector(1, &buf);
  
  if (buf[0x06] & 8)
    return Format::HDFS;

  // DFS provides 31 file slots, and Watford DFS 62.  Watford DFS does
  // this by doubling the size of the catalog into sectors 2 and 3 (as
  // well as DFS's 0 and 1).  It puts recognition bytes in sector 2.
  // However, it's possible for a DFS-format file to contain the
  // recognition bytes in its body.  We don't want to be fooled if
  // that happens.  To avoid it, we check whether the body of any file
  // (of the standard DFS 31 files) starts in sector 2.  If so, this
  // cannot be a Watford DFS format disc.
  const byte last_catalog_entry_pos = buf[0x05];
  unsigned pos = 8;
  for (pos = 8; pos <= last_catalog_entry_pos; pos += 8)
    {
      auto start_sector = buf[pos + 7];
      if (start_sector == 2)
	{
	  /* Sector 2 is used by a file, so not Watford DFS. */
	  return Format::DFS;
	}
    }

  // Look for the Watford DFS recognition string
  // in the initial entry in its extended catalog.
  drive->read_sector(2, &buf);
  if (std::all_of(buf.cbegin(), buf.cbegin()+0x08,
		  [](byte b) { return b == 0xAA; }))
    {
      return Format::WDFS;
    }
  return Format::DFS;
}

  std::string format_name(Format f)
  {
    switch (f)
      {
      case Format::HDFS: return "HDFS";
      case Format::DFS: return "Acorn DFS";
      case Format::WDFS: return "Watford DFS";
      case Format::Solidisk: return "Solidisk DFS";
      }
    abort();
  }

}  // namespace DFS

namespace std
{
  std::ostream& operator<<(std::ostream& os, const DFS::Format& f)
  {
    return os << DFS::format_name(f);
  }
}
