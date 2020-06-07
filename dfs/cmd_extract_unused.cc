#include "commands.h"

#include <fstream>
#include <iomanip>
#include <sstream>

#include <string.h>

#include "dfscontext.h"
#include "dfs_filesystem.h"
#include "dfstypes.h"
#include "media.h"

namespace
{
using DFS::sector_count_type;
using DFS::sector_count;

std::string make_name(const std::string& dest_dir, sector_count_type first_sector)
{
  assert(dest_dir.back() == '/');
  std::ostringstream ss;
  ss << dest_dir << "unused_" << std::hex << std::noshowbase
     << std::uppercase << std::setfill('0') << std::setw(3)
     << first_sector << ".bin";
  return ss.str();
}

class CommandExtractUnused : public DFS::CommandInterface
{
public:
  const std::string name() const override
  {
    return "extract-unused";
  }

  const std::string usage() const override
  {
    return "usage: " + name() + " destination-directory\n"
      "For each span of unused space in the selected drive\n"
      "(see the --drive global option), write a file into\n"
      "destination-directory.\n"
      "The output files are given names corresponding to the\n"
      "first sector they occupy (such as unused_1E4.bin).\n";
  }

  const std::string description() const override
  {
    return "extract a copy of unused areas of the disc";
  }

  bool operator()(const DFS::StorageConfiguration& storage,
		  const DFS::DFSContext& ctx,
		  const std::vector<std::string>& args) override
  {
    // Use the --drive option to select which drive to extract data from.
    if (args.size() < 2)
      {
	std::cerr << name() << ": please specify the destination directory.\n";
	return false;
      }
    if (args.size() > 2)
      {
	std::cerr << name() << ": just one argument (the destination "
		  << "directory) is needed.\n";
	return false;
      }
    std::string dest_dir(args[1]);
    if (dest_dir.back() != '/')
      dest_dir.push_back('/');

    DFS::AbstractDrive* drive = 0;
    if (!storage.select_drive(ctx.current_drive, &drive))
      return false;
    assert(0 != drive);

    DFS::FileSystem fs(drive);
    std::vector<int> occupied_by = fs.sector_to_catalog_entry_mapping();
    int begin = -1;
    unsigned short count = 0;
    // We're going to loop over the unoccupied areas of the disc,
    // extracting each.  We add a sentinel value to ensure that we
    // also extract the final free span, if any (this avoids
    // duplication of code).
    occupied_by.push_back(std::numeric_limits<int>::max()); // sentinel
    for (DFS::sector_count_type sec = 0; sec < occupied_by.size(); ++sec)
      {
	if (occupied_by[sec] != DFS::FileSystem::sector_unused)
	  {
	    if (begin >= 0)
	      {
		if (!write_span(drive, dest_dir, sector_count(begin), sec))
		  return false;
		++count;
	      }
	    begin = -1;
	    continue;
	  }
	else if (begin < 0)
	  {
	    begin = sec;
	  }
      }
    std::cout << std::dec << count << " files were written to "
	      << dest_dir << "\n";
    return true;
  }

private:
  bool write_span(DFS::AbstractDrive *drive,
		  const std::string& dest_dir,
		  sector_count_type start_sector,
		  // end_sector is the first sector not included.
		  sector_count_type end_sector)
  {
    assert(start_sector < end_sector);
    const std::string file_name(make_name(dest_dir, start_sector));
    std::ofstream output(file_name, std::ofstream::binary|std::ofstream::trunc);
    if (!output)
      {
	std::cerr << "unable to create output file " << file_name << "\n";
	return false;
      }

    DFS::AbstractDrive::SectorBuffer buf;
    for (sector_count_type sec = start_sector; sec < end_sector; ++sec)
      {
	bool beyond_eof = false;
	drive->read_sector(sec, &buf, beyond_eof);
	if (beyond_eof)
	  {
	    std::cerr << "warning: media (" << drive->get_total_sectors() << " sectors) is shorter "
		      << "than file system (" << end_sector << " sectors)\n";
	    break;
	  }
	errno = 0;
	output.write(reinterpret_cast<char*>(buf.data()), buf.size());
	if (!output)
	  break;
      }
    output.close();
    if (!output)
      {
	std::cerr << "error: failed to write to " << file_name << ": "
		  << strerror(errno) << "\n";
	return false;
      }
    return true;
  }

};
  REGISTER_COMMAND(CommandExtractUnused);
}  // namespace
