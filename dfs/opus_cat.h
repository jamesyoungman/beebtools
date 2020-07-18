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
#ifndef INC_DISC_CAT_H
#define INC_DISC_CAT_H

#include <assert.h>      // for assert
#include <limits>        // for numeric_limits
#include <optional>      // for optional
#include <vector>        // for vector
#include "abstractio.h"  // for SectorBuffer
#include "dfstypes.h"    // for sector_count_type

namespace DFS
{
  class SectorMap;
  struct Geometry;

  namespace internal
  {
    class OpusDiscCatalogue
    {
    public:
      struct VolumeLocation
      {
      VolumeLocation(int catalog_sector, unsigned long start, unsigned long end, char vol)
	: catalog_location_(catalog_sector),
	  start_sector_(start), len_(end - start), volume_(vol)
	{
	  assert(end >= start);
	  const auto max = std::numeric_limits<DFS::sector_count_type>::max();
	  assert(start <= max);
	  assert(end <= max);
	}

	bool operator<(const VolumeLocation& other) const
	{
	  return start_sector() < other.start_sector();
	}

	int catalog_location() const
	{
	  return catalog_location_;
	}

	unsigned long start_sector() const
	{
	  return start_sector_;
	}

	unsigned long len() const
	{
	  return len_;
	}

	char volume() const
	{
	  return volume_;
	}

	void set_next_sector(unsigned long next)
	{
	  assert(next >= start_sector_);
	  len_ = next - start_sector_;
	}

      private:
	int catalog_location_;
	unsigned long start_sector_;
	unsigned long len_;
	char volume_;
      };

      static OpusDiscCatalogue get_catalogue(DFS::DataAccess& media,
					     std::optional<const DFS::Geometry> geom);
      // We construct an OpusDiscCatalogue once were certain an image
      // file contains an Opus DDOS disc catalogue, but also we do
      // this while probing the image file in order to guess what's in
      // it.  This means we don't always have an accurate idea yet of
      // what's in the image file (e.g. the sectors per track).
      explicit OpusDiscCatalogue(const DFS::SectorBuffer& sector16,
				 std::optional<const DFS::Geometry> geom);
      const std::vector<VolumeLocation> get_volume_locations() const;
      void map_sectors(DFS::SectorMap*) const;

    private:
      DFS::sector_count_type total_disc_sectors_;
      unsigned int sectors_per_track_;
      // we don't store the total track count because we see it set to 0 anyway.
      std::vector<VolumeLocation> locations_;
    };
  }  // namespace internal

}   // namespace DFS



#endif
