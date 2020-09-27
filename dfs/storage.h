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
#ifndef INC_STORAGE_H
#define INC_STORAGE_H 1

#include <assert.h>         // for assert
#include <fstream>          // for ostream
#include <map>              // for map, _Rb_tree_const_iterator
#include <memory>           // for unique_ptr
#include <optional>         // for optional
#include <string>           // for string
#include <utility>          // for pair
#include <vector>           // for vector

#include "abstractio.h"     // for DataAccess
#include "dfs_filesystem.h" // for FileSystem
#include "dfs_format.h"     // for Format
#include "dfs_volume.h"     // for Volume
#include "driveselector.h"  // for drive_number
#include "geometry.h"       // for Geometry

namespace DFS
{
  // DriveAllocation represents a choice of how to assign image
  // files to drive slots.
  //
  // Suppose (from empty) we insert two single-sided image files.  The
  // first will be drive 0.  For strategy PHYSICAL, the second will be
  // drive 1, just as if we inserted two single-sided floppy disks
  // into a BBC Micro.  For strategy PHYSICAL, the second will be
  // drive 2, as if the two image files represented the two sides of a
  // physical floppy.
  enum class DriveAllocation
    {
     FIRST = 1,  // always use the next available slot
     PHYSICAL = 2, // behave as if image files were physical discs
    };

  class AbstractDrive : public DataAccess
  {
  public:
    virtual ~AbstractDrive();
    virtual Geometry geometry() const = 0;
    virtual std::string description() const = 0;
  };

  class DriveConfig
  {
  public:
    //DriveConfig(const DriveConfig&) = default;
    std::optional<Format> format() const;
    DriveConfig(std::optional<Format> fmt, AbstractDrive* p);
    ~DriveConfig() {}
    AbstractDrive* drive() const;

    DriveConfig& operator=(const DriveConfig&) = default;
    bool operator==(const DriveConfig&);

  private:
    std::optional<Format> fmt_;
    AbstractDrive* drive_;	// not owned
  };

  class VolumeMountResult
  {
  public:
    VolumeMountResult(std::unique_ptr<DFS::FileSystem>, DFS::Volume*);
    DFS::FileSystem* file_system() const;
    DFS::Volume* volume() const;

  private:
    std::unique_ptr<DFS::FileSystem> fs_;
    DFS::Volume* vol_;
  };

  class StorageConfiguration
  {
  public:
    StorageConfiguration();
    bool connect_drives(const std::vector<std::optional<DriveConfig>>& sides,
			DriveAllocation how);

    bool is_drive_connected(drive_number drive) const
    {
      auto it = drives_.find(drive);
      if (it == drives_.end())
	return false;
      if (!it->second)
	{
	  // connected but unformatted
	  return true;
	}
      assert(it->second->drive() != 0);
      return true;
    }

    static bool decode_drive_number(const std::string& s, DFS::VolumeSelector *result,
				    std::string& error);
    std::vector<drive_number> get_all_occupied_drive_numbers() const;
    void show_drive_configuration(std::ostream& os) const;
    void connect_internal(const DFS::SurfaceSelector& d, const std::optional<DriveConfig>& drive);
    std::optional<Format> drive_format(drive_number drive, std::string& error) const;
    bool select_drive(const DFS::SurfaceSelector&, AbstractDrive **pp, std::string& error) const;
    std::unique_ptr<DFS::FileSystem> mount_fs(const DFS::SurfaceSelector&, std::string& error) const;
    std::optional<VolumeMountResult> mount(const DFS::VolumeSelector& vol, std::string& error) const;

  private:
    std::map<drive_number, std::optional<DriveConfig>> drives_;
    std::map<drive_number, std::unique_ptr<AbstractDrive>> caches_;
  };

  void failed_to_mount_surface(std::ostream&, const SurfaceSelector&, const std::string&);
  void failed_to_mount_volume(std::ostream&, const VolumeSelector&, const std::string&);
}

#endif
