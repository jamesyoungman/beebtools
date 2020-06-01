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

  bool StorageConfiguration::connect_drive(std::unique_ptr<AbstractDrive>&& media,
					   DriveAllocation how)
  {
    // Attach at the next free spot.
    const auto limit = std::numeric_limits<drive_number>::max();
    drive_number selected = 0;
    if (!drives_.empty())
      {
	auto last_drive = drives_.rbegin();
	selected = last_drive->first;
	if (selected == limit)
	  return full();
	++selected;

	if (how == DriveAllocation::EVEN)
	  {
	    // Put the new disk in an even numbered drive.
	    if (selected % 2)
	      {
		if (selected == limit)
		  return full();
		++selected;
	      }
	  }
      }
    connect_internal(&drives_[selected], std::move(media));
    return true;
  }

  bool StorageConfiguration::select_drive(unsigned int drive, AbstractDrive **pp) const
  {
    auto it = drives_.find(drive);
    if (it == drives_.end() || !it->second)
      {
	std::cerr << "There is no disc in drive " << drive << "\n";
	return false;
      }
    *pp = it->second.get();
    return true;
  }

  bool StorageConfiguration::decode_drive_number(const std::string& drive_arg, unsigned* num)
  {
    std::string s;
    if (drive_arg.size() > 0 && drive_arg[0] == ':')
      {
	s.assign(drive_arg.substr(1));
      }
    else
      {
	s.assign(drive_arg);
      }
    size_t end;
    unsigned long int d = std::stoul(s, &end, 10);
    if (end != s.size())
      {
	std::cerr << "Drive number " << drive_arg
		  << " has a non-digit in it ("
		  << s.substr(end) << ") which we will ignore\n";
      }
    if (d <= std::numeric_limits<unsigned>::max())
      {
	*num = static_cast<unsigned>(d);
	return true;
      }
    else
      {
	std::cerr << "Drive number " << drive_arg
		  << " is too large.\n";
	return false;
      }
  }

  bool StorageConfiguration::select_drive_by_afsp(const std::string& afsp, AbstractDrive **pp,
						  drive_number current) const
  {
    drive_number drive_wanted;
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
    auto show = [this, &os](drive_number d)
		{
		  auto it = drives_.find(d);
		  os << "Drive " << d << ": "
		     << (it != drives_.end() ? "occupied" : "empty");
		  if (it != drives_.end())
		    {
		      assert(is_drive_connected(d));
		      const AbstractDrive* image = it->second.get();
		      os << ", " << image->description();
		    }
		  os << "\n";
		};

    // loop_limit is the last drive we visit, which is different to
    // the usual convention.  We do it this way as there is no
    // guarantee that drives_.rbegin()->first can be incremented
    // without a wrap.
    drive_number loop_limit = 3;
    if (!drives_.empty())
      {
	loop_limit = std::max(drives_.rbegin()->first, loop_limit);
      }
    drive_number i = 0;
    do
      {
	show(i);
	// We don't use a for loop here because loop_limit+1 may be 0
	// (since loop_limit is unsigned).
      } while (i++ < loop_limit);
  }

}  // namespace DFS
