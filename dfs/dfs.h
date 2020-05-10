#ifndef INC_DFS_H
#define INC_DFS_H 1

#include <map>
#include <string>

#include "dfstypes.h"

namespace DFS {
  unsigned long sign_extend(unsigned long address);
  unsigned long compute_crc(const byte* start, const byte *end);
  extern const std::map<std::string, std::string> option_help;
}  // namespace DFS

#endif
