#include "commands.h"

#include <iostream>
#include <vector>
#include <string>

namespace
{
  void read_file_body(const DFS::CatalogEntry& entry,
		      std::vector<DFS::byte>* body)
  {
    entry.visit_file_body_piecewise([body]
				    (const DFS::byte* begin, const DFS::byte* end)
				    {
				      std::copy(begin, end,
						std::back_inserter(*body));
				      return true;
				    });
  }

}  // namespace


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

bool body_command(const StorageConfiguration& storage, const DFSContext& ctx,
		  const std::vector<std::string>& args,
		  file_body_logic logic)
{
  if (args.size() < 2)
    {
      std::cerr << "please give a file name.\n";
      return false;
    }
  if (args.size() > 2)
    {
      // The Beeb ignores subsequent arguments.
      std::cerr << "warning: ignoring additional arguments.\n";
    }
  ParsedFileName name;
  std::string error;
  if (!parse_filename(ctx, args[1], &name, error))
    {
      std::cerr << args[1] << " is not a valid file name: " << error << "\n";
      return false;
    }
  std::unique_ptr<DFS::FileSystem> file_system(storage.mount(name.drive, error));
  if (!file_system)
    {
      std::cerr << "failed to select drive " << name.drive << ": " << error << "\n";
      return false;
    }
  const auto& root(file_system->root());
  const std::optional<CatalogEntry> entry = root.find_catalog_entry_for_name(name);
  if (!entry)
    {
      std::cerr << args[1] << ": not found\n";
      return false;
    }
  std::vector<DFS::byte> body;
  read_file_body(*entry, &body);
  const std::vector<std::string> tail(args.begin() + 1, args.end());
  return logic(body.data(), body.data() + body.size(), tail);
}

}  // namespace DFS
