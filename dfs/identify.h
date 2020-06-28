#ifndef INC_IDENTFY_H
#define INC_IDENTFY_H 1

#include <optional>
#include <utility>

#include "abstractio.h"
#include "geometry.h"
#include "dfs_format.h"

namespace DFS
{
  struct ImageFileFormat
  {
    ImageFileFormat(Geometry g, bool interleave);
    std::string description() const;

    Geometry geometry;
    bool interleaved;
  };

  // Probe some media to figure out what geometry the disc (image) is.
  std::optional<ImageFileFormat> identify_image(DataAccess&, const std::string& filename, std::string& error);
  // Probe some media to figure out what filesystem is on it.
  std::optional<Format> identify_file_system(DataAccess& access, Geometry geom, bool interleaved, std::string& error);

  bool single_sided_filesystem(Format f, DataAccess&);

  namespace internal		/* exposed for unit testing */
  {
    bool smells_like_acorn_dfs(DataAccess& access,
			       const DFS::SectorBuffer& sec1);
    bool smells_like_hdfs(const SectorBuffer& sec1);
    bool smells_like_watford(DataAccess& access,
			     const DFS::SectorBuffer& sec1);
    bool smells_like_opus_ddos(DataAccess& media, sector_count_type* sectors);
  }

}  // namespace DFS
#endif
