#include "commands.h"

#include <fstream>
#include <iomanip>
#include <sstream>

#include <string.h>

#include "cleanup.h"
#include "dfscontext.h"
#include "dfs_volume.h"
#include "dfs_unused.h"
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

  bool invoke(const DFS::StorageConfiguration& storage,
	      const DFS::DFSContext& ctx,
	      const std::vector<std::string>& args) override
  {
    if (ctx.current_volume.subvolume())
      {
	std::cerr << name() << ": please specify only a drive number, not also a volume letter.\n";
	return false;
      }

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

    const DFS::SurfaceSelector surface(ctx.current_volume.surface());
    std::string error;
    auto fail = [&surface, &error]()
		{
		  std::cerr << "failed to select drive " << surface
			    << ": " << error << "\n";
		  return false;
		};
    DFS::AbstractDrive *drive = 0;
    if (!storage.select_drive(surface, &drive, error))
      return fail();
    auto mounted_fs = storage.mount_fs(surface, error);
    if (!mounted_fs)
      return fail();

    // We're going to loop over the unoccupied areas of the disc,
    // extracting each.  We add a sentinel value to ensure that we
    // also extract the final free span, if any (this avoids
    // duplication of code).
    std::unique_ptr<DFS::SectorMap> occupied_by = mounted_fs->get_sector_map(surface);

    const sector_count_type last_sec = mounted_fs->disc_sector_count();
    occupied_by->add_other(last_sec, ":::end");
    int begin = -1;
    unsigned short count = 0;
    for (DFS::sector_count_type sec = 0; sec <= last_sec; ++sec)
      {
	std::optional<std::string> name = occupied_by->at(sec);
	if (name)
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
    ostream_flag_saver restore_cout_flags(std::cout);
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

    for (sector_count_type sec = start_sector; sec < end_sector; ++sec)
      {
	auto got = drive->read_block(sec);
	if (!got)
	  {
	    std::cerr << "warning: media ("
		      << drive->geometry().total_sectors() << " sectors) is shorter "
		      << "than file system (" << end_sector << " sectors)\n";
	    break;
	  }
	errno = 0;
	output.write(reinterpret_cast<char*>(got->data()), got->size());
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
