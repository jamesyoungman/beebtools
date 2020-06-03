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

namespace DFS
{
  StorageConfiguration::StorageConfiguration()
  {
  }

  bool check_sequence_fits(DFS::drive_number i,
			   const DFS::drive_number to_do,
			   std::function<bool(DFS::drive_number)> occupied)
  {
    if (occupied(i))
      return false;		// Don't use occpied slots.
    switch (i % 4)
      {
      case 0:
      case 1:
	if (occupied(i + 2))
	  {
	    // this is "side 0" where "side 1" is occupied.
	    return false;
	  }
	break;
      case 2:
      case 3:
	if (occupied(i - 2))
	  {
	    // this is "side 1" where "side 0" is occupied.
	    return false;
	  }
	break;
      }
    const auto limit = std::numeric_limits<drive_number>::max();
    DFS::drive_number done = 0;
    while (i < limit-1 && done < to_do)
      {
	if (occupied(i))
	  return false;
	++done;
	i += 2;
      }
    return done == to_do;
  }

  bool StorageConfiguration::connect_drives(const std::vector<AbstractDrive*>& drives,
					    DriveAllocation how)
  {
    const auto limit = std::numeric_limits<drive_number>::max();
    if (how == DriveAllocation::PHYSICAL)
      {
	auto occ = [this](DFS::drive_number i) -> bool
		    {
		      return is_drive_connected(i);
		    };
	for (DFS::drive_number n = 0; n < limit; ++n)
	  {
	    if (check_sequence_fits(n, static_cast<DFS::drive_number>(drives.size()), occ))
	      {
		for (auto d : drives)
		  {
		    assert(!is_drive_connected(n));
		    drives_[n] = d;
		    n += 2;
		  }
		return true;
	      }
	  }
	return false;
      }
    else
      {
	DFS::drive_number n = 0;
	for (auto d : drives)
	  {
	    for (; n < limit; ++n)
	      {
		if (!is_drive_connected(n))
		  {
		    drives_[n] = d;
		    break;
		  }
	      }
	  }
	return n < limit;
      }
  }



  bool StorageConfiguration::select_drive(unsigned int drive, AbstractDrive **pp) const
  {
    auto it = drives_.find(drive);
    if (it == drives_.end())
      {
	std::cerr << "There is no disc in drive " << drive << "\n";
	return false;
      }
    assert(it->second != 0);
    *pp = it->second;
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
		      os << ", " << it->second->description();
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
