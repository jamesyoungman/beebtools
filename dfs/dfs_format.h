#ifndef INC_DFS_FORMAT_H
#define INC_DFS_FORMAT_H 1

#include <iostream>
#include <string>
#include <utility>

#include "abstractio.h"
#include "storage.h"

namespace DFS
{

enum class Format
  {
   HDFS,
   DFS,
   WDFS,
   Solidisk,
   // I have no documentation for Opus's format.
  };
  std::string format_name(Format f);

  std::pair<Format, DFS::sector_count_type>
  identify_image_format(const DataAccess&);

}  // namespace DFS


namespace std
{
  std::ostream& operator<<(std::ostream& os, const DFS::Format& f);
}  // namespace std

#endif
// Local Variables:
// Mode: C++
// End:
