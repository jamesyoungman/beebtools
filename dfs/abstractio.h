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
