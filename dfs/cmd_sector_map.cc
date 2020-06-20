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
#include "driveselector.h"

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
    DFS::drive_number drive_num(0);
    if (args.size() > 2)
      {
	std::cerr << "at most one command-line argument is needed.\n";
	return false;
      }
    if (args.size() < 2)
      {
	drive_num = ctx.current_drive;
      }
    else
      {
	error.clear();
	if (!DFS::StorageConfiguration::decode_drive_number(args[1], &drive_num, error))
	  return fail();
	if (!error.empty())
	  std::cerr << "warning: " << error << "\n";
      }
    auto file_system(storage.mount(drive_num, error));
    if (!file_system)
      return faild(drive_num);
    auto catalog(file_system->root());

    const std::vector<DFS::CatalogEntry> entries(catalog.entries());
    typedef std::vector<DFS::CatalogEntry>::size_type catalog_entry_index;
    std::vector<std::optional<std::vector<DFS::CatalogEntry>::size_type>> occupied_by;
    occupied_by.resize(file_system->disc_sector_count());
    catalog_entry_index occ_by_catalog = entries.size();
    for (std::vector<DFS::CatalogEntry>::size_type i = 0;
	 i < entries.size();
	 ++i)
      {
	const DFS::CatalogEntry& entry(entries[i]);
	assert(entry.start_sector() < occupied_by.size());
	assert(entry.last_sector() < occupied_by.size());
	std::fill(occupied_by.begin() + entry.start_sector(),
		  occupied_by.begin() + entry.last_sector() + 1,
		  i);
      }
    for (DFS::sector_count_type sec : catalog.get_sectors_occupied_by_catalog())
      occupied_by[sec] = occ_by_catalog;

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
	else if (*occupied_by[sec] == occ_by_catalog)
	  {
	    name = "catalog";
	  }
	else
	  {
	    assert(*occupied_by[sec] <= entries.size());
	    const auto& e = entries[*occupied_by[sec]];
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
