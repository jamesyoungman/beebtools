#ifndef INC_DFS_UNUSED_H
#define INC_DFS_UNUSED_H 1

#include "dfs_catalog.h"
#include "abstractio.h"

#include <optional>
#include <map>

namespace DFS
{
  class SpaceMap
  {
  public:
    explicit SpaceMap(const Catalog&, std::optional<std::string> sentinel);
    std::optional<std::string> at(DFS::sector_count_type sec) const;

  private:
    std::map<int, std::string> used_by_;
  };

}  // DFS

#endif
