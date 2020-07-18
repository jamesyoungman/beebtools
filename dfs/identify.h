//
//   Copyright 2020 James Youngman
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
#ifndef INC_IDENTFY_H
#define INC_IDENTFY_H 1

#include <optional>      // for optional
#include <string>        // for string

#include "abstractio.h"  // for SectorBuffer
#include "dfs_format.h"  // for Format
#include "dfstypes.h"    // for sector_count_type
#include "geometry.h"    // for Geometry

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
			       const DFS::SectorBuffer& sec1, std::string& error);
    bool smells_like_hdfs(const SectorBuffer& sec1);
    bool smells_like_watford(DataAccess& access,
			     const DFS::SectorBuffer& sec1);
    bool smells_like_opus_ddos(DataAccess& media, sector_count_type* sectors);
  }

}  // namespace DFS
#endif
