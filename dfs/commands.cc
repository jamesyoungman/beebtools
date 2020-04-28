#include "commands.h"

namespace DFS
{
  CommandInterface* CIReg::get_command(const std::string& name)
  {
    auto m = get_command_map();
    auto it = m->find(name);
    if (it == m->end())
      return 0;
    return it->second.get();
  }

  CIReg::map_type* CIReg::get_command_map()
  {
    static std::unique_ptr<map_type> instance;
    if (!instance)
      {
	instance = std::make_unique<map_type>();
      }
    assert(instance);
    return instance.get();
  }

  bool CIReg::visit_all_commands(std::function<bool(CommandInterface*)> visitor)
  {
    auto m = get_command_map();
    for (const auto& cmd : *m)
      {
	bool ok = visitor(cmd.second.get());
	if (!ok)
	  return false;
      }
    return true;
  }

}  // namespace DFS
