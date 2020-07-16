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
#include "commands.h"

#include <string.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "afsp.h"
#include "cleanup.h"
#include "dfs.h"
#include "dfs_volume.h"
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

  bool invoke(const DFS::StorageConfiguration& storage,
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
    DFS::SurfaceSelector drive_num(0);
    if (args.size() > 2)
      {
	std::cerr << "at most one command-line argument is needed.\n";
	return false;
      }
    if (args.size() < 2)
      {
	if (ctx.current_volume.subvolume())
	  {
	    std::cerr << "Please specify only a drive number with --drive(to get a sector map of the whole drive).\n";
	    return false;
	  }
	drive_num = ctx.current_volume.surface();
      }
    else
      {
	error.clear();
	size_t end;
	auto got = DFS::SurfaceSelector::parse(args[1], &end, error);
	if (!got)
	  return fail();
	drive_num = *got;
	if (!error.empty())
	  std::cerr << "warning: " << error << "\n";
	if (end != args[1].size())
	  {
	    std::cerr << "trailing junk after drive number " << args[1] << "\n";
	    return false;
	  }
      }
    std::unique_ptr<DFS::FileSystem> fs = (storage.mount_fs(drive_num, error));
    if (!fs)
      return faild(drive_num);
    std::unique_ptr<DFS::SectorMap> sector_map = fs->get_sector_map(drive_num);

    int column = 0;
    // Ensure that a track is an integer number of lines of output
    // (3 for double density, 2 for single density).
    const int track_len = fs->geometry().sectors;
    const int max_col = (track_len == 18) ? 6 : (track_len == 16) ? 4 : 5;
    constexpr int name_col_width = 12;
    constexpr char sector_col_header[] = "Sector";
    assert(strlen(sector_col_header) < std::numeric_limits<int>::max());
    const int sector_col_width =
      std::max(6, static_cast<int>(strlen(sector_col_header)));
    std::cout << std::setw(sector_col_width) << sector_col_header << ":\n"
	      << std::setw(sector_col_width) << "(dec)" << ": "
	      << "Name of file occupying each sector\n";
    const auto sectors = fs->disc_sector_count();
    ostream_flag_saver restore_cout_flags(std::cout);
    for (DFS::sector_count_type sec = 0; sec < sectors; ++sec)
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

	const std::string name = sector_map->at(sec).value_or("-");
	std::cout << std::setw(name_col_width) << std::setfill(' ') << std::left << name << ' ';
	if (++column == max_col)
	  column = 0;
      }
    std::cout << "\n";
    return std::cout.good();
  }
};
REGISTER_COMMAND(CommandSectorMap);
