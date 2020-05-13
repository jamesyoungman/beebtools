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
    return "sector_map";
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
    DFS::AbstractDrive *drive;
    if (args.size() > 2)
      {
	std::cerr << "Please specify at most one argument, the drive number\n";
	return false;
      }
    else if (args.size() == 2)
      {
	if (!storage.select_drive_by_afsp(args[1], &drive, ctx.current_drive))
	  return false;
      }
    else
      {
	if (!storage.select_drive(ctx.current_drive, &drive))
	  return false;
      }
    DFS::FileSystem file_system(drive);
    const int sector_unused = -1;
    const int sector_catalogue = -2;
    // occupied_by is a mapping from sector number to catalog position.
    std::vector<int> occupied_by(file_system.disc_sector_count(), sector_unused);
    const int entries = file_system.catalog_entry_count();
    occupied_by[0] = occupied_by[1] = sector_catalogue;
    if (file_system.disc_format() == DFS::Format::WDFS)
      occupied_by[2] = occupied_by[3] = sector_catalogue;
    for (int i = 1; i <= entries; ++i)
      {
	const auto& entry = file_system.get_catalog_entry(i);
	ldiv_t division = ldiv(entry.file_length(), DFS::SECTOR_BYTES);
	const int sectors_for_this_file = division.quot + (division.rem ? 1 : 0);
	assert(sectors_for_this_file < file_system.disc_sector_count());
	assert(entry.start_sector() < occupied_by.size());
	assert(entry.start_sector() + sectors_for_this_file < occupied_by.size());
	std::fill(occupied_by.begin() + entry.start_sector(),
		  occupied_by.begin() + entry.start_sector() + sectors_for_this_file,
		  i);
      }
    int column = 0;
    constexpr int max_col = 8;
    constexpr int name_col_width = 7;
    constexpr char sector_col_header[] = "Sector";
    const auto sector_col_width = std::max(size_t(6), strlen(sector_col_header));
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
	switch (occupied_by[sec])
	  {
	  case sector_unused:
  	    name = "-";
	    break;

	  case sector_catalogue:
	    name = "catalog";
	    break;

	  default:
	    {
	      auto e = file_system.get_catalog_entry(occupied_by[sec]);
	      name = std::string(1, e.directory()) + e.name();
	      assert(name.size() <= name_col_width);
	      break;
	    }
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
