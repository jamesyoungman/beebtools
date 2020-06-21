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

namespace DFS
{

// Opus DDOS divides a disc into up to 8 volumes (identified by a
// letter, A-H).  In our object model, the file system is divided into
// a number of volumes, each of which has a root catalog.  Disc
// formats other than Opus DDOS have just one volume.
class Volume
{
public:
  explicit Volume(DFS::Format format, DataAccess&);
  const Catalog& root() const;

private:
  DataAccess& media_;
  std::unique_ptr<Catalog> root_;
};

// FileSystem is an image of a single file system (as opposed to a
// wrapper around a disk image file, which might for example contain a
// separate file system for each surface).
class FileSystem
{
public:
  explicit FileSystem(DataAccess&, DFS::Format fmt, DFS::Geometry geom);
  const Catalog& root() const;

  // Determine what UI styling to use for the current file system.
  DFS::UiStyle ui_style(const DFSContext&) const;

  inline Format disc_format() const
  {
    return format_;
  }

  DFS::sector_count_type disc_sector_count() const;
  Geometry geometry() const;
  DataAccess& device() const;

private:
  byte get_byte(sector_count_type sector, unsigned offset) const;

  Format format_;
  Geometry geometry_;
  DataAccess& media_;
  std::map<std::optional<char>, std::unique_ptr<Volume>> volumes_;
};

}  // namespace DFS

#endif
