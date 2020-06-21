#include "commands.h"

#include <sstream>
#include <string>
#include <vector>

#include "dfscontext.h"
#include "dfs_volume.h"
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
		  const DFS::SurfaceSelector& d, std::string& error)
  {
    DFS::VolumeSelector vol(d);
    auto mounted(storage.mount(vol, error));
    if (!mounted)
      return false;
    std::cout << d << ":" << mounted->volume()->root().title() << "\n";
    if (mounted->file_system()->disc_format() == DFS::Format::OpusDDOS)
      {
	// TODO: fix this for Opus DDOS.
	// For Opus DDOS, we should probably iterate over all the
	// volumes on the drive.
	std::cerr << "warning: this disc may contain other volumes too.\n";
      }
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
	    DFS::VolumeSelector vol(0);
	    error.clear();
	    // TODO: for Opus DDOS, allow the user to request all
	    // volumes on a drive.
	    if (!DFS::StorageConfiguration::decode_drive_number(arg, &vol, error))
	      return fail();
	    if (!error.empty())
	      std::cerr << "warning: " << error << "\n";
	    if (vol.subvolume())
	      {
		std::cerr << "Opus DDOS volumes are not yet supported.\n";
		return false;
	      }
	    todo.push_back(vol.surface());
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
