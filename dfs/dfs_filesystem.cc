//
//   Copyright 2020 James Youngman
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
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
#include "dfs_volume.h"
#include "fsp.h"
#include "opus_cat.h"
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
  FileSystem::FileSystem(DataAccess& media,
			 DFS::Format fmt,
			 DFS::Geometry geom)
    : format_(fmt), geometry_(geom), media_(media),
      volumes_(DFS::internal::init_volumes(media, fmt, geom))
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
	return DFS::UiStyle::Acorn;
      case Format::OpusDDOS:
	return DFS::UiStyle::Opus;
      }
    return DFS::UiStyle::Acorn;
  }

  std::vector<std::optional<char>>
  FileSystem::subvolumes() const
  {
    std::vector<std::optional<char>> result;
    for (const auto& vol : volumes_)
      {
	result.push_back(vol.first);
      }
    return result;
  }

  Format FileSystem::disc_format() const
  {
    return format_;
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

  DataAccess& FileSystem::whole_device() const
  {
    return media_;
  }

  sector_count_type FileSystem::disc_sector_count() const
  {
    if (disc_format() == DFS::Format::OpusDDOS)
      {
	return geometry_.total_sectors();
      }
    else
      {
	for (const auto& vol : volumes_)
	  return vol.second->root().total_sectors();
	throw BadFileSystem("no volumes in file system");
      }
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


Volume* FileSystem::mount(std::optional<char> key, std::string& error) const
{
  if (volumes_.size() > 1 && !key)
    {
      // When the disc image we are working with is an Opus DDOS image
      // (but at no other time) , drive "0" is equivalent to "0A".
      key = DEFAULT_VOLUME;
    }

  auto it = volumes_.find(key);
  if (it == volumes_.end())
    {
      if (key)
	{
	  std::ostringstream ss;
	  ss << "volume " << (*key) << " not found";
	  error.assign(ss.str());
	}
      else
	{
	  error = "no file system found";
	}
      return 0;
    }
  return it->second.get();
}


DFS::sector_count_type FileSystem::file_storage_space() const
{
  DFS::sector_count_type result(0);
  for (const auto& vol : volumes_)
    {
      result += vol.second->file_storage_space();
    }
  return result;
}

std::unique_ptr<SectorMap> FileSystem::get_sector_map(const SurfaceSelector& surface) const
{
  const bool multiple_catalogs = volumes_.size() > 1;
  std::unique_ptr<SectorMap> result = std::make_unique<SectorMap>(disc_sector_count(), multiple_catalogs);
  for (const auto& vol : volumes_)
    {
      DFS::VolumeSelector volsel(surface);
      if (vol.first)
	volsel = DFS::VolumeSelector(surface, *vol.first);
      vol.second->map_sectors(volsel, result.get());
    }
  if (disc_format() == Format::OpusDDOS)
    {
      auto disc_catalogue = internal::OpusDiscCatalogue::get_catalogue(media_, geometry());
      disc_catalogue.map_sectors(result.get());
    }
  return result;
}

std::string format_name(Format f)
{
  switch (f)
    {
    case Format::HDFS: return "HDFS";
    case Format::DFS: return "Acorn DFS";
    case Format::WDFS: return "Watford DFS";
    case Format::OpusDDOS: return "Opus DDOS";
    }
  abort();
}

// Returns true if the format is double-sided.  that is to say, the
// "total sectors" field of the catalog includes the sectors on both
// sides.
bool single_sided_filesystem(Format fmt, DataAccess& media)
{
  // TODO: move this function to identify.cc.
  if (fmt != DFS::Format::HDFS)
    return true;
  std::optional<DFS::SectorBuffer> got = media.read_block(1);
  if (!got)
    {
      // Zero sides isn't really an option.
      return true;
    }
  const DFS::SectorBuffer& sec1(*got);
  if (sec1[6] & 4)
    {
      // Two-sided HDFS.  We don't have examples or tests for this, so
      // implementation quality will likely be spotty.
      return false;
    }
  return true;
}

std::string description(const BootSetting& opt)
{
  switch (opt)
    {
    case BootSetting::None: return "off";
    case BootSetting::Load: return "LOAD";
    case BootSetting::Run: return "RUN";
    case BootSetting::Exec: return "EXEC";
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
    std::ostream::sentry s(os);
    if (s)
      {
	os << DFS::format_name(f);
      }
    return os;
  }
}
