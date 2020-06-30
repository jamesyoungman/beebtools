// Tests for image file format probing
#include "identify.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

#include "abstractio.h"
#include "cleanup.h"
#include "dfs.h"
#include "dfstypes.h"

using DFS::byte;
using DFS::DataAccess;
using DFS::SectorBuffer;

namespace
{
  bool dump_sectors = false;

  bool want(const std::string& label, const std::set<std::string>& only)
  {
    if (only.empty())
      return true;
    return only.find(label) != only.end();
  }

void dump(std::ostream& os, const DFS::SectorBuffer& data)
{
  ostream_flag_saver flag_saver(os);
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

bool match_any_probed_geom_or_none(std::optional<const DFS::Geometry>)
{
  return true;
}

DFS::SectorBuffer zeroed_sector()
{
  DFS::SectorBuffer result;
  result.fill(0);
  return result;
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
    // It's OK for there to be holes.  Sectors for which we have no
    // reocorded data return all-zero.
    if (DFS::verbose && dump_sectors)
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
	    return zeroed_sector();
	  }
	if (DFS::verbose)
	  std::cerr << "TestImage::read_block(" << sec << "): out of bounds, returning nothing\n";
	return std::nullopt;
      }
    DFS::SectorBuffer result = it->second;
    if (DFS::verbose && dump_sectors)
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

  ImageBuilder& with_le_word_change(DFS::sector_count_type sec, byte offset, unsigned short word)
  {
    assert(content_.find(sec) != content_.end());
    assert(offset < 255);
    content_[sec][offset] = byte(word & 0xFF);
    content_[sec][offset+1] = byte((word >> 8) & 0xFF);
    return *this;
  }

  ImageBuilder& with_be_word_change(DFS::sector_count_type sec, byte offset, unsigned short word)
  {
    assert(content_.find(sec) != content_.end());
    assert(offset < 255);
    content_[sec][offset] = byte((word >> 8) & 0xFF);
    content_[sec][offset+1] = byte(word & 0xFF);
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
    data_.fill(byte(0));
  }

  SectorBuilder& with_fill(byte offset, byte val, byte copies)
  {
    assert((offset + copies) < DFS::SECTOR_BYTES);
    std::fill(data_.begin() + offset,
	      data_.begin() + offset + copies, val);
    return *this;
  }

  SectorBuilder& with_byte(byte pos, byte val)
  {
    data_[pos] = val;
    return *this;
  }

  SectorBuilder& with_bytes(const std::map<byte, byte> positions_and_values)
  {
    for (const auto todo : positions_and_values)
      {
	auto [where, what] = todo;
	data_[where] = what;
      }
    return *this;
  }

  SectorBuilder& with_u16(byte pos, uint16_t val)
  {
    data_[pos] = (val >> 8) && 0xFF;
    ++pos;
    data_[pos] = val && 0xFF;
    return *this;
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
  CatalogBuilder(unsigned int orig_sectors,
		 unsigned int fragments = 1u,
		 DFS::sector_count_type catalog_origin = 0)
    : catalog_origin_(catalog_origin)
  {
    unsigned int total_sectors = orig_sectors;
    assert(fragments > 0);
    assert(fragments <= 2);
    for (unsigned int i = 0; i < fragments; ++i)
      {
	sectors_[catalog_origin + i*2] = zeroed_sector();
	sectors_[catalog_origin + i*2+1] = zeroed_sector();
      }
    sectors_[catalog_origin + 1][7] = total_sectors & 0xFF;
    total_sectors >>= 8;
    sectors_[catalog_origin + 1][6] = total_sectors & 0x03;
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
    DFS::SectorBuffer& name_sec(sectors_[catalog_origin_ + 0]);
    std::copy(name_bytes.begin(), name_bytes.end(),
	      name_sec.begin() + pos);
    DFS::SectorBuffer& metadata_sec(sectors_[catalog_origin_ + 1]);
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
  DFS::sector_count_type catalog_origin_;
  std::map<unsigned int, DFS::SectorBuffer> sectors_;
};

std::map<DFS::sector_count_type, DFS::SectorBuffer> acorn_catalog(DFS::sector_count_type total_sectors)
{
  return CatalogBuilder(total_sectors, 1).build();
}


struct VolumeConfig
{
public:
  VolumeConfig(DFS::sector_count_type start,
	       DFS::sector_count_type track_count)
    : start_track(start), end_track(start + track_count)
  {
  }

  unsigned int total_sectors(const DFS::Geometry& geom) const
  {
    return (end_track - start_track) * geom.sectors;
  }

  const DFS::sector_count_type start_track;
  const DFS::sector_count_type end_track;
};

// empty_opus generates an Opus DDOS image with two volumes (two
// because just one cannot extend over the whole disc) but no files.
ImageBuilder
empty_opus(const DFS::Geometry& geom)
{
  assert(geom.sectors > 10);	// DDOS only on double density discs.
  assert(geom.sectors <= std::numeric_limits<byte>::max());
  return ImageBuilder()
    .with_geometry(geom)
    // Sector 16 contains the volume catalogue.
    .with_sector(16,
		 SectorBuilder()
		 .with_byte(0, 0x20) // config/version number
		 .with_byte(1, 0x05) // sectors on disk (0x5A0=1440), high byte
		 .with_byte(2, 0xA0) // sectors on disk (0x5A0=1440), low byte
		 .with_byte(3, static_cast<byte>(geom.sectors))
		 .with_byte(4, 0xFF) // tracks on this disk (saw 0xFF)
		 // vol A starts at track 1 and ends at the end of
		 // track 57 (the start point of vol B).  This gives
		 // 57 tracks, that is 0x402 available sectors.
		 // However, the Acorn catalog format only allows 10
		 // bits for sector count, so we can only use
		 // 0x3FF=1023 of those sectors.
		 .with_byte(8, 0x01)
		 // vol B starts at track 57 = (0x39) and is the last
		 // volume.  It extends up to track 79, giving 23
		 // tracks, hence 414=0x19E sectors.  Of that we use
		 // 0x18C sectors (22 tracks).
		 .with_byte(10, 0x39)
		 .build())
    // The catalog of volume A occupies disc sectors 0 and 1.
    .with_sector(1,
		 SectorBuilder()
		 .with_byte(6, 0x03) // b0/1: sector count bits b8,b9
		 .with_byte(7, 0xF0) // sector count buts b0-b7
		 .with_byte(5, 0)    // 0 entries in catalogue A
		 .build())
    // The catalog of volume B occupies disc sectors 2 and 3.
    .with_sector(3,
		 SectorBuilder()
		 .with_byte(6, 0x01) // b0/1: sector count bits b8,b9
		 .with_byte(7, 0x8C) // sector count buts b0-b7
		 .with_byte(5, 0)   // 0 entries in catalogue B
		 .build());
}

//opus_with_zero_volumes generates a disc image which is structurally
// similar to an empty Opus DDOS disc image, but contains zero volumes
// in the disc catalog.  Hence it's not really a valid Opus DDOS image
// (but should be a valid Acorn DFS image).
ImageBuilder
opus_with_zero_volumes(const DFS::Geometry& geom)
{
  return empty_opus(geom)
    .with_byte_change(16, 8, 0)	// no volume A
    .with_byte_change(16, 10, 0) // no volume B
    .with_byte_change(16, 12, 0) // no volume C
    .with_byte_change(16, 14, 0) // no volume D
    .with_byte_change(16, 16, 0) // no volume E
    .with_byte_change(16, 18, 0) // no volume F
    .with_byte_change(16, 20, 0) // no volume G
    .with_byte_change(16, 22, 0); // no volume H
}

  struct Example
  {
    using geometry_matcher = std::function<bool(std::optional<DFS::Geometry>)>;

    Example(std::string file_name, std::optional<DFS::Format> fmt,
	    const TestImage& img,
	    std::optional<geometry_matcher> gmatcher = std::nullopt)
      : file_name_(file_name), expected_id(fmt), image(img), geom_matcher_(gmatcher)
    {
    }

    const std::string label() const
    {
      return file_name_;
    }

    const std::string file_name() const
    {
      return file_name_;
    }

    bool matches_geometry(std::optional<const DFS::Geometry> guess)
    {
      // The geom_matcher_ member is itself optional, but if it is set,
      // the function's sole argument is also optional.
      if (geom_matcher_)
	{
	  return (*geom_matcher_)(guess);
	}
      if (!guess)
	{
	  // Because of the limitations of the simple sector-dump
	  // image file formats, there is no separate format/geometry
	  // information other than the file extension, so often when
	  // we cannot probe the format of the file system within the
	  // image, there is not enough information to figure out the
	  // geometry of the imaged disc.  Hence when expected_id is
	  // null (i.e. we could not discover the filesystem type), we
	  // consider it OK for the geometry probe also to fail.
	  return !expected_id;
	}
      return image.geometry() == guess;
    }

    std::string file_name_;
    std::optional<DFS::Format> expected_id;
    TestImage image;
    std::optional<geometry_matcher> geom_matcher_;
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
    .with_sector(file_start_sector+1, zeroed_sector());
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
    DFS::Geometry fm_40t_ss(40, 1, 10, DFS::Encoding::FM);
    DFS::Geometry fm_80t_ss(80, 1, 10, DFS::Encoding::FM);
    DFS::Geometry mfm_40t_ss(40, 1, 18, DFS::Encoding::MFM);
    DFS::Geometry mfm_80t_ss(80, 1, 18, DFS::Encoding::MFM);
    DFS::Geometry mfm_80t_ds(80, 2, 18, DFS::Encoding::MFM);

    result.push_back(Example("no_sectors_at_all.ssd", std::nullopt,
			     ImageBuilder()
			     .with_geometry(DFS::Geometry(0, 1, 10, DFS::Encoding::FM))
			     .build()));
    result.push_back(Example("just_one_sector.ssd", std::nullopt,
			     ImageBuilder()
			     .with_geometry(DFS::Geometry(1, 1, 1, DFS::Encoding::FM))
			     .with_sector(0, SectorBuilder().build())
			     .build()));
    result.push_back(Example("blank_40t.ssd", std::nullopt,
			     ImageBuilder()
			     .with_geometry(fm_40t_ss)
			     .build()));
    result.push_back(Example("acorn_ss_40t.ssd", DFS::Format::DFS,
			     ImageBuilder()
			     .with_geometry(fm_40t_ss)
			     .with_sectors(acorn_catalog(40*10)).build()));
    result.push_back(Example("acorn_ss_80t.ssd", DFS::Format::DFS,
			     ImageBuilder()
			     .with_geometry(fm_80t_ss)
			     .with_sectors(acorn_catalog(80*10)).build()));
    // Full size disc but the catalog says the file system has 0
    // sectors.  Not DFS because the prospective "catalog" says the
    // media too short to contain a the catalog itself.
    result.push_back(Example("acorn_0_sectors.ssd", std::nullopt,
			     ImageBuilder()
			     .with_geometry(fm_80t_ss)
			     .with_sectors(acorn_catalog(0)).build()));
    // 1 track disc but the catalog says the file system has 0
    // sectors.  Not DFS because the prospective "catalog" says the
    // media too short to contain a the catalog itself.
    result.push_back(Example("acorn_0_sectors_g1track.ssd", std::nullopt,
			     ImageBuilder()
			     .with_geometry(DFS::Geometry(1, 1, 10, DFS::Encoding::FM))
			     .with_sectors(acorn_catalog(0)).build()));
    // Full size disc but the catalog says the file system has 1
    // sector.  Not DFS because the prospective "catalog" says the
    // media too short to contain a the catalog itself.
    result.push_back(Example("acorn_1_sector.ssd",
			     std::nullopt,
			     ImageBuilder()
			     .with_geometry(fm_80t_ss)
			     .with_sectors(acorn_catalog(1)).build()));
    // 1 track disc but the catalog says the file system has 1 sector.
    // Not DFS because the prospective "catalog" says the media too
    // short to contain a the catalog itself.
    result.push_back(Example("acorn_1_sector_g1track.ssd",
			     std::nullopt,
			     ImageBuilder()
			     .with_geometry(DFS::Geometry(1, 1, 10, DFS::Encoding::FM))
			     .with_sectors(acorn_catalog(1)).build()));
    // Two-sector single-density disc and the catalog says the file
    // system has 2 sectors.  The media is not physically large enough
    // to contain a Watford DFS extended catalog.  But this is also not
    // a valid Acorn DFS filesystem, as there is not enough space for a
    // 1-byte file.
    result.push_back(Example("2_phys_sector.ssd", std::nullopt,
			     ImageBuilder()
			     .with_geometry(DFS::Geometry(1, 1, 2, DFS::Encoding::FM))
			     .with_sectors(acorn_catalog(2)).build()));

    // 1 track single-density disc and the catalog says the file
    // system has 3 sectors (which is the minimum to feasibly contain
    // file data).
    result.push_back(Example("acorn_3_sector_g1track.ssd", DFS::Format::DFS,
			     ImageBuilder()
			     .with_geometry(DFS::Geometry(1, 1, 10, DFS::Encoding::FM))
			     // The physical geometry of the disc is not a standard format
			     // so for now accept any guessed geometry.
			     .with_sectors(acorn_catalog(3)).build(),
			     match_any_probed_geom_or_none));


    for (int last_cat_enty_offset = 1; last_cat_enty_offset < 8; ++last_cat_enty_offset)
      {
	std::ostringstream ss;
	ss << "acorn_bad_entry_offset_" << last_cat_enty_offset << ".ssd";
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

    result.push_back(Example("file_at_s2_fm.ssd", DFS::Format::DFS,
			     near_watford_file_overlap()
			     .with_geometry(fm_80t_ss)
			     .build()));
    result.push_back(Example("file_at_s2_mfm.sdd", DFS::Format::DFS,
			     near_watford_file_overlap()
			     .with_geometry(mfm_80t_ss)
			     .build()));
    result.push_back(Example("watford_empty.ssd", DFS::Format::WDFS,
			     actual_watford()
			     .with_geometry(fm_80t_ss)
			     .build()));
    result.push_back(Example("no_wdfs_recog.ssd", DFS::Format::DFS,
			     near_watford_no_recognition()
			     .with_geometry(fm_80t_ss)
			     .build()));
    result.push_back(Example("empty_hdfs_1s.ssd", DFS::Format::HDFS,
			     empty_hdfs(1)
			     .with_geometry(fm_80t_ss)
			     .build()));
    result.push_back(Example("empty_hdfs_2s.sdd", DFS::Format::HDFS,
			     empty_hdfs(2)
			     .with_geometry(mfm_80t_ds)
			     .build(),
			     // TODO: improve support for two-sided file systems.
			     match_any_probed_geom_or_none));
    result.push_back(Example("empty_opus_ddos.sdd", DFS::Format::OpusDDOS,
			     empty_opus(mfm_80t_ss)
			     .with_geometry(mfm_80t_ss)
			     .build()));
    result.push_back(Example("opus_zero_volumes.sdd", DFS::Format::DFS,
			     opus_with_zero_volumes(mfm_80t_ss)
			     .with_geometry(mfm_80t_ss)
			     .build()));
    result.push_back(Example("empty_opus_zero_td.sdd",
			     // Detected as Acorn as the Opus Volume catalog is invalid.
			     DFS::Format::DFS,
			     empty_opus(mfm_80t_ss)
			     .with_geometry(mfm_80t_ss)
			     .with_le_word_change(16, 1, 0) // set total sectors to 0.
			     .build()));
    result.push_back(Example("opus_short_720.sdd",
			     // Detected as Acorn as the Opus volume
			     // catalog says that there are 1440
			     // sectors (80 tracks), but the media
			     // only has 720 (40 tracks).
			     DFS::Format::DFS,
			     empty_opus(mfm_40t_ss)
			     .with_geometry(mfm_40t_ss)
			     // Update sector count in volume catalog.
			     .with_le_word_change(16, 1, 1440)
			     // Update total sectors field of catalog
			     // (to be a reasonable value).
			     .with_byte_change(1, 7, mfm_40t_ss.total_sectors() & 0xFF)
			     .with_byte_change(1, 6, (mfm_40t_ss.total_sectors() >> 8) & 0x3)
			     .build()));
    result.push_back(Example("empty_opus_bad_cat_b.sdd",
			     // Detected as Acorn as the catalog for volume B
			     // is invalid.
			     DFS::Format::DFS,
			     empty_opus(mfm_80t_ss)
			     .with_geometry(mfm_80t_ss)
			     .with_string(0, 8, "FNAME") // file name
			     .with_byte_change(0, 0x0F, '$') // dir
			     .with_byte_change(1, 0x0F, 20) // start sector
			     /* Opus and Acorn have different origins for file start sector,
				so make sure the file body is reasonable for either case. */
			     .with_sector(20, SectorBuilder().with_byte(0, 0x0D).build()) // body (Acorn)
			     .with_sector(20+18, SectorBuilder().with_byte(0, 0x0D).build()) // body (Opus)
			     /* It's this change that makes the catalog invalid. */
			     .with_byte_change(3, 5, 7 /* not multiple of 8 */)
			     /* Give the file a valid load address and length */
			     .with_le_word_change(3, 8, 0xFFFF) // load address
			     .with_le_word_change(3, 0x0C, 1) // file len
			     .build()));
    result.push_back(Example("opus_1439.sdd",
			     // Not Opus because wrong total sectors in sector 16.
			     DFS::Format::DFS,
			     empty_opus(mfm_80t_ss)
			     .with_geometry(mfm_80t_ss)
			     // No need to adjust the catalog for consistency as we are not
			     // changing the size of any volume.
			     .with_be_word_change(16, 1, 1439) // total sectors, not 1440
			     .build()));

    std::set<std::string> labels;
    for (const auto& ex : result)
      {
	if (labels.find(ex.label()) != labels.end())
	  {
	    std::cerr << "duplicate label " << ex.label() << "\n";
	    abort();
	  }
	labels.insert(ex.label());
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

  bool test_id_exclusive_and_exhaustive(const std::set<std::string>& only)
  {
    bool all_ok = true;
    auto examples = make_examples();
    size_t longest_label_len = 0u;
    for (auto& ex : examples)
      {
	if (!want(ex.label(), only)) continue;
	longest_label_len = std::max(longest_label_len, ex.label().size());
      }
    if (longest_label_len > std::numeric_limits<int>::max())
      longest_label_len = std::numeric_limits<int>::max();
    for (auto& ex : examples)
      {
	if (!want(ex.label(), only)) continue;
	std::cerr << "format probe: " << std::setw(static_cast<int>(longest_label_len)) << ex.label() << ": ";

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

bool test_geometry_prober(const std::set<std::string>& only)
{
  bool all_ok = true;
  auto examples = make_examples();
  size_t longest_label_len = 0u;
  size_t test_count = 0;
  for (auto& ex : examples)
    {
      if (!want(ex.label(), only)) continue;
      ++test_count;
      longest_label_len = std::max(longest_label_len, ex.label().size());
    }
  if (longest_label_len > std::numeric_limits<int>::max())
    longest_label_len = std::numeric_limits<int>::max();

  std::cerr << std::setfill('=') << std::setw(60) << "" << std::setfill(' ') << "\n"
	    << "running " << test_count << " tests:\n";
  for (auto& ex : examples)
    {
      if (!want(ex.label(), only)) continue;
      bool test_result;

      auto intro = [longest_label_len, &ex]()
		   {
		     std::cerr << "geometry probe test: "
			       << std::setw(static_cast<int>(longest_label_len)) << ex.label() << ": ";
		   };
      auto describe_fs = [](std::ostream& os, std::optional<DFS::Format> f)
		    {
		      os << "file system is ";
		      if (f)
			os << *f;
		      else
			os << "undecided";
		    };
      intro();

      describe_fs(std::cerr, ex.expected_id);
      std::cerr << ", actual geometry: " << ex.image.geometry().to_str() << "\n";

      std::string error;
      std::optional<DFS::ImageFileFormat> probed = identify_image(ex.image, ex.file_name(), error);
      if (DFS::verbose)
	{
	  intro();
	  std::cerr << "actual geometry: " << ex.image.geometry().to_str();
	}
      intro();
      std::optional<DFS::Geometry> probed_geometry;
      if (probed)
	probed_geometry = probed->geometry;

      if (probed)
	{
	  std::cerr << "guessed " << probed_geometry->to_str();
	}
      else
	{
	  std::cerr << "unable to guess geometry (" << error << ")";
	}
      test_result = ex.matches_geometry(probed_geometry);
      if (!test_result)
	all_ok = false;
      std::cerr << "\n";
      intro();
      std::cerr << (test_result ? "PASS\n\n" : "FAIL\n\n");
    }
  return all_ok;
}



}  // namespace

int main(int argc, char *argv[])
{
  // Specify test labels on the command line to run just those.
  std::set<std::string> only;
  for (int i = 1; i < argc; ++i)
    {
      only.insert(argv[i]);
    }

  // dump_sectors is off by default because it generates so much output.
  const std::string env_var("TEST_PROBER_DUMP_SECTORS");
  if (getenv(env_var.c_str()))
    {
      dump_sectors = true;
    }
  else
    {
      std::cerr << "Environment variable " << env_var << " is not set, "
		<< "will not dump the content of sectors we create or read\n";
      dump_sectors = false;
    }

  bool all_ok = true;
  // Run with verbose=true first in case we have an assertion error.
  for (bool verbose : {true, false})
    {
      DFS::verbose = verbose;
      if (!test_id_exclusive_and_exhaustive(only))
	all_ok = false;
      if (!test_geometry_prober(only))
	all_ok = false;
    }
  if (!all_ok)
    {
      std::cerr << "At least one test failed; see above for details.\n";
      return 1;
    }
  std::cout << "All tests passed.\n";
  return 0;
}
