#include "storage.h"

#include <string.h>		// strerror

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "dfsimage.h"
#include "dfstypes.h"

using std::vector;

using DFS::byte;

namespace
{
  void connect_internal(std::unique_ptr<DFS::AbstractDrive>* where,
			std::unique_ptr<DFS::AbstractDrive>&& incoming)
  {
    *where = std::move(incoming);
  }

  bool full()
  {
    std::cerr << "There is not enough room to attach more drives.\n";
    return false;
  }
}


namespace DFS
{
  StorageConfiguration::StorageConfiguration()
  {
  }

  bool StorageConfiguration::connect_drive(std::unique_ptr<AbstractDrive>&& media)
  {
    // Attach at the next free spot.
    for (unsigned i = 0; i < drives_.size(); ++i)
      {
	if (!drives_[i])
	  {
	    connect_internal(&drives_[i], std::move(media));
	    return true;
	  }
      }
    return full();
  }

  bool StorageConfiguration::select_drive(unsigned int drive, AbstractDrive **pp) const
  {
    if (drive >= drives_.size())
      {
	std::cerr << "There is no drive " << drive << "\n";
	return false;
      }
    AbstractDrive *p = drives_[drive].get();
    if (0 == p)
      {
	std::cerr << "There is no disc in drive " << drive << "\n";
	return false;
      }
    *pp = p;
    return true;
  }

  bool StorageConfiguration::decode_drive_number(const std::string& drive_arg, unsigned* num)
  {
    if (drive_arg.size() == 1 && isdigit(drive_arg[0]))
      {
	*num = (drive_arg[0] - '0');
	return true;
      }
    else if (drive_arg.size() > 1 && drive_arg[0] == ':' && isdigit(drive_arg[1]))
      {
	*num = (drive_arg[1] - '0');
	return true;
      }
    else
      {
	std::cerr << "Please specify a drive number (" << drive_arg
		  << " doesn't look like a drive number)\n";
	return false;
      }
  }

  bool StorageConfiguration::select_drive_by_afsp(const std::string& afsp, AbstractDrive **pp, int current) const
  {
    unsigned drive_wanted;
    if (afsp.size() > 1 && afsp[0] == ':' && isdigit(afsp[1]))
      {
	drive_wanted = afsp[1] - '0';
      }
    else
      {
	std::cerr << "warning: " << afsp
		  << " doesn't look like a wildcard containing a drive "
		  << "specification, defaulting to drive " << current << "\n";
	drive_wanted = current;
      }
    return select_drive(drive_wanted, pp);
  }

  void StorageConfiguration::show_drive_configuration(std::ostream& os) const
  {
    unsigned drive;
    using std::cout;
    for (drive = 0; drive < drives_.size(); ++drive)
      {
	os << "Drive " << drive << ": "
	   << (drives_[drive] ? "occupied" : "empty");
	auto image = drives_[drive].get();
	if (0 != image)
	  {
	    os << ", " << image->description();
	  }
	os << "\n";
      }
  }

}  // namespace DFS
