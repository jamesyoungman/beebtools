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
#ifndef INC_DFS_UNUSED_H
#define INC_DFS_UNUSED_H 1

#include <map>         // for map
#include <optional>    // for optional
#include <string>      // for string

#include "dfstypes.h"  // for sector_count_type

namespace DFS
{
  class VolumeSelector;
  struct ParsedFileName;

  class SectorMap
  {
  public:
    explicit SectorMap(bool multiple_catalogs);
    std::optional<std::string> at(DFS::sector_count_type sec) const;

    void add_file_sectors(DFS::sector_count_type begin,
			  DFS::sector_count_type end, // not included
			  const ParsedFileName& fn);
    void add_catalog_sector(DFS::sector_count_type where, const VolumeSelector& vol);
    void add_other(DFS::sector_count_type where, const std::string& label);

  private:
    bool multiple_catalogs_;
    std::map<int, std::string> used_by_;
  };

}  // DFS

#endif
