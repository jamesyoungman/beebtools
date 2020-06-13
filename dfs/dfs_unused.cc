#include "dfs_unused.h"

#include "abstractio.h"

namespace
{
  std::map<int, std::string> make_space_map(const DFS::Catalog& catalog,
					    std::optional<std::string> sentinel)
  {
    std::map<int, std::string> result;
    for (const DFS::CatalogEntry& entry : catalog.entries())
      {
	auto here = result.begin();
	for (int i = entry.start_sector(); i < entry.last_sector(); ++i)
	  {
	    result.insert(here, std::make_pair(i, entry.name()));
	  }
      }
    const std::string cat_name("catalog");
    auto here = result.begin();
    for (DFS::sector_count_type sec : catalog.get_sectors_occupied_by_catalog())
      {
	result.insert(here, std::make_pair(sec, cat_name));
      }
    if (sentinel)
      {
	result.insert(result.end(), std::make_pair(catalog.total_sectors(), *sentinel));
      }
    return result;
  }

}  // namespace

namespace DFS
{
  SpaceMap::SpaceMap(const Catalog& catalog, std::optional<std::string> sentinel)
    : used_by_(make_space_map(catalog, sentinel))
  {
  }

  std::optional<std::string> SpaceMap::at(DFS::sector_count_type sec) const
  {
    std::optional<std::string> result;
    auto it = used_by_.find(sec);
    if (it != used_by_.end())
      result = it->second;
    return result;
  }

}  // DFS
