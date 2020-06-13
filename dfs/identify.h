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
    ImageFileFormat(Geometry g, bool interleave)
      : geometry(g), interleaved(interleave)
    {
    }

    std::string description() const;

    Geometry geometry;
    bool interleaved;
  };

  // Probe some media to figure out what filesystem is on it.
  std::pair<Format, ImageFileFormat> identify_image(DataAccess&,
						    const std::string& filename);
  Format identify_file_system(DataAccess& access, Geometry geom, bool interleaved);

}  // namespace DFS
#endif
