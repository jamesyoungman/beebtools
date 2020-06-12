#ifndef INC_FSP_H
#define INC_FSP_H 1

#include <string>
#include <utility>

#include "dfscontext.h"

namespace DFS
{
  struct ParsedFileName
  {
    int drive;
    char dir;
    std::string name;
  };

bool parse_filename(const DFSContext& ctx, const std::string& fsp, ParsedFileName* p, std::string& error);
}  // namsespace DFS
#endif
