// Tests for image file format probing
#include "identify.h"

#include <algorithm>
#include <iterator>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

#include "abstractio.h"
#include "dfs.h"
#include "dfstypes.h"

using DFS::byte;
using DFS::DataAccess;
using DFS::SectorBuffer;

namespace
{

void dump(std::ostream& os, const DFS::SectorBuffer& data)
{
  constexpr unsigned int block_size = 8u;
  os << std::hex << std::setfill('0') << std::right;
  for (unsigned int block = 0; block < data.size(); block += block_size)
    {
      os << std::setw(3) << block << "| ";
      for (unsigned int offset = 0, addr=block; offset < block_size; ++offset, ++addr)
	{
	  if (offset)
	    os << ' ';
	  os << std::setw(2) << static_cast<unsigned int>(data[addr]);
	}
      os << " | ";
      for (unsigned int offset = 0, addr=block; offset < block_size; ++offset, ++addr)
	{
	  char ch = data[addr];
	  if (!isgraph(static_cast<unsigned char>(ch)))
	    ch = '.';
	  os << ch;
	}
      os << '\n';
    }
}


struct TestImage : public DFS::DataAccess
{
public:
  TestImage(const std::map<DFS::sector_count_type, DFS::SectorBuffer> data, const DFS::Geometry geom)
    : total_sectors_(geom.total_sectors()), content_(data), geom_(geom)
  {
    // Verify that all sector numbers >= 0
    assert(std::all_of(content_.begin(), content_.end(),
		       [](auto it) { return it.first >= 0; }));
    if (!content_.empty())
      {
	// Verify that there are no holes.
	auto last = content_.rbegin()->first;
	assert(last == content_.size() - 1u);
      }
    if (DFS::verbose)
      {
	for (const auto& todo : data)
	  {
	    std::cerr << "TestImage::TestImage(): sector " << todo.first << " is populated:\n";
	    dump(std::cerr, todo.second);
	  }
      }
  }

  virtual ~TestImage() {}

  std::optional<DFS::SectorBuffer> read_block(unsigned long sec) override
  {
    auto it = content_.find(DFS::sector_count(sec));
    if (it == content_.end())
      {
	if (sec < total_sectors_)
	  {
	    // We don't have test data for it, but it is within the bounds
	    // of the device; return a zeroed secctor.
	    if (DFS::verbose)
	      std::cerr << "TestImage::read_block(" << sec << "): returning zeroed sector\n";
	    return DFS::SectorBuffer();
	  }
	if (DFS::verbose)
	  std::cerr << "TestImage::read_block(" << sec << "): out of bounds, returning nothing\n";
	return std::nullopt;
      }
    DFS::SectorBuffer result = it->second;
    if (DFS::verbose)
      {
	std::cerr << "TestImage::read_block(" << sec << "): returning data:\n";
	dump(std::cerr, result);
      }
    return result;
  }

  const DFS::Geometry& geometry() const
  {
    return geom_;
  }

private:
  const DFS::sector_count_type total_sectors_; // may be fewer than in geom_.
  std::map<DFS::sector_count_type, DFS::SectorBuffer> content_;
  DFS::Geometry geom_;
};


struct ImageBuilder
{
public:
  ImageBuilder() {};

  ImageBuilder& with_geometry(const DFS::Geometry& g)
  {
    geom_ = g;
    return *this;
  }

  ImageBuilder& with_sector(DFS::sector_count_type where, const DFS::SectorBuffer& data)
  {
    content_[where] = data;
    return *this;
  }

  ImageBuilder& with_sectors(const std::map<DFS::sector_count_type, DFS::SectorBuffer>& sectors)
  {
    for (const auto& todo : sectors)
      {
	auto [where, what] = todo;
	if (DFS::verbose)
	  std::cerr << "ImageBuilder:with_sectors(): setting sector " << where << "\n";
	content_[where] = what;
      }
    return *this;
  }

  ImageBuilder& with_byte_change(DFS::sector_count_type sec, byte offset, byte value)
  {
    assert(content_.find(sec) != content_.end());
    content_[sec][offset] = value;
    return *this;
  }

  ImageBuilder with_bitmask_change(DFS::sector_count_type sec, byte offset,
				   byte and_bits, byte or_bits)
  {
    assert(content_.find(sec) != content_.end());
    content_[sec][offset] &= and_bits;
    content_[sec][offset] |= or_bits;
    return *this;
  }

  ImageBuilder& with_string(DFS::sector_count_type lba, byte offset, const std::string& s)
  {
    assert(s.size() < DFS::SECTOR_BYTES);
    assert((offset + s.size()) < DFS::SECTOR_BYTES);
    DFS::SectorBuffer& sec = content_[lba];
    for (const char ch : s)
      {
	sec[offset++] = ch;
      }
    return *this;
  }

  TestImage build()
  {
    assert(geom_);
    return TestImage(content_, *geom_);
  }

private:
  std::optional<DFS::Geometry> geom_;
  std::map<DFS::sector_count_type, DFS::SectorBuffer> content_;
};

struct SectorBuilder
{
public:
  SectorBuilder()
  {
    with_fill(byte(0), byte(0), byte(256));
  }

  SectorBuilder& with_fill(byte offset, byte val, byte copies)
  {
    assert((offset + copies) < DFS::SECTOR_BYTES);
    std::fill(data_.begin() + offset,
	      data_.begin() + offset + copies, val);
    return *this;
  }

  void with_byte(byte pos, byte val)
  {
    data_[pos] = val;
  }

  void with_bytes(const std::map<byte, byte> positions_and_values)
  {
    for (const auto todo : positions_and_values)
      {
	auto [where, what] = todo;
	data_[where] = what;
      }
  }

  void with_u16(byte pos, uint16_t val)
  {
    data_[pos] = (val >> 8) && 0xFF;
    ++pos;
    data_[pos] = val && 0xFF;
  }

  DFS::SectorBuffer build() const
  {
    return data_;
  }

private:
  DFS::SectorBuffer data_;
};

struct CatalogBuilder
{
public:
  CatalogBuilder(unsigned int total_sectors, unsigned int fragments = 1u)
  {
    assert(fragments > 0);
    assert(fragments <= 2);
    for (unsigned int i = 0; i < fragments; ++i)
      {
	sectors_[i*2] = DFS::SectorBuffer();
	sectors_[i*2+1] = DFS::SectorBuffer();
      }
    sectors_[1][7] = total_sectors & 0xFF;
    total_sectors >>= 8;
    sectors_[1][6] = total_sectors & 0x03;
    total_sectors >>= 2;
    // Ensure that the original value was representable in the fields available.
    assert(total_sectors == 0);

    if (fragments > 1)
      {
	std::fill(sectors_[2].begin(), sectors_[2].begin()+8, byte(0xAA));
      }
  }

  CatalogBuilder& with_file(unsigned int slot, DFS::sector_count_type start_sec,
			    unsigned int file_len,
			    char dir, const std::string& name,  bool locked,
			    unsigned short load_addr, unsigned short exec_addr)
  {
    assert(slot <= 31);
    std::array<byte, 8> name_bytes, metadata_bytes;
    assert(name.size() <= 7);
    std::fill(name_bytes.begin(), name_bytes.end(), ' ');
    std::copy(name.begin(), name.end(), name_bytes.begin());
    name_bytes[7] = byte(dir) | byte(locked ? 0x80 : 0);
    metadata_bytes[0] = byte(load_addr & 0xFF);
    metadata_bytes[1] = byte((load_addr & 0xFF00) >> 8);
    metadata_bytes[2] = byte(exec_addr & 0xFF);
    metadata_bytes[3] = byte((exec_addr & 0xFF00) >> 8);
    metadata_bytes[4] = byte(file_len & 0xFF);
    metadata_bytes[5] = byte((file_len & 0xFF00) >> 8);
    metadata_bytes[6] =
      byte((start_sec & 0x300) >> 8 |
	   (load_addr & 0x300) >> 6 |
	   (file_len  & 0x300) >> 4 |
	   (exec_addr & 0x300) >> 2);
    metadata_bytes[7] = start_sec & 0xFF;
    unsigned int pos = slot * 8;
    DFS::SectorBuffer& name_sec(sectors_[0]);
    std::copy(name_bytes.begin(), name_bytes.end(),
	      name_sec.begin() + pos);
    DFS::SectorBuffer& metadata_sec(sectors_[1]);
    if (metadata_sec[5] < pos)
      {
	metadata_sec[5] = static_cast<byte>(pos);
      }
    std::copy(metadata_bytes.begin(), metadata_bytes.end(),
	      metadata_sec.begin() + pos);
    return *this;
  }

  const std::map<unsigned int, DFS::SectorBuffer>& build() const
  {
    return sectors_;
  }

private:
  std::map<unsigned int, DFS::SectorBuffer> sectors_;
};

std::map<DFS::sector_count_type, DFS::SectorBuffer> acorn_catalog(DFS::sector_count_type total_sectors)
{
  return CatalogBuilder(total_sectors, 1).build();
}

  struct Example
  {
    Example(std::string lbl, std::optional<DFS::Format> fmt, const TestImage& img)
      : label(lbl), expected_id(fmt), image(img)
    {
    }

    std::string label;
    std::optional<DFS::Format> expected_id;
    TestImage image;
  };

  struct Votes
  {
    Votes(DataAccess& da)
      : total_selected_(0)
    {
      std::optional<DFS::SectorBuffer> sec1 = da.read_block(1);
      DFS::sector_count_type sec;
      if (sec1)
	{
	  is_hdfs = DFS::internal::smells_like_hdfs(*sec1);
	  is_watford = DFS::internal::smells_like_watford(da, *sec1);
	  is_acorn_dfs = DFS::internal::smells_like_acorn_dfs(da, *sec1);
	}
      else
	{
	  is_hdfs = false;
	  is_watford = false;
	  is_acorn_dfs = false;
	}
      is_opus_ddos = DFS::internal::smells_like_opus_ddos(da, &sec);
      total_selected_ += is_acorn_dfs;
      total_selected_ += is_hdfs;
      total_selected_ += is_watford;
      total_selected_ += is_opus_ddos;
    }

    bool exclusive(std::string& error) const
    {
      if (total_selected_ > 1)
	{
	  std::ostringstream ss;
	  ss << "votes were not mutually exclusive: " << to_str();
	  error = ss.str();
	  return false;
	}
      return true;
    }

    bool selected_something() const
    {
      return total_selected_ > 0;
    }

    std::string to_str() const
    {
      std::stringstream ss;
      ss << std::boolalpha
	 << "is_hdfs=" << is_hdfs
	 << ", is_watford=" << is_watford
	 << ", is_opus_ddos=" << is_opus_ddos
	 << ", is_acorn_dfs=" << is_acorn_dfs;
      return ss.str();
    }

    int total_selected_;
    bool is_acorn_dfs;
    bool is_hdfs;
    bool is_watford;
    bool is_opus_ddos;
  };

ImageBuilder near_watford_file_overlap()
{
  // Create an acorn image which cannot be a Watford DFS image
  // because the location otherwise occupied by the extended catalog
  // is occupied by a file which incidentally contains only the
  // Watford marker bytes (the WDFS documentation claims it may
  // itself misidentify such discs).
  const DFS::sector_count_type file_start_sector = 2;
  const unsigned int catalog_slot = 1;
  const int file_len = 256;
  constexpr unsigned short page(0x1900);
  DFS::SectorBuffer file_body = SectorBuilder().with_fill(0, 0xAA, 8).build();
  const std::string file_name("FILEAA");
  auto catalog_sectors = CatalogBuilder(80*10, 1)
    .with_file(catalog_slot, file_start_sector, file_len, '$', file_name, false, page, page)
    .build();
  return ImageBuilder()
    .with_sectors(catalog_sectors)
    // Put the recognition bytes in sector 2.
    .with_sector(file_start_sector, file_body)
    .with_sector(file_start_sector+1, DFS::SectorBuffer());
}

ImageBuilder near_watford_no_recognition()
{
  return ImageBuilder()
    .with_sectors(CatalogBuilder(80*10, 2).build())
    // change one of the recognition bytes.
    .with_byte_change(2, 1, static_cast<byte>('X'));
}

ImageBuilder actual_watford()
{
  return ImageBuilder().with_sectors(CatalogBuilder(80*10, 2).build());
}

ImageBuilder empty_hdfs(int sides)
{
  assert(sides == 1 || sides == 2);
  byte byte6bits23 = 8;
  if (sides == 2)
    byte6bits23 |= 4;
  return ImageBuilder()
    .with_sectors(CatalogBuilder(80*10, 1).build())
    .with_bitmask_change(1, 6, byte(~12u), byte6bits23);
}


  std::vector<Example> make_examples()
  {
    std::vector<Example> result;
    DFS::Geometry fm_40t_ss(40, 1, 10,DFS::Encoding::FM);
    DFS::Geometry fm_80t_ss(80, 1, 10,DFS::Encoding::FM);
    DFS::Geometry mfm_80t_ss(80, 1, 18,DFS::Encoding::FM);
    DFS::Geometry mfm_80t_ds(80, 2, 18,DFS::Encoding::MFM);

    result.push_back(Example("empty", std::nullopt,
			     ImageBuilder()
			     .with_geometry(fm_40t_ss)
			     .build()));
    result.push_back(Example("acorn_ss_40t", DFS::Format::DFS,
			     ImageBuilder()
			     .with_geometry(fm_40t_ss)
			     .with_sectors(acorn_catalog(40*10)).build()));
    result.push_back(Example("acorn_ss_80t", DFS::Format::DFS,
			     ImageBuilder()
			     .with_geometry(fm_80t_ss)
			     .with_sectors(acorn_catalog(80*10)).build()));
    // Full size disc but the catalog says the file system has 0
    // sectors.  Not DFS because the prospective "catalog" says the
    // media too short to contain a the catalog itself.
    result.push_back(Example("acorn_0_sectors", std::nullopt,
			     ImageBuilder()
			     .with_geometry(fm_80t_ss)
			     .with_sectors(acorn_catalog(0)).build()));
    // 1 track disc but the catalog says the file system has 0
    // sectors.  Not DFS because the prospective "catalog" says the
    // media too short to contain a the catalog itself.
    result.push_back(Example("acorn_0_sectors_g1track", std::nullopt,
			     ImageBuilder()
			     .with_geometry(DFS::Geometry(1, 1, 10, DFS::Encoding::FM))
			     .with_sectors(acorn_catalog(0)).build()));
    // Full size disc but the catalog says the file system has 1
    // sector.  Not DFS because the prospective "catalog" says the
    // media too short to contain a the catalog itself.
    result.push_back(Example("acorn_1_sector",
			     std::nullopt,
			     ImageBuilder()
			     .with_geometry(fm_80t_ss)
			     .with_sectors(acorn_catalog(1)).build()));
    // 1 track disc but the catalog says the file system has 1 sector.
    // Not DFS because the prospective "catalog" says the media too
    // short to contain a the catalog itself.
    result.push_back(Example("acorn_1_sector_g1track",
			     std::nullopt,
			     ImageBuilder()
			     .with_geometry(DFS::Geometry(1, 1, 10, DFS::Encoding::FM))
			     .with_sectors(acorn_catalog(1)).build()));
    // 1 track single-density disc and the catalog says the file
    // system has 3 sectors (which is the minimum to feasibly contain
    // file data).
    result.push_back(Example("acorn_3_sector_g1track", DFS::Format::DFS,
			     ImageBuilder()
			     .with_geometry(DFS::Geometry(1, 1, 10, DFS::Encoding::FM))
			     .with_sectors(acorn_catalog(3)).build()));

    for (int last_cat_enty_offset = 1; last_cat_enty_offset < 8; ++last_cat_enty_offset)
      {
	std::ostringstream ss;
	ss << "acorn_bad_entry_offset_" << last_cat_enty_offset;
	result.push_back(Example(ss.str(), std::nullopt,
				 ImageBuilder()
				 .with_geometry(fm_80t_ss)
				 .with_sectors(CatalogBuilder(80*10).build())
				 // This image cannot be valid as the "offset
				 // of last catalog entry" byte is not a
				 // multiple of 8.
				 .with_byte_change(1, 5 /* not multiple of 8 */, 1)
				 .build()));
      }

    result.push_back(Example("file_at_s2_fm", DFS::Format::DFS,
			     near_watford_file_overlap()
			     .with_geometry(fm_80t_ss)
			     .build()));
    result.push_back(Example("file_at_s2_mfm", DFS::Format::DFS,
			     near_watford_file_overlap()
			     .with_geometry(mfm_80t_ss)
			     .build()));
    result.push_back(Example("watford_empty", DFS::Format::WDFS,
			     actual_watford()
			     .with_geometry(fm_80t_ss)
			     .build()));
    result.push_back(Example("no_wdfs_recog", DFS::Format::DFS,
			     near_watford_no_recognition()
			     .with_geometry(fm_80t_ss)
			     .build()));
    result.push_back(Example("empty_hdfs_1s", DFS::Format::HDFS,
			     empty_hdfs(1)
			     .with_geometry(fm_80t_ss)
			     .build()));
    result.push_back(Example("empty_hdfs_2s", DFS::Format::HDFS,
			     empty_hdfs(2)
			     .with_geometry(mfm_80t_ds)
			     .build()));

    std::set<std::string> labels;
    for (const auto& ex : result)
      {
	if (labels.find(ex.label) != labels.end())
	  {
	    std::cerr << "duplicate label " << ex.label << "\n";
	    abort();
	  }
	labels.insert(ex.label);
      }
    return result;
  }


bool check_votes(const Votes& v, std::optional<DFS::Format> expected_id)
{
  std::string error;
  if (!v.exclusive(error))
    {
      std::cerr <<  "identified as being more than one thing: "
		<< v.to_str() << ": " << error << "\n";
      return false;
    }

  if (!expected_id)
    {
      std::cerr << "expected not to be identifiable: ";
      if (v.selected_something())
	{
	  std::cerr << "identified as " << v.to_str() << " but expected format was [unknown]\n";
	  return false;
	}
      return true;
    }

  std::cerr << " expected format was " << *expected_id << ": ";

  if (!v.selected_something())
    {
      std::cerr << "not identified: FAIL\n";
      return false;
    }
  if (v.is_hdfs && *expected_id != DFS::Format::HDFS)
    {
      std::cerr << "identified as HDFS: FAIL\n";
      return false;
    }
  if (v.is_watford && expected_id != DFS::Format::WDFS)
    {
      std::cerr << "identified as Watford DFS: FAIL\n";
      return false;
    }
  if (v.is_opus_ddos && expected_id != DFS::Format::OpusDDOS)
    {
      std::cerr << "identified as Opus DDOS: FAIL\n";
      return false;
    }
  if (v.is_acorn_dfs && expected_id != DFS::Format::DFS)
    {
      std::cerr << "identified as Acorn DFS: FAIL\n";
      return false;
    }
  return true;
}

  bool test_id_exclusive_and_exhaustive()
  {
    bool all_ok = true;
    auto examples = make_examples();
    size_t longest_label_len = 0u;
    for (auto& ex : examples)
      {
	longest_label_len = std::max(longest_label_len, ex.label.size());
      }
    if (longest_label_len > std::numeric_limits<int>::max())
      longest_label_len = std::numeric_limits<int>::max();
    for (auto& ex : examples)
      {
	std::cerr << "image " << std::setw(static_cast<int>(longest_label_len)) << ex.label << ": ";

	// Run the individual recognisers.
	Votes v(ex.image);
	std::string error;
	if (!check_votes(v, ex.expected_id))
	  {
	    all_ok = false;
	    continue;
	  }

	std::optional<DFS::Format> fmt;
	try
	  {
	    fmt = DFS::identify_file_system(ex.image, ex.image.geometry(), false, error);
	    if (fmt)
	      {
		std::cerr << "identify_file_system returned " << (*fmt) << ": ";
	      }
	    else
	      {
		std::cerr << "identify_file_system failed (" << error << "): ";
	      }
	  }
	catch (std::exception& e)
	  {
	    std::cerr << "(caught exception " << e.what() << " while trying to identify file system): ";
	    fmt = std::nullopt;
	  }
	if (ex.expected_id)
	  {
	    if (fmt != *ex.expected_id)
	      {
		std::cerr << ": FAIL\n";
		all_ok = false;
		continue;
	      }
	  }
	else
	  {
	    if (fmt)
	      {
		std::cerr << "but wasn't supposed to be recognisable: FAIL\n";
		all_ok = false;
		continue;
	      }
	  }
	std::cerr << ": PASS\n";
      }
    return all_ok;
  }

}  // namespace

int main(int, char *[])
{
  for (bool verbose : {false, true})
    {
      DFS::verbose = verbose;
      if (!test_id_exclusive_and_exhaustive())
	{
	  std::cerr << "At least one test failed; see above for details.\n";
	  return 1;
	}
    }
  std::cout << "All tests passed.\n";
  return 0;
}
