#ifndef INC_STORAGE_H
#define INC_STORAGE_H 1

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "abstractio.h"
#include "dfstypes.h"
#include "geometry.h"

namespace DFS
{
  class FileSystem;

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
    using SectorBuffer = DFS::SectorBuffer; // TODO: get rid of this.
    virtual sector_count_type get_total_sectors() const = 0;
    virtual std::string description() const = 0;
  };

  class StorageConfiguration
  {
  public:
    StorageConfiguration();
    bool connect_drives(const std::vector<DFS::AbstractDrive*>& sides,
			DriveAllocation how);

    bool is_drive_connected(drive_number drive) const
    {
      auto it = drives_.find(drive);
      if (it == drives_.end())
	return false;
      assert(it->second != 0);
      return true;
    }

    // CARE: decode_drive_number returns false when there is an error, but it can
    // also return true and leave a warning message in |error|.
    static bool decode_drive_number(const std::string& s, drive_number *result,
				    std::string& error);
    std::vector<drive_number> get_all_occupied_drive_numbers() const;
    void show_drive_configuration(std::ostream& os) const;
    void connect_internal(drive_number d, AbstractDrive* p);
    bool select_drive(drive_number drive, AbstractDrive **pp, std::string& error) const;
    std::unique_ptr<DFS::FileSystem> mount(drive_number drive, std::string& error) const;

  private:
    std::map<drive_number, AbstractDrive*> drives_;
    std::map<drive_number, std::unique_ptr<AbstractDrive>> caches_;
  };
}

#endif
