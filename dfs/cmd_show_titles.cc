#include "commands.h"

#include <sstream>

#include "dfs_filesystem.h"
#include "driveselector.h"

namespace
{

class CommandShowTitles : public DFS::CommandInterface
{
  public:
  const std::string name() const override
  {
    return "show-titles";
  }

  const std::string usage() const override
  {
    return name() + " [drive]...\n"
      "Show the titles of the discs in the specified drives.\n"
      "If no drive argument is specified, show all titles.\n";
  }

  const std::string description() const override
  {
    return "display disc titles";
  }

  bool show_title(const DFS::StorageConfiguration& storage,
		  DFS::drive_number d, std::string& error)
  {
    auto file_system(storage.mount(d, error));
    if (!file_system)
      return false;
    std::cout << d << ":" << file_system->root().title() << "\n";
    return std::cout.good();
  }

  bool operator()(const DFS::StorageConfiguration& storage,
		  const DFS::DFSContext&,
		  const std::vector<std::string>& args) override
  {
    std::string error;
    auto faild = [&error](DFS::drive_number d)
		 {
		   std::cerr << "failed to select drive " << d << ": " << error << "\n";
		   return false;
		 };
    auto fail = [&error]()
		 {
		   std::cerr << error << "\n";
		   return false;
		 };
    std::vector<DFS::drive_number> todo;
    if (args.size() > 1)
      {
	bool first = true;
	for (const std::string& arg : args)
	  {
	    if (first)
	      {
		first = false;
		continue;      // this is the command name, ignore it.
	      }
	    DFS::drive_number d(0);
	    error.clear();
	    if (!DFS::StorageConfiguration::decode_drive_number(arg, &d, error))
	      return fail();
	    if (!error.empty())
	      std::cerr << "warning: " << error << "\n";
	    todo.push_back(d);
	  }
      }
    else
      {
	todo = storage.get_all_occupied_drive_numbers();
      }

    for (DFS::drive_number drive_num : todo)
      {
	if (!show_title(storage, drive_num, error))
	  return faild(drive_num);
      }
    return true;
  }

};
REGISTER_COMMAND(CommandShowTitles);

} // namespace
