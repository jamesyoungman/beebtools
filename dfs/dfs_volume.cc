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
#include "dfs_volume.h"
#include <map>            // for map
#include <memory>         // for unique_ptr, make_unique, allocator
#include <optional>       // for optional, nullopt_t, nullopt
#include <type_traits>    // for remove_reference<>::type
#include <utility>        // for pair, make_pair, move
#include <vector>         // for vector

#include "abstractio.h"   // for DataAccess
#include "dfs_catalog.h"  // for Catalog
#include "dfs_format.h"   // for Format, Format::OpusDDOS
#include "exceptions.h"   // for BadFileSystem
#include "geometry.h"     // for Geometry
#include "opus_cat.h"     // for OpusDiscCatalogue::VolumeLocation, ...

namespace DFS
{
  class SectorMap;
  class VolumeSelector;

  namespace internal
  {
    std::map<std::optional<char>, std::unique_ptr<DFS::Volume>>
    init_volumes(DFS::DataAccess& media, DFS::Format fmt, const DFS::Geometry& geom)
    {
      std::map<std::optional<char>, std::unique_ptr<DFS::Volume>> result;
      if (fmt == DFS::Format::OpusDDOS)
	{
	  auto got = media.read_block(16);
	  if (!got)
	    throw DFS::BadFileSystem("file system detected as Opus DDOS but the sector which should contain the disc catalogue is unreadable");
	  auto dc = OpusDiscCatalogue(*got, geom);
	  for (const auto& vol_loc : dc.get_volume_locations())
	    {
	      auto vol = std::make_unique<DFS::Volume>(fmt,
						       vol_loc.catalog_location(),
						       vol_loc.start_sector(),
						       vol_loc.len(),
						       media);
	      result.insert(std::make_pair(vol_loc.volume(), std::move(vol)));
	    }
	}
      else
	{
	  auto vol = std::make_unique<DFS::Volume>(fmt, 0, 0, geom.total_sectors(), media);
	  result.insert(std::make_pair(std::nullopt, std::move(vol)));
	}
      return result;
    }

  }  // namespace internal

  Volume::Volume(DFS::Format format,
		 DFS::sector_count_type catalog_location,
		 unsigned long first_sector,
		 unsigned long total_sectors,
		 DataAccess& media)
    : catalog_location_(catalog_location),
      total_sectors_(DFS::sector_count(total_sectors)),
      volume_tracks_(first_sector, total_sectors, media),
      root_(std::make_unique<Catalog>(format, catalog_location, media))
  {
  }

  const Catalog& Volume::root() const
  {
    return *root_;
  }

  DFS::sector_count_type
  Volume::file_storage_space() const
  {
    return total_sectors_;
  }

  DataAccess& Volume::data_region()
  {
    return volume_tracks_;
  }

  void Volume::map_sectors(const DFS::VolumeSelector& vol,
			   DFS::SectorMap* out) const
  {
    root_->map_sectors(vol, catalog_location_, volume_data_origin(), out);
  }

}  // namespace DFS
