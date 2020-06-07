#ifndef INC_STRINGUTIL_H
#define INC_STRINGUTIL_H 1

#include <string>

#include "dfstypes.h"

namespace DFS {

namespace stringutil
{

bool case_insensitive_less(const std::string& left, const std::string& right);
bool case_insensitive_equal(const std::string& left, const std::string& right);
std::string rtrim(const std::string& input);

inline char byte_to_ascii7(DFS::byte b)
{
  return char(b & 0x7F);
}


}  // namespace stringutil

}  // namespace DFS

#endif
