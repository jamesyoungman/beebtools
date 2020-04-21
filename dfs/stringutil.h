#ifndef INC_STRINGUTIL_H
#define INC_STRINGUTIL_H 1

#include <string>

namespace DFS {

namespace stringutil 
{
    
bool case_insensitive_less(const std::string& left, const std::string& right);
bool case_insensitive_equal(const std::string& left, const std::string& right);
std::string rtrim(const std::string& input);

}  // namespace stringutil

}  // namespace DFS

#endif
