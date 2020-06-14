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

  class DataAccess
  {
  public:
    virtual ~DataAccess();
    // On error, raise OsError.  On read beyonf EOF, returns empty buffer.
    virtual std::optional<SectorBuffer> read_block(DFS::sector_count_type lba) const = 0;
  };

}
#endif
