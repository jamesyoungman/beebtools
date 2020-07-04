#ifndef INC_DFS_VOLUME_H
#define INC_DFS_VOLUME_H 1

#include <map>
#include <memory>
#include <optional>

#include "abstractio.h"
#include "dfs_catalog.h"
#include "dfs_format.h"
#include "dfs_unused.h"
#include "geometry.h"

namespace DFS
{

// Opus DDOS divides a disc into up to 8 volumes (identified by a
// letter, A-H).  In our object model, the file system is divided into
// a number of volumes, each of which has a root catalog.  Disc
// formats other than Opus DDOS have just one volume.
class Volume
{
 public:
  explicit Volume(DFS::Format format,
		  DFS::sector_count_type catalog_location,
		  unsigned long first_sector, unsigned long total_sectors,
		  DataAccess&);
  const Catalog& root() const;
  DataAccess& data_region();
  void map_sectors(const DFS::VolumeSelector& vol, DFS::SectorMap*) const;

  unsigned long volume_data_origin() const
  {
    return volume_tracks_.origin();
  }
  DFS::sector_count_type file_storage_space() const;


 private:
  class Access : public DataAccess
   {
   public:
   Access(unsigned long first_sector, unsigned long sectors,
	  DataAccess& underlying)
     : origin_(first_sector), len_(sectors), underlying_(underlying)
       {
       }

     std::optional<SectorBuffer> read_block(unsigned long lba) override
       {
	 if (lba > len_)
	   return std::nullopt;
	 return underlying_.read_block(origin_ + lba);
       }

     unsigned long origin() const
     {
       return origin_;
     }

   private:
     unsigned long origin_;
     unsigned long len_;
     DataAccess& underlying_;
   };
 sector_count_type catalog_location_;
 sector_count_type total_sectors_;
 Access volume_tracks_;
 std::unique_ptr<Catalog> root_;
};

 namespace internal
 {
    std::map<std::optional<char>, std::unique_ptr<DFS::Volume>>
      init_volumes(DFS::DataAccess& media, DFS::Format fmt, const DFS::Geometry&);
 }  // namespace internal

}  // namespace DFS

#endif
