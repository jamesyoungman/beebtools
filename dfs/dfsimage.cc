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
  Format FileSystemMetadata::identify_format(AbstractDrive* drive)
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

  FileSystemMetadata::FileSystemMetadata(AbstractDrive *drive)
    : format_(identify_format(drive))
  {
    AbstractDrive::SectorBuffer s;
    drive->read_sector(0, &s);
    const byte title_initial = s[0];
    std::transform(&s[0], &s[8], std::back_inserter(title_), byte_to_ascii7);
    drive->read_sector(1, &s);
    std::transform(&s[0], &s[4], std::back_inserter(title_), byte_to_ascii7);
    if (format_ != Format::HDFS)
      {
	sequence_number_ = s[4];
      }
    else
      {
	// I have not been able to find a description of what Duggan's
	// HDFS uses the "key number" field for.
	sequence_number_.reset();
      }
    position_of_last_catalog_entry_ = s[5];
    // s1[6] is where all the interesting stuff alternate-format-wise is.  Bits:
    // b0: bit 8 of total sector count (Acorn => all)
    // b1: bit 9 of total sector count (Acorn => all)
    // b2: recognition ID, low bit: Watford large (if b3 unset) or HDFS double sided
    //     For Solidisk DDFS, bit 10 of start sector
    // b3: recognition ID, high bit: if set, disc is HDFS
    //     For Solidisk DDFS, bit 18 of ? (file length according to MDFS.net,
    //     but that seems off, since there is only one copy of this value, the
    //     files can't all have the same file length value)
    // b4: OPT 4 setting (low bit) (Acorn => all)
    // b5: OPT 4 setting (high bit) (Acorn => all)
    // b6:
    // b7:
    //
    // Recognition ID values:
    // 0: Acorn DFS or Watford DFS (distinguish by looking at catalogue)
    // 1: Watford DFS, large disk
    // 2: HDFS single-sided
    // 3: HDFS double-sided
    if (s[6] & 8)
      {
	// s1[6] & 8 is the HDFS recognition bit.
	assert(format_ == Format::HDFS);
      }
    else
      {
	assert(format_ != Format::HDFS);
	if (s[6] & 4)
	  {
	    // Watford large disk
	    assert(format_ == Format::WDFS);
	  }
	else
	  {
	    // Either Acorn or Watford DFS.
	    assert(format_ == Format::WDFS || format_ == Format::DFS);
	  }
      }

    switch ((s[6] >> 4) & 0x03)
      {
      case 0: boot_ = BootSetting::None; break;
      case 1: boot_ = BootSetting::Load; break;
      case 2: boot_ = BootSetting::Run;  break;
      case 3: boot_ = BootSetting::Exec; break;
      }

    total_sectors_ = s[7]	// bits 0-7
      | ((s[6] & 3) << 8);	// bits 8-9
    switch (format_)
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

  FileSystem::FileSystem(AbstractDrive* drive)
    : media_(drive),
      metadata_(drive)
  {
  }

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
  return metadata_.title();
}

  offset FileSystem::end_of_catalog() const
  {
    return metadata_.position_of_last_catalog_entry();
  }

  int FileSystem::catalog_entry_count() const
  {
    int count = end_of_catalog() / 8;
    if (disc_format() == Format::WDFS)
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

std::string description(const BootSetting& opt)
{
  switch (opt)
    {
    case BootSetting::None: return "off";
    case BootSetting::Load: return "load";
    case BootSetting::Run: return "run";
    case BootSetting::Exec: return "exec";
    }
  return "?";
}

int value(const BootSetting& opt)
{
  return static_cast<int>(opt);
}

}  // namespace DFS

namespace std
{
  std::ostream& operator<<(std::ostream& os, const DFS::Format& f)
  {
    return os << DFS::format_name(f);
  }

  std::ostream& operator<<(std::ostream& os, const DFS::BootSetting& opt)
  {
    return os << std::dec << value(opt) << " (" << description(opt) << ")";
  }
}
