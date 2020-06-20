#include "storage.h"

#include <string.h>		// strerror

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "dfs_filesystem.h"
#include "dfstypes.h"
#include "driveselector.h"

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

    bool has(unsigned long sec) const
    {
      return sec < cache_.size() && cache_[sec].get() != 0;
    }

    bool get(unsigned long sec, DFS::AbstractDrive::SectorBuffer *buf) const
    {
      if (!has(sec))
	return false;
      *buf = *(cache_[sec].get());
      return true;
    }

    void put(unsigned long sec, const DFS::AbstractDrive::SectorBuffer *buf)
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

    virtual std::optional<DFS::SectorBuffer> read_block(unsigned long sector) override
    {
      DFS::SectorBuffer buf;
      if (cache_.get(sector, &buf))
	{
	  return buf;
	}
      std::optional<SectorBuffer> b = underlying_->read_block(sector);
      if (b)
	{
	  cache_.put(sector, &*b);
	}
      return b;
    }

    std::string description() const override
    {
      return underlying_->description();
    }

    DFS::Geometry geometry() const override
    {
      return underlying_->geometry();
    }

  private:
    DFS::AbstractDrive* underlying_;
    mutable SectorCache cache_;
  };

}  // namespace


namespace DFS
{
  DriveConfig::DriveConfig(DFS::Format fmt, AbstractDrive* p)
    : fmt_(fmt), drive_(p)
  {
  }

  Format DriveConfig::format() const
  {
    return fmt_;
  }

  AbstractDrive* DriveConfig::drive() const
  {
    return drive_;
  }

  DFS::AbstractDrive::~AbstractDrive()
  {
  }

  StorageConfiguration::StorageConfiguration()
  {
  }

  bool check_sequence_fits(DFS::drive_number i,
			   const std::vector<DriveConfig>::size_type to_do,
			   std::function<bool(DFS::drive_number)> occupied)
  {
    typedef const std::vector<DriveConfig> vec;
    if (occupied(i))
      return false;		// Don't use occpied slots.
    if (occupied(i.opposite_surface()))
      return false;		// slot for opposite surface already used

    const auto limit = std::numeric_limits<DFS::drive_number>::max().prev();
    vec::size_type done = 0;
    while (i < limit && done < to_do)
      {
	if (occupied(i))
	  return false;
	++done;
	i = DFS::drive_number::corresponding_side_of_next_device(i);
      }
    return done == to_do;
  }

  void StorageConfiguration::connect_internal(drive_number n, const DriveConfig& cfg)
  {
    const sector_count_type cached_sectors = 4;
    assert(!is_drive_connected(n));
    drives_.emplace(n, cfg);
    caches_[n] = std::make_unique<CachedDevice>(cfg.drive(), cached_sectors);
  }

  bool StorageConfiguration::connect_drives(const std::vector<DriveConfig>& drives,
					    DriveAllocation how)
  {
    const auto limit = std::numeric_limits<drive_number>::max();
    if (how == DriveAllocation::PHYSICAL)
      {
	auto occ = [this](DFS::drive_number i) -> bool
		    {
		      return is_drive_connected(i);
		    };
	for (DFS::drive_number n = DFS::drive_number(0);
	     n < limit;
	     n = n.next())
	  {
	    if (check_sequence_fits(n, drives.size(), occ))
	      {
		for (auto d : drives)
		  {
		    connect_internal(n, d);
		    n = n.next().next();
		  }
		return true;
	      }
	  }
	return false;
      }
    else
      {
	DFS::drive_number n(0);
	for (auto d : drives)
	  {
	    for (; n < limit; n = n.next())
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

  std::optional<Format> StorageConfiguration::drive_format(drive_number drive,
							   std::string& error) const
  {
    auto it = drives_.find(drive);
    if (it == drives_.end())
      {
	std::ostringstream ss;
	ss << "there is no disc in drive " << drive << "\n";
	error = ss.str();
	return std::nullopt;
      }
    return it->second.format();
  }

  bool StorageConfiguration::select_drive(drive_number drive, AbstractDrive **pp,
					  std::string& error) const
  {
    auto it = caches_.find(drive);
    if (it == caches_.end())
      {
	std::ostringstream ss;
	ss << "there is no disc in drive " << drive << "\n";
	error = ss.str();
	return false;
      }
    assert(it->second);
    *pp = it->second.get();
    return true;
  }

  bool StorageConfiguration::decode_drive_number(const std::string& drive_arg, drive_number* num,
						 std::string& error)
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
    if (s.empty())
      {
	error = "empty drive numbers are not valid";
	return false;
      }
    size_t end;
    auto got = DFS::SurfaceSelector::parse(s, &end, error);
    if (!got)
      return false;
    *num = *got;
    return true;
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
    const auto max_drive = drives_.empty() ? DFS::SurfaceSelector(0) : drives_.rbegin()->first;
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

		      os << ", " << it->second.drive()->geometry().description();
		      // TODO: consider printing the format description too, here.
		      os << ", " << it->second.drive()->description();
		    }
		  os << "\n";
		};

    drive_number loop_limit = DFS::SurfaceSelector::acorn_default_last_surface();
    if (!drives_.empty())
      {
	loop_limit = std::max(drives_.rbegin()->first, loop_limit);
      }
    drive_number i(0);
    do
      {
	show(i);
	// We don't use a for loop here because loop_limit+1 may be 0
	// (since loop_limit is unsigned).
      } while (i.postincrement() < loop_limit);
  }


  std::unique_ptr<DFS::FileSystem> StorageConfiguration::mount(drive_number drive, std::string& error) const
  {
    AbstractDrive *p;
    if (!select_drive(drive, &p, error))
      return 0;
    std::optional<Format> fmt = drive_format(drive, error);
    if (!fmt)
      return 0;
    return std::make_unique<FileSystem>(*p, *fmt, p->geometry());
  }

}  // namespace DFS
