#ifndef INC_DFS_FORMAT_H
#define INC_DFS_FORMAT_H 1

#include <iostream>
#include <string>

namespace DFS
{

enum class Format
  {
   HDFS,
   DFS,
   WDFS,
   Solidisk,
   OpusDDOS,
  };
  std::string format_name(Format f);
  bool single_sided_filesystem(Format f);
}  // namespace DFS


namespace std
{
  std::ostream& operator<<(std::ostream& os, const DFS::Format& f);
}  // namespace std

#endif
// Local Variables:
// Mode: C++
// End:
