//
//   Copyright 2020 James Youngman
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
#ifndef INC_COMMANDS_H
#define INC_COMMANDS_H 1

#include <cassert>
#include <functional>
#include <map>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

#include "dfscontext.h"
#include "dfs_filesystem.h"
#include "storage.h"

namespace DFS
{
  class CommandInterface
  {
  public:
    virtual const std::string name() const = 0;
    virtual const std::string usage() const = 0;
    virtual const std::string description() const = 0;
    virtual bool invoke(const StorageConfiguration&,
			const DFSContext&,
			const std::vector<std::string>& args) = 0;
  };

  class CIReg
  {
  public:
    using map_type = std::map<std::string, std::unique_ptr<CommandInterface>>;

    CIReg(std::unique_ptr<CommandInterface> inst)
    {
      assert(inst != 0);
      const std::string name(inst->name());
      assert(!name.empty());
      auto m = get_command_map();
      m->insert(m->begin(), std::make_pair(name, std::move(inst)));
    }
    static CommandInterface* get_command(const std::string& name);
    static bool visit_all_commands(std::function<bool(CommandInterface*)> visitor);

  private:
    static map_type* get_command_map();
  };

  CIReg* get_command_registry();

#define STRINGIFY(n) #n
#define PASTE(a,b) a##b
#define REGISTER_COMMAND(classname)	\
  static classname PASTE(sole_instance_, classname); \
  DFS::CIReg PASTE(classname,_reg)(std::make_unique<classname>())

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

bool cmd_list(const StorageConfiguration& config, const DFSContext& ctx,
	      const std::vector<std::string>& args);


  class CommandHelp : public CommandInterface
  {
  public:
    const std::string name() const override;
    const std::string usage() const override;
    const std::string description() const override;
    static bool check_consistency();
    bool invoke(const DFS::StorageConfiguration&,
		const DFS::DFSContext&,
		const std::vector<std::string>& args) override;
  };


}  // namespace DFS

#endif
