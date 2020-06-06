#include "storage.h"

#include <string.h>		// strerror

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "dfsimage.h"
#include "dfstypes.h"

using std::vector;

using DFS::byte;

namespace
{
  class SectorCache
  {
  public:
    explicit SectorCache(DFS::sector_count_type initial_sectors)
    {
      cache_.resize(initial_sectors);
    }

    bool has(DFS::sector_count_type sec) const
    {
      return sec < cache_.size() && cache_[sec].get() != 0;
    }

    bool get(DFS::sector_count_type sec, DFS::AbstractDrive::SectorBuffer *buf) const
    {
      if (!has(sec))
	return false;
      *buf = *(cache_[sec].get());
      return true;
    }

    void put(DFS::sector_count_type sec, const DFS::AbstractDrive::SectorBuffer *buf)
    {
      if (sec >= cache_.size())
	return;
      cache_[sec] = std::make_unique<DFS::AbstractDrive::SectorBuffer>();
      std::copy(buf->begin(), buf->end(), cache_[sec]->begin());
    }

  private:
    std::vector<std::unique_ptr<DFS::AbstractDrive::SectorBuffer>> cache_;
  };

  class CachedDevice : public DFS::AbstractDrive
  {
  public:
    CachedDevice(AbstractDrive* underlying,
		 DFS::sector_count_type cached_sectors)
      : underlying_(underlying),
	cache_(cached_sectors)
    {
    }

    ~CachedDevice() override
    {
    }

    virtual void read_sector(DFS::sector_count_type sector, DFS::AbstractDrive::SectorBuffer* buf,
			     bool& beyond_eof) override
    {
      if (cache_.get(sector, buf))
	return;
      underlying_->read_sector(sector, buf, beyond_eof);
      cache_.put(sector, buf);
    }

    DFS::sector_count_type get_total_sectors() const override
    {
      return underlying_->get_total_sectors();
    }

    std::string description() const override
    {
      return underlying_->description();
    }

  private:
    DFS::AbstractDrive* underlying_;
    SectorCache cache_;
  };

}  // namespace


namespace DFS
{
  DFS::AbstractDrive::~AbstractDrive()
  {
  }

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

  void StorageConfiguration::connect_internal(drive_number n, AbstractDrive* d)
  {
    const sector_count_type cached_sectors = 4;
    assert(!is_drive_connected(n));
    drives_[n] = d;
    caches_[n] = std::make_unique<CachedDevice>(d, cached_sectors);
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
		    connect_internal(n, d);
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
		    connect_internal(n, d);
		    break;
		  }
	      }
	  }
	return n < limit;
      }
  }

  bool StorageConfiguration::select_drive(unsigned int drive, AbstractDrive **pp) const
  {
    auto it = caches_.find(drive);
    if (it == caches_.end())
      {
	std::cerr << "There is no disc in drive " << drive << "\n";
	return false;
      }
    assert(it->second);
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

  std::vector<drive_number> StorageConfiguration::get_all_occupied_drive_numbers() const
  {
    std::vector<drive_number> result;
    result.reserve(drives_.size());
    for (const auto& number_and_device : drives_)
      result.push_back(number_and_device.first);
    return result;
  }

  void StorageConfiguration::show_drive_configuration(std::ostream& os) const
  {
    const auto max_drive = drives_.empty() ? 0 : drives_.rbegin()->first;
    std::ostringstream ss;
    ss << max_drive;
    const int max_drive_num_len = static_cast<int>(ss.str().size());

    auto show = [this, &os, max_drive_num_len](drive_number d)
		{
		  auto it = drives_.find(d);
		  os << "Drive " << std::setw(max_drive_num_len) << d << ": "
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
