#ifndef INC_DFSTYPES_H
#define INC_DFSTYPES_H 1

#include <assert.h>

#include <limits>
#include <vector>

namespace DFS
{
  typedef unsigned char byte;
  typedef std::vector<byte>::size_type offset;
  typedef unsigned short sector_count_type; // needs 10 bits

  inline sector_count_type sector_count(long int x)
  {
    assert(x >= 0);
    assert(x <= std::numeric_limits<sector_count_type>::max());
    return static_cast<DFS::sector_count_type>(x);
  }
}  // namespace DFS

#endif
