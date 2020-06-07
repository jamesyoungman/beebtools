#ifndef INC_DFSIMAGE_H
#define INC_DFSIMAGE_H 1

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
BadFileSystem eof_in_catalog();

// FileSystem is an image of a single file system (as opposed to a
// wrapper around a disk image file, which might for example contain a
// separate file system for each surface).
class FileSystem
{
public:
  static Format identify_format(AbstractDrive* drive);
  explicit FileSystem(AbstractDrive* drive);
  const Catalog& root() const;

  // Determine what UI styling to use for the current file system.
  DFS::UiStyle ui_style(const DFSContext&) const;

  inline Format disc_format() const
  {
    return format_;
  }

  sector_count_type disc_sector_count() const;

  // sector_to_catalog_entry_mapping uses special values to represent the
  // catalog itself (0) and free sectors (-1).
  static constexpr int sector_unused = -1;
  static constexpr int sector_catalogue = 0;
  std::vector<int> sector_to_catalog_entry_mapping() const;

private:
  byte get_byte(sector_count_type sector, unsigned offset) const;

  Format format_;
  AbstractDrive* media_;
  std::unique_ptr<Catalog> root_;
};

}  // namespace DFS

namespace std
{
  std::ostream& operator<<(std::ostream& os, const DFS::Format& f);
}  // namespace std

#endif
