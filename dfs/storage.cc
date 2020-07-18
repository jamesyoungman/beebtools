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
#include "storage.h"

#include <ext/alloc_traits.h>  // for __alloc_traits<>::value_type
#include <string.h>            // for size_t
#include <algorithm>           // for copy, max
#include <array>               // for array
#include <fstream>             // for operator<<, ostringstream, basic_ostream
#include <functional>          // for function
#include <iomanip>             // for operator<<, setw
#include <iterator>            // for reverse_iterator
#include <optional>            // for optional, nullopt
#include <string>              // for string, operator<<, char_traits, basic...
#include <vector>              // for vector, vector<>::size_type
#include "dfs_filesystem.h"    // for FileSystem
#include "dfstypes.h"          // for sector_count_type, byte
#include "driveselector.h"     // for drive_number, operator<<, SurfaceSelector

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

    bool get(unsigned long sec, DFS::SectorBuffer *buf) const
    {
      if (!has(sec))
	return false;
      *buf = *(cache_[sec].get());
      return true;
    }

    void put(unsigned long sec, const DFS::SectorBuffer *buf)
    {
      if (sec >= cache_.size())
	return;
      cache_[sec] = std::make_unique<DFS::SectorBuffer>();
      std::copy(buf->begin(), buf->end(), cache_[sec]->begin());
    }

  private:
    std::vector<std::unique_ptr<DFS::SectorBuffer>> cache_;
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
      std::optional<DFS::SectorBuffer> b = underlying_->read_block(sector);
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

  VolumeMountResult::VolumeMountResult(std::unique_ptr<DFS::FileSystem> fs, DFS::Volume* vol)
    : fs_(std::move(fs)), vol_(vol)
  {
    assert(fs_.get() != 0);
    assert(vol != 0);
  }

  FileSystem* VolumeMountResult::file_system() const
  {
    return fs_.get();
  }


  Volume* VolumeMountResult::volume() const
  {
    return vol_;
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

  void StorageConfiguration::connect_internal(const DFS::SurfaceSelector& n, const std::optional<DriveConfig>& cfg)
  {
    const sector_count_type cached_sectors = 4;
    assert(!is_drive_connected(n));
    drives_.emplace(n, cfg);
    if (cfg)
      {
	caches_[n] = std::make_unique<CachedDevice>(cfg->drive(), cached_sectors);
      }
    else
      {
	caches_[n] = 0;
      }
  }

  bool StorageConfiguration::connect_drives(const std::vector<std::optional<DriveConfig>>& drives,
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
    if (!it->second)
      return std::nullopt;	// unformatted
    return it->second->format();
  }

  bool StorageConfiguration::select_drive(const DFS::SurfaceSelector& drive, AbstractDrive **pp,
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
    if (!it->second)
      {
	std::ostringstream ss;
	ss << "the disc in drive " << drive << " is unformatted\n";
	error = ss.str();
	return false;
      }
    *pp = it->second.get();
    return true;
  }

  bool StorageConfiguration::decode_drive_number(const std::string& drive_arg,
						 DFS::VolumeSelector* vol,
						 std::string& error)
  {
    error.clear();
    size_t end;
    auto got = DFS::VolumeSelector::parse(drive_arg, &end, error);
    if (!got)
      return false;
    if (end < drive_arg.size())
      {
	std::ostringstream ss;
	ss << "invalid volume " << drive_arg;
	error = ss.str();
	return false;
      }
    *vol = *got;
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
		      if (it->second)
			{
			  os << ", " << it->second->drive()->geometry().description();
			  // TODO: consider printing the format description too, here.
			  os << ", " << it->second->drive()->description();
			}
		      else
			{
			  os << ", unformatted";
			}
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


  std::unique_ptr<DFS::FileSystem> StorageConfiguration::mount_fs(const DFS::SurfaceSelector& drive, std::string& error) const
  {
    AbstractDrive *p;
    if (!select_drive(drive, &p, error))
      return 0;
    std::optional<Format> fmt = drive_format(drive, error);
    if (!fmt)
      return 0;
    return std::make_unique<FileSystem>(*p, *fmt, p->geometry());
  }

  std::optional<VolumeMountResult> StorageConfiguration::mount(const DFS::VolumeSelector& vol,
							       std::string& error) const
  {
    std::unique_ptr<DFS::FileSystem> fs = mount_fs(vol.surface(), error);
    if (!fs)
      return std::nullopt;

    Volume *pvol = fs->mount(vol.subvolume(), error); // we retain ownership
    if (!pvol)
      return std::nullopt;
    return VolumeMountResult(std::move(fs), pvol);
  }

}  // namespace DFS
