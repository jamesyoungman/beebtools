#include "dfs_filesystem.h"

#include <assert.h>

#include <algorithm>
#include <exception>
#include <iomanip>
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
}  // namespace

namespace DFS
{

  FileSystem::FileSystem(AbstractDrive *drive,
			 DFS::Format fmt,
			 DFS::sector_count_type /* total sectors */)
    : format_(fmt), media_(drive),
      root_(std::make_unique<Catalog>(format_, drive))
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
	    // Watford large disk
	    assert(disc_format() == Format::WDFS);
	  }
	else
	  {
	    // Either Acorn or Watford DFS.
	    assert(disc_format() == Format::WDFS || disc_format() == Format::DFS);
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
      }
    // TODO: find out if there are differences for Opus, too.
    return DFS::UiStyle::Acorn;
  }

  byte FileSystem::get_byte(sector_count_type sector, unsigned offset) const
  {
    assert(offset < DFS::SECTOR_BYTES);
    AbstractDrive::SectorBuffer buf;
    bool beyond_eof = false;
    media_->read_sector(sector, &buf, beyond_eof);
    if (beyond_eof)
      throw eof_in_catalog();
    return buf[offset];
  }

  sector_count_type FileSystem::disc_sector_count() const
  {
    return root_->total_sectors();
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
  return *root_.get();
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
