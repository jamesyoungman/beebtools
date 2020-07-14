#ifndef INC_GEOMETRY_H
#define INC_GEOMETRY_H

#include <optional>
#include <string>

#include "dfstypes.h"

namespace DFS
{
  enum class Encoding
    {
     FM,			// AKA, single density
     MFM			// AKA, double density
    };
  std::string encoding_description(const Encoding&);
  std::string encoding_to_str(const DFS::Encoding&);

  struct Geometry
  {
    int cylinders;		// for a single-surface device, same as track count
    int heads;			// or number of surfaces/sides
    DFS::sector_count_type sectors; // for a base Acorn DFS disc, 10 (per track).
    // sector size is always 256 bytes in this code base (since it is
    // universal for BBC Micro file systems, though it is unusual for
    // computers generally).
    std::optional<Encoding> encoding;

    Geometry(int c, int h, DFS::sector_count_type s, std::optional<Encoding> enc);
    Geometry();
    DFS::sector_count_type total_sectors() const;
    bool operator==(const Geometry&) const;
    std::string to_str() const;
    std::string description() const;
  };

  std::optional<Geometry> guess_geometry_from_total_bytes
    (unsigned long total_bytes,
     std::optional<int> heads = std::nullopt);

}  // namespace DFS

#endif
