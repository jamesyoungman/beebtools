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
// FileSystem is an image of a single file system (as opposed to a
// wrapper around a disk image file, which might for example contain a
// separate file system for each surface).
class FileSystem
{
public:
  // TODO: pass in the filesystem type, now that we need to figure it
  // out before we call the FileSystem constructor.
  explicit FileSystem(AbstractDrive* drive);
  const Catalog& root() const;

  // Determine what UI styling to use for the current file system.
  DFS::UiStyle ui_style(const DFSContext&) const;

  inline Format disc_format() const
  {
    return format_;
  }

  sector_count_type disc_sector_count() const;

private:
  byte get_byte(sector_count_type sector, unsigned offset) const;

  Format format_;
  AbstractDrive* media_;
  std::unique_ptr<Catalog> root_;
};

}  // namespace DFS

#endif
