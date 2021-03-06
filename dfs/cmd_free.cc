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
#include <assert.h>         // for assert
#include <stdlib.h>         // for div, div_t
#include <iomanip>          // for operator<<, setfill, setw
#include <iostream>         // for operator<<, basic_ostream, ostream, ...
#include <limits>           // for numeric_limits
#include <locale>           // for numpunct, locale
#include <optional>         // for optional
#include <string>           // for string, operator<<, allocator, char_traits
#include <vector>           // for vector

#include "abstractio.h"     // for SECTOR_BYTES
#include "cleanup.h"        // for ostream_flag_saver
#include "commands.h"       // for CommandInterface, REGISTER_COMMAND
#include "dfs_catalog.h"    // for CatalogEntry, Catalog
#include "dfs_volume.h"     // for Volume
#include "dfscontext.h"     // for DFSContext
#include "driveselector.h"  // for operator<<, VolumeSelector
#include "storage.h"        // for StorageConfiguration, VolumeMountResult

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

  bool invoke(const DFS::StorageConfiguration& storage,
	      const DFS::DFSContext& ctx,
	      const std::vector<std::string>& args) override
  {
    std::string error;
    auto fail = [&error]()
		 {
		   std::cerr << error << "\n";
		   return false;
		 };

    DFS::VolumeSelector vol(0);
    if (args.size() > 2)
      {
	std::cerr << "at most one command-line argument is needed.\n";
	return false;
      }
    if (args.size() < 2)
      {
	vol = ctx.current_volume;
      }
    else
      {
	error.clear();
	if (!DFS::StorageConfiguration::decode_drive_number(args[1], &vol, error))
	  return fail();
	if (!error.empty())
	  std::cerr << "warning: " << error << "\n";
      }
    auto mounted = storage.mount(vol, error);
    if (!mounted)
      {
	failed_to_mount_volume(std::cerr, vol, error);
	return false;
      }
    auto catalog(mounted->volume()->root());

    int sectors_used = 2;
    const std::vector<DFS::CatalogEntry> entries = catalog.entries();
    for (const auto& entry : entries)
      {
	assert(entry.file_length() < std::numeric_limits<int>::max());
	div_t division = div(static_cast<int>(entry.file_length()), DFS::SECTOR_BYTES);
	const int sectors_for_this_file = division.quot + (division.rem ? 1 : 0);
	const int last_sector_of_file = entry.start_sector() + sectors_for_this_file;
	if (last_sector_of_file > sectors_used)
	  {
	    sectors_used = last_sector_of_file;
	  }
      }
    ostream_flag_saver restore_cout_flags(std::cout);
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
    assert(entries.size() < std::numeric_limits<int>::max());
    int files_used = static_cast<int>(entries.size());
    int files_free = catalog.max_file_count() - files_used;
    int sectors_free = catalog.total_sectors() - sectors_used;
    std::cout << std::uppercase;
    show(files_free, sectors_free, "Free");
    show(files_used, sectors_used, "Used");
    std::cout.imbue(prevlocale);
    return true;
  }
};
REGISTER_COMMAND(CommandFree);

} // namespace
