#ifndef INC_DFSTYPES_H
#define INC_DFSTYPES_H 1

#include <assert.h>

#include <array>
#include <limits>
#include <vector>

namespace DFS
{
  typedef unsigned char byte;
  typedef std::vector<byte>::size_type offset;
  typedef unsigned int drive_number;

  // sector counts need 10 bits normally, but there are some
  // double-density file systems needing more.  16 bits though is
  // certainly enough.  The unsigned int type is guaranteed to be at
  // least 16.  As is unsigned short.  We don't use unsigned short as
  // this gives rise to the need for additional casts and calls to
  // sector_count().  sector_count_type actually used to be unsigned
  // short, and so some of those needless casts are actually still in
  // the code.
  typedef unsigned int sector_count_type;

  inline sector_count_type sector_count(long int x)
  {
    assert(x >= 0);
    assert(x <= std::numeric_limits<sector_count_type>::max());
    return static_cast<DFS::sector_count_type>(x);
  }
}  // namespace DFS

#endif
