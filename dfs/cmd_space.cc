#include "commands.h"

#include <string.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "afsp.h"
#include "dfs.h"

using std::cerr;
using std::cout;


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
    return "usage: " + name() + " drive\n"
      "Displays a list of spaces between files.\n";
  }

  const std::string description() const override
  {
    return "show spaces between files";
  }

  bool operator()(const DFS::StorageConfiguration& storage,
		  const DFS::DFSContext& ctx,
		  const std::vector<std::string>& args) override
  {
    DFS::AbstractDrive *drive;
    unsigned int drive_num;
    if (args.size() > 2)
      {
	std::cerr << "Please specify at most one argument, the drive number\n";
	return false;
      }
    else if (args.size() == 2)
      {
	if (!DFS::StorageConfiguration::decode_drive_number(args[1], &drive_num))
	  return false;
	if (!storage.select_drive(drive_num, &drive))
	  return false;
      }
    else
      {
	if (!storage.select_drive(ctx.current_drive, &drive))
	  return false;
	drive_num = ctx.current_drive;
      }
    DFS::FileSystem file_system(drive);
    const auto& root(file_system.root());

    std::cerr << "Disc total sectors = "
       << std::setw(3) << std::hex << std::uppercase
       << file_system.disc_sector_count() << "\n";
    std::cerr << "Disc sectors occupied by catalog = "
	      << std::setw(3) << std::hex << std::uppercase
	      << root.catalog_sectors() << "\n";
    std::cerr << "Total file storage space in sectors = "
	      << (file_system.disc_sector_count()
		  - root.catalog_sectors()) << "\n";

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
    const std::vector<std::vector<DFS::CatalogEntry>> catalogs = root.get_catalog_in_disc_order();
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
		       assert(last < next);
		       unsigned int gap = next - (last + 1);
		       if (gap)
			 {
			   gaps.push_back(gap);
			 }
		     };

    assert(catalogs.size() < std::numeric_limits<int>::max());
    if (catalogs.size())  // check subtraction below won't wrap.
      {
	for (int c = static_cast<int>(catalogs.size())-1; c >= 0; --c)
	  {
	    if (c == 0)
	      {
		// Account for any gap between the catalog and the
		// first file of the first catalog.
		//
		// TODO: this won't work correctly for Opus, where the
		// volume catalog and the file storage are on
		// different parts of the disc.
		maybe_gap(DFS::sector_count(root.catalog_sectors() - 1u),
			  catalogs[0].back().start_sector());
	      }
	    assert(catalogs[c].size() < std::numeric_limits<int>::max());
	    for (int entry = static_cast<int>(catalogs[c].size())-1; entry >= 0; --entry)
	      {
		auto last_sec = catalogs[c][entry].last_sector();
		maybe_gap(last_sec, start_sec_of_next(c, entry));
	      }
	  }
      }

    std::cout << "Gap sizes on disc " << drive_num << ":\n";
    bool first = true;
    for (const auto& gap : gaps)
      {
	if (!first)
	  std::cout << ' ';
	first = false;
	std::cout << std::setw(3) << std::hex << std::uppercase
		  << std::setfill('0') << gap;
    }

    std::cout << "\n\nTotal space free = "
	      << std::hex << std::uppercase
	      << std::accumulate(gaps.cbegin(), gaps.cend(), 0)
	      << " sectors\n";
    return std::cout.good();
  }
};
REGISTER_COMMAND(CommandSpace);
