#include "dfs_unused.h"

#include <sstream>

#include "abstractio.h"
#include "dfs_filesystem.h"
#include "dfs_volume.h"
#include "opus_cat.h"

namespace
{
  std::string file_label(const DFS::ParsedFileName& name,
			 bool multiple_catalogs)
  {
    std::ostringstream ss;
    if (multiple_catalogs)
      {
	ss << ':' << name.vol.effective_subvolume() << '.';
      }
    if (name.dir)
      {
	ss << name.dir << '.';
      }
    ss << name.name;
    return ss.str();
  }

}  // namespace

namespace DFS
{
  SectorMap::SectorMap(const DFS::sector_count_type total_sectors, bool multiple_catalogs)
    : total_sectors_(total_sectors),
      multiple_catalogs_(multiple_catalogs)
  {

  }

  std::optional<std::string> SectorMap::at(DFS::sector_count_type sec) const
  {
    std::optional<std::string> result;
    auto it = used_by_.find(sec);
    if (it != used_by_.end())
      result = it->second;
    return result;
  }

  void SectorMap::add_other(DFS::sector_count_type where, const std::string& label)
  {
    used_by_[where] = label;
  }

  void SectorMap::add_catalog_sector(DFS::sector_count_type where, const VolumeSelector& vol)
  {
    if (vol.subvolume())
      {
	std::ostringstream ss;
	ss << "*CAT:" << vol;
	used_by_[where] = ss.str();
      }
    else
      {
	// There is no need for a distinguishing suffix to identify
	// which catalog, so use a more descriptive label.
	used_by_[where] = "catalog";
      }
  }

  void SectorMap::add_file_sectors(DFS::sector_count_type begin,
				   DFS::sector_count_type end, // not included
				   const ParsedFileName& name)
  {
    std::string label = file_label(name, multiple_catalogs_);
    auto hint = used_by_.begin();
    for (auto sec = begin; sec < end; ++sec)
      {
	used_by_.insert(hint, std::make_pair(sec, label));
      }
  }
}  // DFS
