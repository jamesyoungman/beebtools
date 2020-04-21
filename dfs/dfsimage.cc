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
std::string CatalogEntry::name() const
{
  return ascii7_string(data_ + cat_offset_, 0x07);
}

char CatalogEntry::directory() const
{
  return 0x7F & (data_[cat_offset_ + 0x07]);
}
  
std::string Image::title() const
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


int Image::find_catalog_slot_for_name(const DFSContext& ctx, const std::string& arg) const
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
      const std::string trimmed_name(stringutil::rtrim(entry.name()));

      if (dir != entry.directory())
	{
#if VERBOSE_FOR_TESTS
	  std::cerr << "No match; " << dir << " != " << entry.directory() << "\n";
#endif
	  continue;
	}

      if (!stringutil::case_insensitive_equal(name, trimmed_name))
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

std::pair<const byte*, const byte*> Image::file_body(int slot) const
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

Format identify_format(const std::vector<byte>& image)
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


}  // namespace DFS
