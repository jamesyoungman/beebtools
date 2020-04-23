#ifndef INC_DFSTYPES_H
#define INC_DFSTYPES_H 1

#include <vector>

namespace DFS
{
  typedef unsigned char byte;
  typedef std::vector<byte>::size_type offset;
  typedef unsigned short sector_count_type; // needs 10 bits

}  // namespace DFS

#endif
