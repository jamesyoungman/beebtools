#ifndef INC_COMMANDS_H
#define INC_COMMANDS_H 1

#include <functional>
#include <map>
#include <vector>

#include "dfscontext.h"
#include "dfsimage.h"
#include "storage.h"

namespace DFS 
{
typedef std::function<bool(const StorageConfiguration&,
			   const DFSContext& ctx,
			   const std::vector<std::string>& extra_args)> Command;
typedef std::function<bool(const unsigned char* body_start,
			   const unsigned char* body_end,
			   const std::vector<std::string>& args_tail)> file_body_logic;
 
bool body_command(const StorageConfiguration& config, const DFSContext& ctx,
		  const std::vector<std::string>& args,
		  file_body_logic logic);

extern std::map<std::string, Command> commands;

bool cmd_cat(const StorageConfiguration&, const DFSContext&, const std::vector<std::string>&);
bool cmd_type(const StorageConfiguration&, const DFSContext&, const std::vector<std::string>&);
bool cmd_type_binary(const StorageConfiguration&, const DFSContext&, const std::vector<std::string>&);
bool cmd_list(const StorageConfiguration& config, const DFSContext& ctx,
	      const std::vector<std::string>& args);

}  // namespace DFS

#endif
