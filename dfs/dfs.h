#ifndef INC_DFS_H
#define INC_DFS_H 1

#include <map>
#include <string>

#include "dfstypes.h"

namespace DFS {
  unsigned long sign_extend(unsigned long address);
  unsigned long compute_crc(const byte* start, const byte *end);
  const std::map<std::string, std::string>& get_option_help();
}  // namespace DFS

#endif
