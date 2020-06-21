#ifndef INC_DFS_FILESYSTEM_H
#define INC_DFS_FILESYSTEM_H 1

#include <assert.h>

#include <algorithm>
#include <exception>
#include <functional>
#include <optional>
#include <vector>
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

// Temporarily, we include dfs_volume.h so that callers of mount()
// don't also need themselves to include dfs_volume.h
#include "dfs_volume.h"
// TODO: remove this.


namespace DFS
{
  class Volume;

// FileSystem is an image of a single file system (as opposed to a
// wrapper around a disk image file, which might for example contain a
// separate file system for each surface).
class FileSystem
{
public:
  static constexpr char DEFAULT_VOLUME = 'A';
  explicit FileSystem(DataAccess&, DFS::Format fmt, DFS::Geometry geom);
  Volume* mount(char vol) const;
  Volume* mount() const;

  // Determine what UI styling to use for the current file system.
  DFS::UiStyle ui_style(const DFSContext&) const;

  inline Format disc_format() const
  {
    return format_;
  }

  DFS::sector_count_type disc_sector_count() const;
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
