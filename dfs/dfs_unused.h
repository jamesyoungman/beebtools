#ifndef INC_DFS_UNUSED_H
#define INC_DFS_UNUSED_H 1

#include "dfs_catalog.h"
#include "abstractio.h"
#include "fsp.h"

#include <optional>
#include <map>

namespace DFS
{
  class SectorMap
  {
  public:
    explicit SectorMap(DFS::sector_count_type device_total_sectors,
		       bool multiple_catalogs);
    std::optional<std::string> at(DFS::sector_count_type sec) const;

    void add_file_sectors(DFS::sector_count_type begin,
			  DFS::sector_count_type end, // not included
			  const ParsedFileName& fn);
    void add_catalog_sector(DFS::sector_count_type where, const VolumeSelector& vol);
    void add_other(DFS::sector_count_type where, const std::string& label);

  private:
    DFS::sector_count_type total_sectors_;
    bool multiple_catalogs_;
    std::map<int, std::string> used_by_;
  };

}  // DFS

#endif
