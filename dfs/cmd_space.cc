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
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "afsp.h"
#include "cleanup.h"
#include "dfs.h"
#include "dfscontext.h"
#include "dfs_catalog.h"
#include "dfs_volume.h"
#include "driveselector.h"
#include "storage.h"

using std::cerr;
using std::cout;

namespace
{
  std::optional<std::vector<DFS::VolumeSelector>>
  select_volumes(const DFS::DFSContext& ctx,
		 const std::vector<std::string>& args,
		 std::string& error)
  {
    std::vector<DFS::VolumeSelector> result;
    if (args.size() < 2)
      {
	result.push_back(ctx.current_volume);
	return result;
      }

    DFS::VolumeSelector vol(ctx.current_volume);
    bool first = true;
    for (const std::string& arg : args)
      {
	if (first)  // the command name.
	  {
	    // TODO: none of the command implementation use this
	    // argument, so we should simplify the code by not passing
	    // it.
	    first = false;
	    continue;
	  }

	error.clear();
	if (!DFS::StorageConfiguration::decode_drive_number(arg, &vol, error))
	  return std::nullopt;
	result.push_back(vol);
	if (!error.empty())
	  std::cerr << "warning: " << error << "\n";
      }
    return result;
  }
}  // namespace

// "space" is like the Watford DFS command HELP SPACE,
class CommandSpace : public DFS::CommandInterface
{
public:
  const std::string name() const override
  {
    return "space";
  }

  const std::string usage() const override
  {
    return "usage: " + name() + " drive [drive...]\n"
      "Displays a list of spaces between files.  More than one drive can be specified.\n";
  }

  const std::string description() const override
  {
    return "show spaces between files";
  }

  bool invoke(const DFS::StorageConfiguration& storage,
	      const DFS::DFSContext& ctx,
	      const std::vector<std::string>& args) override
  {
    ostream_flag_saver restore_cerr_flags(std::cerr);
    ostream_flag_saver restore_cout_flags(std::cout);
    std::string error;
    std::map<DFS::VolumeSelector, DFS::sector_count_type> free_space;
    std::optional<std::vector<DFS::VolumeSelector>> selected =
      select_volumes(ctx, args, error);
    if (!selected)
      {
	std::cerr << error << "\n";
	return false;
      }
    for (const DFS::VolumeSelector& selector : *selected)
      {
	auto mounted = storage.mount(selector, error);
	if (!mounted)
	  {
	    std::cerr << error << "\n";
	    return false;
	  }
	auto root(mounted->volume()->root());

	// Files occur on the disk in a kind of reverse order.  The last
	// file on the disk is the first one mentioned in the catalog in
	// sector 3.  The last file in that catalog occurs immediately
	// after the first file mentioned in the catalog in sector 1.  The
	// last file mentioned in the catalog in sector 1 is the last file
	// on the disk.
	//
	// We're not set up to easily iterate over catalog entries in that
	// order because the FileSystem interface mostly hides the detail
	// of how many catalog sectors there are.
	//
	// The actual output of the (WDFS) HELP SPACE command is actually
	// generated in an order we find more convenient: starting at the
	// last catalog entry in the catalog in sector 3, (which we number
	// as the highest slot) and working back towards the first entry
	// in the catalog in sector 0 (which we number as slot 0).
	//
	// Note that indexing within catalogs[][] is 0-based, unlike the
	// normal usage for DFS catalogs, because the 0-entry for the disc
	// title is not included.
	const std::vector<std::vector<DFS::CatalogEntry>> catalogs =
	  root.get_catalog_in_disc_order();
	assert(catalogs.size() <= std::numeric_limits<int>::max());
	auto start_sec_of_next = [&catalogs, &root]
	  (unsigned int catalog, unsigned int entry) -> DFS::sector_count_type
				 {
				   assert(catalog < catalogs.size());
				   assert(entry <= catalogs[catalog].size());
				   if (entry > 0)
				     return catalogs[catalog][entry-1].start_sector();
				   if (catalog == catalogs.size()-1)
				     return root.total_sectors();
				   return catalogs[catalog+1].back().start_sector();
				 };
	std::vector<unsigned int> gaps;
	auto maybe_gap = [&gaps](DFS::sector_count_type last,
				 DFS::sector_count_type next)
			 {
			   if (last > next)
			     throw DFS::BadFileSystem("catalog entries are out of order");
			   unsigned int gap = next - last;
			   if (gap)
			     {
			       gaps.push_back(gap);
			     }
			 };

	bool added_initial_gap = false;
	auto add_initial_gap =
	  [&root, &catalogs, maybe_gap, &gaps, &added_initial_gap]
	  (std::optional<std::pair<int, int>> first_file)
	  {
	    assert(!added_initial_gap);
	    auto following = root.total_sectors();
	    if (first_file)
	      {
		const auto& ce(catalogs[first_file->first][first_file->second]);
		following = ce.start_sector();
	      }
	    auto cat_sectors = data_sectors_reserved_for_catalog(root.disc_format());
	    maybe_gap(cat_sectors, following);
	    added_initial_gap = true;
	  };

	std::optional<std::pair<int, int>> first_file_entry_pos; // catalog #, entry #
	std::optional<DFS::sector_count_type> first_file_sec;
	for (int c = 0; c < static_cast<int>(catalogs.size()); ++c)
	  {
	    for (int entry = 0;
		 entry < static_cast<int>(catalogs[c].size());
		 ++entry)
		   {
		     auto sec = catalogs[c][entry].start_sector();
		     if (first_file_sec)
		       {
			 if (sec < *first_file_sec)
			   {
			     first_file_sec = sec;
			     first_file_entry_pos = std::make_pair(c, entry);
			   }
		       }
		     else
		       {
			 first_file_sec = sec;
			 first_file_entry_pos = std::make_pair(c, entry);
		       }
		   }
	  }

	assert(catalogs.size() < std::numeric_limits<int>::max());
	if (catalogs.size())  // check subtraction below won't wrap.
	  {
	    for (int c = static_cast<int>(catalogs.size())-1; c >= 0; --c)
	      {
		assert(catalogs[c].size() < std::numeric_limits<int>::max());
		for (int entry = static_cast<int>(catalogs[c].size())-1; entry >= 0; --entry)
		  {
		    assert(first_file_entry_pos);
		    // Watford DFS seems to emit the initial gap with
		    // an odd ordering.  Hence this condition is a bit
		    // unexpected.
		    if (c == first_file_entry_pos->first &&
			entry == first_file_entry_pos->second &&
			first_file_entry_pos->first == 0)
		      {
			// Account for any gap between the catalog and the
			// file with the lowest start sector.
			add_initial_gap(first_file_entry_pos);
			added_initial_gap = true;
		      }
		    auto last_sec = catalogs[c][entry].last_sector();
		    maybe_gap(last_sec+1, start_sec_of_next(c, entry));
		  }
	      }
	  }
	if (!added_initial_gap)
	  {
	    add_initial_gap(first_file_entry_pos);
	  }

	std::cout << "Gap sizes on disc " << selector << ":\n";
	bool first = true;
	ostream_flag_saver restore_cout_flags(std::cout);
	std::cout << std::hex << std::uppercase << std::setfill('0');
	for (const auto& gap : gaps)
	  {
	    if (!first)
	      std::cout << ' ';
	    first = false;
	    std::cout << std::setw(3) << gap;
	  }
	auto free_sectors = std::accumulate(gaps.cbegin(), gaps.cend(), 0);
	std::cout << "\n\nTotal space free = " << free_sectors << " sectors\n";
	free_space[selector] = free_sectors;
      }
    if (selected->size() > 1)
      {
	std::cout << std::hex << std::uppercase;
	DFS::sector_count_type tot_free = 0;
	std::cout << std::setfill(' ');
	for (const auto& f : free_space)
	  {
	    std::cout << "Total space free in volume "
		      << std::setfill(' ') << std::setw(4) << f.first
		      << " = " << std::setw(4) << std::setfill('0')
		      << f.second << " sectors\n";
	    tot_free += f.second;
	  }
	std::cout << "Total space free in all volumes = "
		  << std::setfill('0') << std::setw(4) << tot_free
		  << " sectors\n";
      }
    return std::cout.good();
  }
};
REGISTER_COMMAND(CommandSpace);
