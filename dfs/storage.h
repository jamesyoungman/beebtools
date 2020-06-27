#ifndef INC_STORAGE_H
#define INC_STORAGE_H 1

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "abstractio.h"
#include "dfs_format.h"
#include "dfstypes.h"
#include "driveselector.h"
#include "geometry.h"

namespace DFS
{
  class FileSystem;
  class Volume;

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
    Format format() const;
    DriveConfig(Format fmt, AbstractDrive* p);
    ~DriveConfig() {}
    AbstractDrive* drive() const;

    DriveConfig& operator=(const DriveConfig&) = default;
    bool operator==(const DriveConfig&);

  private:
    Format fmt_;
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
    bool connect_drives(const std::vector<DriveConfig>& sides,
			DriveAllocation how);

    bool is_drive_connected(drive_number drive) const
    {
      auto it = drives_.find(drive);
      if (it == drives_.end())
	return false;
      assert(it->second.drive() != 0);
      return true;
    }

    static bool decode_drive_number(const std::string& s, DFS::VolumeSelector *result,
				    std::string& error);
    std::vector<drive_number> get_all_occupied_drive_numbers() const;
    void show_drive_configuration(std::ostream& os) const;
    void connect_internal(const DFS::SurfaceSelector& d, const DriveConfig& drive);
    std::optional<Format> drive_format(drive_number drive, std::string& error) const;
    bool select_drive(const DFS::SurfaceSelector&, AbstractDrive **pp, std::string& error) const;
    std::unique_ptr<DFS::FileSystem> mount_fs(const DFS::SurfaceSelector&, std::string& error) const;
    std::optional<VolumeMountResult> mount(const DFS::VolumeSelector& vol, std::string& error) const;

  private:
    std::map<drive_number, DriveConfig> drives_;
    std::map<drive_number, std::unique_ptr<AbstractDrive>> caches_;
  };
}

#endif
