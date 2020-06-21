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
#include "geometry.h"


namespace
{
  class OpusDiscCatalogue
  {
  public:
    struct VolumeLocation
    {
      VolumeLocation(int catalog_sector, unsigned long start, unsigned long end, char vol)
	: catalog_location(catalog_sector),
	  start_sector(start), len(end - start), volume(vol)
      {
      }

      bool operator<(const VolumeLocation& other) const
      {
	return start_sector < other.start_sector;
      }

      int catalog_location;
      unsigned long start_sector;
      unsigned long len;
      char volume;
    };

    explicit OpusDiscCatalogue(const DFS::SectorBuffer& sector16,
			       const DFS::Geometry geom)
      : total_disc_sectors_((sector16[1] << 8) | sector16[2]),
	sectors_per_track_(sector16[3])
    {
      if (total_disc_sectors_ != geom.total_sectors())
	{
	  throw DFS::BadFileSystem("inconsistent total sector count in Opus DDOS disc catalogue");
	}
      if (sectors_per_track_ != geom.sectors)
	{
	  throw DFS::BadFileSystem("inconsistent sectors-per-ttrack in Opus DDOS disc catalogue");
	}
      static const char labels[] = "ABCDEFGH";
      char label;
      unsigned offset = 8;
      for (int i = 0; (label=labels[i]) != '\0'; ++i)
	{
	  const unsigned int track = sector16[offset];
	  assert(geom.cylinders >= 0);
	  if (track >= static_cast<unsigned int>(geom.cylinders))
	    {
	      std::ostringstream os;
	      os << "Opus DDOS volume " << label << " has starting track "
		 << std::dec << std::setw(0) << track << " but the disc itself "
		 << "only has " << geom.cylinders << " tracks";
	      throw DFS::BadFileSystem(os.str());
	    }
	  auto start = DFS::safe_unsigned_multiply(track, sectors_per_track_);
	  locations_.emplace_back(i*2, start, start, label);
	  offset += 2u;
	}
      std::sort(locations_.begin(), locations_.end());
      unsigned long next_sector = total_disc_sectors_;
      for (auto it = locations_.rbegin();
	   it != locations_.rend();
	   ++it)
	{
	  it->len = next_sector - it->start_sector;
	  next_sector = it->start_sector;
	}
    }

    const std::vector<VolumeLocation> get_volume_locations() const
    {
      return locations_;
    }

  private:
    int total_disc_sectors_;
    unsigned int sectors_per_track_;
    // we don't store the total track count because we see it set to 0 anyway.
    std::vector<VolumeLocation> locations_;
  };
}  // namespace


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
						       vol_loc.catalog_location,
						       vol_loc.start_sector,
						       vol_loc.len,
						       media);
	      result.insert(std::make_pair(vol_loc.volume, std::move(vol)));
	    }
	}
      else
	{
	  auto vol = std::make_unique<DFS::Volume>(fmt, 0, 0, geom.total_sectors(), media);
	  result.insert(std::make_pair(DFS::FileSystem::DEFAULT_VOLUME, std::move(vol)));
	}
      return result;
    }

  }  // namespace internal

  Volume::Volume(DFS::Format format,
		 DFS::sector_count_type catalog_location,
		 unsigned long first_sector,
		 unsigned long total_sectors,
		 DataAccess& media)
    : volume_tracks_(first_sector, total_sectors, media),
      root_(std::make_unique<Catalog>(format, catalog_location, media))
  {
  }

  const Catalog& Volume::root() const
  {
    return *root_;
  }

  DataAccess& Volume::data_region()
  {
    return volume_tracks_;
  }

}  // namespace DFS
