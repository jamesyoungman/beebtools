#ifndef INC_DFS_FILESYSTEM_H
#define INC_DFS_FILESYSTEM_H 1

#include <assert.h>

#include <algorithm>
#include <exception>
#include <functional>
#include <optional>
#include <vector>
#include <stdexcept>
#include <string>
#include <utility>

#include "abstractio.h"
#include "dfs_catalog.h"
#include "dfscontext.h"
#include "dfstypes.h"
#include "exceptions.h"
#include "fsp.h"
#include "media.h"
#include "storage.h"
#include "stringutil.h"

namespace DFS
{
  class Volume;
  class SectorMap;

// FileSystem is an image of a single file system (as opposed to a
// wrapper around a disk image file, which might for example contain a
// separate file system for each surface).
class FileSystem
{
public:
  static constexpr char DEFAULT_VOLUME = 'A';
  explicit FileSystem(DataAccess&, DFS::Format fmt, DFS::Geometry geom);
  Volume* mount(std::optional<char> vol, std::string& error) const;
  std::vector<std::optional<char>> subvolumes() const;
  // Determine what UI styling to use for the current file system.
  DFS::UiStyle ui_style(const DFSContext&) const;
  Format disc_format() const;

  std::unique_ptr<SectorMap> get_sector_map(const SurfaceSelector& surface) const;

  DFS::sector_count_type disc_sector_count() const;
  DFS::sector_count_type file_storage_space() const;
  Geometry geometry() const;
  DataAccess& whole_device() const;

private:
  byte get_byte(sector_count_type sector, unsigned offset) const;

  Format format_;
  Geometry geometry_;
  DataAccess& media_;
  std::map<std::optional<char>, std::unique_ptr<Volume>> volumes_;
};

}  // namespace DFS

#endif
