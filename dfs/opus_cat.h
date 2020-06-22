#ifndef INC_DISC_CAT_H
#define INC_DISC_CAT_H

#include <iomanip>
#include <vector>

#include "abstractio.h"
#include "dfstypes.h"
#include "dfs_unused.h"
#include "geometry.h"

namespace DFS
{
  namespace internal
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

      static OpusDiscCatalogue get_catalogue(DFS::DataAccess& media,
					     const DFS::Geometry& geom);
      explicit OpusDiscCatalogue(const DFS::SectorBuffer& sector16,
				 const DFS::Geometry& geom);
      const std::vector<VolumeLocation> get_volume_locations() const;
      void map_sectors(DFS::SectorMap*) const;

    private:
      DFS::sector_count_type total_disc_sectors_;
      unsigned int sectors_per_track_;
      // we don't store the total track count because we see it set to 0 anyway.
      std::vector<VolumeLocation> locations_;
      std::array<char, 18> track0_summary_;
    };
  }  // namespace internal

}   // namespace DFS



#endif
