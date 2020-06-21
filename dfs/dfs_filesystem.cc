#include "dfs_filesystem.h"

#include <assert.h>

#include <algorithm>
#include <exception>
#include <iomanip>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

#include "dfs_format.h"
#include "fsp.h"
#include "stringutil.h"

using std::string;

namespace
{
  template <typename Iter>
  std::string ascii7_string(Iter begin, Iter end)
  {
    string result;
    result.reserve(end-begin);
    std::transform(begin, end,
		   std::back_inserter(result),
		   DFS::stringutil::byte_to_ascii7);
    return result;
  }

  std::map<std::optional<char>, std::unique_ptr<DFS::Volume>>
  init_volumes(DFS::DataAccess& media, DFS::Format fmt, DFS::Geometry)
  {
    if (fmt == DFS::Format::OpusDDOS)
      throw DFS::OpusUnsupported();	// TODO: implement Opus DDOS support
    std::map<std::optional<char>, std::unique_ptr<DFS::Volume>> result;
    auto vol = std::make_unique<DFS::Volume>(fmt, media);
    result.insert(std::make_pair(DFS::FileSystem::DEFAULT_VOLUME, std::move(vol)));
    return result;
  }

}  // namespace

namespace DFS
{
  Volume::Volume(DFS::Format format, DataAccess& media)
    : media_(media), root_(std::make_unique<Catalog>(format, media))
  {
    if (format == Format::OpusDDOS)
      {
	// TODO: implement origin for Opus DDOS.  In Opus DDOS, all
	// the catalogs are in track 0, and all the files are stored
	// within track-aligned volumes.  Sector positions in the
	// catalog are relative to the beginning of the first track of
	// the volume (which is not where the catalog is).  So to
	// support Opus DDOS volumes we need to have some way to
	// indicate the "origin" of the sector numbers in the catalog
	// entries.
	throw OpusUnsupported();
      }
  }

  const Catalog& Volume::root() const
  {
    return *root_;
  }

  FileSystem::FileSystem(DataAccess& media,
			 DFS::Format fmt,
			 DFS::Geometry geom)
    : format_(fmt), geometry_(geom), media_(media),
      volumes_(init_volumes(media, fmt, geom))
  {
    const DFS::byte byte106 = get_byte(1, 0x06);

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
    if (byte106 & 8)
      {
	assert(disc_format() == Format::HDFS);
      }
    else
      {
	// TODO: HDFS uses the same on-disk catalog format for
	// subdirectories, but it's possible that this bit is only set
	// in the root.  So, it's possible that this assertion may
	// fire for non-root HDFS directories.
	assert(disc_format() != Format::HDFS);
	if (byte106 & 4)
	  {
	    // Watford large disk; TODO: decide whether the Format
	    // enum should distinguish those.
	    assert(disc_format() == Format::WDFS);
	  }
	else
	  {
	    assert(disc_format() == Format::WDFS ||
		   disc_format() == Format::DFS ||
		   disc_format() == Format::OpusDDOS);
	  }
      }
  }

  DFS::UiStyle FileSystem::ui_style(const DFSContext& ctx) const
  {
    if (ctx.ui_ != DFS::UiStyle::Default)
      return ctx.ui_;
    switch (disc_format())
      {
      case Format::HDFS:
	// There appear to be some differences in UI between HDFS and
	// Acorn, but I don't know what they are in detail.  So for
	// the time being, follow Acorn.
	return DFS::UiStyle::Acorn;
      case Format::DFS:
	return DFS::UiStyle::Acorn;
      case Format::WDFS:
	return DFS::UiStyle::Watford;
      case Format::Solidisk:
	// TODO: are there UI differences?
	return DFS::UiStyle::Acorn;
      case Format::OpusDDOS:
	return DFS::UiStyle::Opus;
      }
    // TODO: find out if there are differences for Opus, too.
    return DFS::UiStyle::Acorn;
  }

  byte FileSystem::get_byte(sector_count_type sector, unsigned offset) const
  {
    assert(offset < DFS::SECTOR_BYTES);
    auto got = media_.read_block(sector);
    if (!got)
      throw eof_in_catalog();
    return (*got)[offset];
  }

  Geometry FileSystem::geometry() const
  {
    return geometry_;
  }

  DataAccess& FileSystem::device() const
  {
    return media_;
  }

  sector_count_type FileSystem::disc_sector_count() const
  {
    if (disc_format() == DFS::Format::OpusDDOS)
      throw OpusUnsupported();	// TODO: implement Opus DDOS support
    return root().total_sectors();
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


const Catalog& FileSystem::root() const
{
  if (disc_format() == DFS::Format::OpusDDOS)
    throw OpusUnsupported();	// TODO: implement Opus DDOS support
  auto it = volumes_.find('A');
  assert(it != volumes_.end());
  return it->second->root();
}

Volume* FileSystem::mount() const
{
  return mount(DEFAULT_VOLUME);
}


Volume* FileSystem::mount(char key) const
{
  auto it = volumes_.find(key);
  if (it == volumes_.end())
    return 0;
  return it->second.get();
}

std::string format_name(Format f)
{
  switch (f)
    {
    case Format::HDFS: return "HDFS";
    case Format::DFS: return "Acorn DFS";
    case Format::WDFS: return "Watford DFS";
    case Format::Solidisk: return "Solidisk DFS";
    case Format::OpusDDOS: return "Opus DDOS";
    }
  abort();
}

// Returns true if the format is double-sided.  that is to say, the
// "total sectors" field of the catalog includes the sectors on both
// sides.
bool single_sided_filesystem(Format)
{
  // No double-sided formats are supported yet.  HDFS can, apparently,
  // be double-sided.
  return true;
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
