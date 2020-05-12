#include "commands.h"

#include <iomanip>
#include <iostream>
#include <locale>
#include <string>

#include "dfsimage.h"

namespace
{
struct comma_thousands : std::numpunct<char>
{
  char do_thousands_sep() const
  {
    return ',';
  }

  std::string do_grouping() const
  {
    return "\3";		// groups of 3 digits
  }
};

class CommandFree : public DFS::CommandInterface // *FREE
{
public:
  const std::string name() const override
  {
    return "free";
  }

  const std::string usage() const override
  {
    return name() + " [drive]\n"
      "The used/free space shown reflects the position of the last file on the disc.\n"
      "Using *COMPACT or a similar tool on the disc may free up additional space.\n";
  }

  const std::string description() const override
  {
    return "display information about a disc's free space";
  }

  bool operator()(const DFS::StorageConfiguration& storage,
		  const DFS::DFSContext& ctx,
		  const std::vector<std::string>& args) override
  {
    DFS::AbstractDrive *drive;
    if (args.size() > 2)
      {
	std::cerr << "at most one command-line argument is needed.\n";
	return false;
      }
    if (args.size() < 2)
      {
	if (!storage.select_drive(ctx.current_drive, &drive))
	  return false;
      }
    else
      {
	if (!storage.select_drive_by_number(args[1], &drive))
	  return false;
      }
    const DFS::FileSystem file_system(drive);
    const DFS::FileSystem* fs = &file_system; // TODO: this is a bit untidy

    int sectors_used = 2;
    const int entries = fs->catalog_entry_count();
    for (int i = 1; i <= entries; ++i)
      {
	const auto& entry = fs->get_catalog_entry(i);
	ldiv_t division = ldiv(entry.file_length(), DFS::SECTOR_BYTES);
	const int sectors_for_this_file = division.quot + (division.rem ? 1 : 0);
	const int last_sector_of_file = entry.start_sector() + sectors_for_this_file;
	if (last_sector_of_file > sectors_used)
	  {
	    sectors_used = last_sector_of_file;
	  }
      }
    int files_free = fs->max_file_count() - fs->catalog_entry_count();
    int sectors_free = fs->disc_sector_count() - sectors_used;
    std::cout << std::uppercase;
    auto show = [](int files, int sectors, const std::string& desc)
		{
		  std::cout << std::setw(2) << std::setfill('0') << std::dec
			    << files << " Files "
			    << std::setw(3) << std::setfill('0') << std::hex
			    << sectors << " Sectors "
			    << std::setw(7) << std::setfill(' ') << std::dec
			    << (sectors * DFS::SECTOR_BYTES)
			    << " Bytes " << desc << "\n";
		};

    auto prevlocale = std::cout.imbue(std::locale(std::cout.getloc(),
					     new comma_thousands)); // takes ownership
    show(files_free, sectors_free, "Free");
    show(fs->catalog_entry_count(), sectors_used, "Used");
    std::cout.imbue(prevlocale);
    return true;
  }
};
REGISTER_COMMAND(CommandFree);

} // namespace
