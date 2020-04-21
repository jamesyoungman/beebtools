#ifndef INC_COMMANDS_H
#define INC_COMMANDS_H 1

#include <functional>
#include <map>
#include <vector>

#include "dfscontext.h"
#include "dfsimage.h"

namespace DFS 
{
typedef std::function<bool(const Image&,
			   const DFSContext& ctx,
			   const std::vector<std::string>& extra_args)> Command;
typedef std::function<bool(const unsigned char* body_start,
			   const unsigned char* body_end,
			   const std::vector<std::string>& args_tail)> file_body_logic;
 
bool body_command(const Image& image, const DFSContext& ctx,
		  const std::vector<std::string>& args,
		  file_body_logic logic);

extern std::map<std::string, Command> commands;

bool cmd_cat(const Image&, const DFSContext&, const std::vector<std::string>&);
bool cmd_type(const Image&, const DFSContext&, const std::vector<std::string>&);
bool cmd_list(const Image& image, const DFSContext& ctx,
	      const std::vector<std::string>& args);

}  // namespace DFS

#endif
