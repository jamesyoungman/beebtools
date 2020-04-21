#ifndef INC_FSP_H
#define INC_FSP_H 1

#include <string>
#include <utility>

#include "dfscontext.h"

namespace DFS
{
std::pair<char, std::string> directory_and_name_of(const DFSContext&, const std::string&);
}  // namsespace DFS
#endif
