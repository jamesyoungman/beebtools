#include "dfsimage.h"

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

  std::string ascii7_string(std::vector<DFS::byte>::const_iterator begin,
			    std::vector<DFS::byte>::size_type len)
  {
    string result;
    result.reserve(len);
    std::transform(begin, begin+len,
		   std::back_inserter(result), byte_to_ascii7);
    return result;
  }
}  // namespace

namespace DFS
{

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


CatalogEntry::CatalogEntry(std::vector<byte>::const_iterator image_data, int slot,
			   Format fmt)
  : data_(image_data), cat_offset_(calc_cat_offset(slot, fmt)), fmt_(fmt)
{
}

CatalogEntry::CatalogEntry(const CatalogEntry& other)
  : data_(other.data_), cat_offset_(other.cat_offset_), fmt_(other.fmt_)
{
}

std::string CatalogEntry::name() const
{
  return ascii7_string(data_ + cat_offset_, 0x07);
}

char CatalogEntry::directory() const
{
  return 0x7F & (data_[cat_offset_ + 0x07]);
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


std::string FileSystemImage::title() const
{
  std::vector<byte> title_data;
  title_data.resize(12);
  std::copy(img_.begin(), img_.begin()+8, title_data.begin());
  std::copy(img_.begin() + 0x100, img_.begin()+0x104, title_data.begin() + 8);
  std::string result;
  result.resize(12);
  std::transform(title_data.begin(), title_data.end(), result.begin(), byte_to_ascii7);
  // TODO: rtrim here?
  return result;
}

  offset FileSystemImage::end_of_catalog() const
  {
    return img_[0x105];
  }

  int FileSystemImage::catalog_entry_count() const
  {
    auto end_of_catalog = img_[0x105];
    int count = end_of_catalog / 8;
    if (disc_format_ == Format::WDFS)
      {
	count += img_[0x305] / 8;
      }
    return count;
  }

int FileSystemImage::find_catalog_slot_for_name(const DFSContext& ctx, const ParsedFileName& name) const
{
  const int entries = catalog_entry_count();
  for (int i = 1; i <= entries; ++i)
    {
      if (get_catalog_entry(i).has_name(name))
	return i;
    }
  return -1;
}

std::pair<const byte*, const byte*> FileSystemImage::file_body(int slot) const
{
  if (slot < 1 || slot > catalog_entry_count())
    throw std::range_error("catalog slot is out of range");

  auto entry = get_catalog_entry(slot);
  auto offset = static_cast<unsigned long>(SECTOR_BYTES) * entry.start_sector();
  auto length = entry.file_length();
  if (length > img_.size())
    throw BadFileSystemImage("file size for catalog entry is larger than the disk image");
  if (offset > (img_.size() - length))
    throw BadFileSystemImage("file extends beyond the end of the disk image");
  const byte* start = img_.data() + offset;
    return std::make_pair(start, start + length);
}

Format identify_format(const std::vector<byte>& image)
{
  if (image[0x106] & 8)
    return Format::HDFS;

  // DFS provides 31 file slots, and Watford DFS 62.  Watford DFS does
  // this by doubling the size of the catalog into sectors 2 and 3 (as
  // well as DFS's 0 and 1).  It puts recognition bytes in sector 2.
  // However, it's possible for a DFS-format file to contain the
  // recognition bytes in its body.  We don't want to be fooled if
  // that happens.  To avoid it, we check whether the body of any file
  // (of the standard DFS 31 files) starts in sector 2.  If so, this
  // cannot be a Watford DFS format disc.
  const byte last_catalog_entry_pos = image[0x105];
  unsigned pos = 8;
  for (pos = 8; pos <= last_catalog_entry_pos; pos += 8)
    {
      auto start_sector = image[pos + 7];
      if (start_sector == 2)
	{
	  /* Sector 2 is used by a file, so not Watford DFS. */
	  return Format::DFS;
	}
    }

  // Look for the Watford DFS recognition string
  // in the initial entry in its extended catalog.
  if (std::all_of(image.begin()+0x200, image.begin()+0x208,
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
