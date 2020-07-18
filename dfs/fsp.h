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
#ifndef INC_FSP_H
#define INC_FSP_H 1

#include <string>           // for string, basic_string

#include "driveselector.h"  // for VolumeSelector

namespace DFS
{
  struct DFSContext;

  struct ParsedFileName
  {
    ParsedFileName();

    DFS::VolumeSelector vol;
    char dir;
    std::string name;
  };

bool parse_filename(const DFSContext& ctx, const std::string& fsp, ParsedFileName* p, std::string& error);
}  // namsespace DFS
#endif
