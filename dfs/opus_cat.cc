#include "opus_cat.h"

#include <algorithm>

#include "abstractio.h"
#include "dfs.h"
#include "exceptions.h"
#include "geometry.h"


namespace DFS
{
  namespace internal
  {
    OpusDiscCatalogue OpusDiscCatalogue::get_catalogue(DFS::DataAccess& media,
						       const DFS::Geometry& geom)
    {
      auto got = media.read_block(16);
      if (!got)
	throw BadFileSystem("inaccessible disc catalogue");
      return OpusDiscCatalogue(*got, geom);
    }

    OpusDiscCatalogue::OpusDiscCatalogue(const DFS::SectorBuffer& sector16,
					 const DFS::Geometry& geom)
      : total_disc_sectors_((sector16[1] << 8) | sector16[2]),
	sectors_per_track_(sector16[3])
    {
      std::fill(track0_summary_.begin(), track0_summary_.end(), '\0');
      track0_summary_[16] = 'T'; // disc catalogue
      track0_summary_[17] = 'X'; // unused, reserved

      if (total_disc_sectors_ != geom.total_sectors())
	{
	  std::ostringstream os;
	  os << "inconsistent total sector count ("
	     << total_disc_sectors_ << " from sector 16, "
	     << geom.total_sectors() << " from the disc image geometry) "
	     << "in Opus DDOS disc catalogue";
	  throw DFS::BadFileSystem(os.str());
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
	  track0_summary_[i*2] = label;
	  track0_summary_[i*2+1] = label;
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

    const std::vector<OpusDiscCatalogue::VolumeLocation>
    OpusDiscCatalogue::get_volume_locations() const
    {
      return locations_;
    }

    void OpusDiscCatalogue::map_sectors(DFS::SectorMap* out) const
    {
      DFS::sector_count_type sector = 0;
      for (char label : track0_summary_)
	{
	  switch (label)
	    {
	    case '\0': // unused
	      break;
	    case 'X':
	      out->add_other(sector, "reserved");
	      break;
	    case 'T':
	      out->add_other(sector, "disc-cat");
	      break;
	    default:
	      break;
	    }
	  ++sector;
	}
    }

  }  // namespace internal
}  // namespace DFS
