#include "commands.h"

#include <string.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "afsp.h"
#include "dfs.h"

using std::cerr;
using std::cout;


class CommandSectorMap : public DFS::CommandInterface
{
public:
  const std::string name() const override
  {
    return "sector-map";
  }

  const std::string usage() const override
  {
    return "usage: " + name() + " drive\n"
      "Displays a map of which sectors store which files.\n"
      "The left hand column shows the sector numbers (in hex)\n"
      "and the rest of the output shows which file occupies that\n"
      "sector (using the same ordering as the 'info' command)\n";
  }

  const std::string description() const override
  {
    return "show where each file is stored on disc";
  }

  bool operator()(const DFS::StorageConfiguration& storage,
		  const DFS::DFSContext& ctx,
		  const std::vector<std::string>& args) override
  {
    unsigned drive_number;
    DFS::AbstractDrive *drive;
    if (args.size() > 2)
      {
	std::cerr << "Please specify at most one argument, the drive number\n";
	return false;
      }
    else if (args.size() == 2)
      {
	if (!DFS::StorageConfiguration::decode_drive_number(args[1], &drive_number))
	  return false;
      }
    else
      {
	drive_number = ctx.current_drive;
      }
    if (!storage.select_drive(drive_number, &drive))
      return false;
    DFS::FileSystem fs(drive);
    const auto& catalog(fs.root());
    const std::vector<std::optional<int>> occupied_by = catalog.sector_to_global_catalog_slot_mapping();

    int column = 0;
    constexpr int max_col = 5;
    constexpr int name_col_width = 9;
    constexpr char sector_col_header[] = "Sector";
    assert(strlen(sector_col_header) < std::numeric_limits<int>::max());
    const int sector_col_width =
      std::max(6, static_cast<int>(strlen(sector_col_header)));
    std::cout << std::setw(sector_col_width) << sector_col_header << ":\n"
	      << std::setw(sector_col_width) << "(dec)" << ": "
	      << "Name of file occupying each sector\n";
    for (DFS::sector_count_type sec = 0; sec < occupied_by.size(); ++sec)
      {
	if (0 == column)
	  {
	    if (sec > 0)
	      std::cout << "\n";
	    std::cout << std::dec
		      << std::right << std::setfill('0')
		      << std::setw(sector_col_width)
		      << sec << ": ";
	  }

	std::string name = "?";
	if (!occupied_by[sec])
	  {
  	    name = "-";
	  }
	else if (*occupied_by[sec] == DFS::Catalog::global_catalog_slot_self)
	  {
	    name = "catalog";
	  }
	else
	  {
	    assert(*occupied_by[sec] > 0);
	    assert(*occupied_by[sec] < std::numeric_limits<unsigned short>::max());
	    auto e = catalog.get_global_catalog_entry(static_cast<unsigned short>(*occupied_by[sec]));
	    name = std::string(1, e.directory()) + "." + e.name();
	    assert(name.size() <= name_col_width);
	  }
	std::cout << std::setw(name_col_width) << std::setfill(' ') << std::left << name << ' ';
	if (++column == max_col)
	  column = 0;
      }
    std::cout << "\n";
    return std::cout.good();
  }
};
REGISTER_COMMAND(CommandSectorMap);
