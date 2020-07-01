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
   OpusDDOS,
  };
  std::string format_name(Format f);
}  // namespace DFS


namespace std
{
  std::ostream& operator<<(std::ostream& os, const DFS::Format& f);
}  // namespace std

#endif
// Local Variables:
// Mode: C++
// End:
