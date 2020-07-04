#include "dfs_volume.h"

#include <iomanip>
#include <map>
#include <memory>
#include <optional>
#include <sstream>

#include "abstractio.h"
#include "dfs.h"
#include "dfs_catalog.h"
#include "dfs_filesystem.h"
#include "dfs_format.h"
#include "opus_cat.h"
#include "geometry.h"


namespace DFS
{
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
	    throw DFS::BadFileSystem("file system detected as Opus DDOS but it's too short to contain a disc catalogue");
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
