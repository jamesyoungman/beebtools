#include "commands.h"

#include <iostream>
#include <vector>
#include <string>

namespace
{
  void read_file_body(const DFS::FileSystem *fs,
		      unsigned short slot,
		      std::vector<DFS::byte>* body)
  {
    using DFS::byte;
    using std::copy;
    fs->visit_file_body_piecewise(slot, [body]
				  (const byte* begin, const byte* end)
					{
					  copy(begin, end,
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
  if (!parse_filename(ctx, args[1], &name))
    return false;



  DFS::AbstractDrive *drive;
  if (!storage.select_drive(name.drive, &drive))
    return false;
  assert(drive != 0);
  const DFS::FileSystem file_system(drive);
  const int slot = file_system.find_catalog_slot_for_name(name);
  if (slot < 0)
    {
      std::cerr << args[1] << ": not found\n";
      return false;
    }
  std::vector<DFS::byte> body;
  assert(slot < std::numeric_limits<unsigned short>::max());
  read_file_body(&file_system, static_cast<unsigned short>(slot), &body);
  const std::vector<std::string> tail(args.begin() + 1, args.end());
  return logic(body.data(), body.data() + body.size(), tail);
}

}  // namespace DFS
