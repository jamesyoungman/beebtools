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
  ImageFileFormat identify_image(DataAccess&, const std::string& filename);
  // Probe some media to figure out what filesystem is on it.
  Format identify_file_system(DataAccess& access, Geometry geom, bool interleaved);

  namespace internal		/* exposed for unit testing */
  {
    bool smells_like_hdfs(const SectorBuffer& sec1);
    bool smells_like_watford(DataAccess& access,
			     const DFS::SectorBuffer& sec1);
    bool smells_like_opus_ddos(DataAccess& media, sector_count_type* sectors);
  }

}  // namespace DFS
#endif
