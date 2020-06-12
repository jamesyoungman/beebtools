#ifndef INC_DFS_FORMAT_H
#define INC_DFS_FORMAT_H 1

#include <functional>
#include <iostream>
#include <string>
#include <utility>

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
  identify_image_format(std::function<void(DFS::sector_count_type,
					   DFS::SectorBuffer*,
					   bool&)> sector_reader);
  std::pair<Format, DFS::sector_count_type>
  identify_drive_format(AbstractDrive*);

}  // namespace DFS


namespace std
{
  std::ostream& operator<<(std::ostream& os, const DFS::Format& f);
}  // namespace std

#endif
// Local Variables:
// Mode: C++
// End:
