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
#ifndef INC_ABSTRACTIO_H
#define INC_ABSTRACTIO_H 1

#include <array>
#include <optional>
#include <string>

#include "dfstypes.h"

namespace DFS
{
  constexpr unsigned int SECTOR_BYTES = 256;
  typedef std::array<byte, DFS::SECTOR_BYTES> SectorBuffer;

  class FileAccess
  {
  public:
    virtual ~FileAccess();
    // On error, raise OsError.  On read beyond EOF, returns empty.
    virtual std::vector<byte> read(unsigned long offset, unsigned long len) = 0;
  };

  class DataAccess
  {
  public:
    virtual ~DataAccess();
    // On error, raise OsError.  On read beyond EOF, returns empty
    // optional.  The lba address is an unsigned long instead of a
    // sector_count_type because sector_count_type is for use within
    // DFS file systems, while the lba address here could be a sector
    // position within (e.g.) an MMB file, which is much larger.
    virtual std::optional<SectorBuffer> read_block(unsigned long lba) = 0;
  };
}
#endif
