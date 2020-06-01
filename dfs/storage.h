#ifndef INC_STORAGE_H
#define INC_STORAGE_H 1

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "dfsimage.h"
#include "dfstypes.h"
#include "media.h"

namespace DFS
{
  class FileSystem;

  class StorageConfiguration
  {
  public:
    // DriveAllocation represents a choice of how to assign image
    // files to drive slots.
    //
    // Suppose (from empty) we insert two single-sided image files.
    // The first will be drive 0.  For strategy EVEN, the second will
    // be drive 2, just as if we inserted two single-sided floppy
    // disks into a BBC Micro.  For strategy ANY, the second will be
    // drive 1, as if the two image files represented the two sides
    // of a physical floppy.
    enum class DriveAllocation
      {
       ANY = 1,  // always use the next available slot
       EVEN = 2,  // use an even slot (as if the image were a physical disc).
      };

    StorageConfiguration();
    bool connect_drive(std::unique_ptr<AbstractDrive>&&, DriveAllocation how);

    bool is_drive_connected(drive_number drive) const
    {
      auto it = drives_.find(drive);
      if (it == drives_.end())
	return false;
      return it->second.get();
    }

    static bool decode_drive_number(const std::string& s, drive_number *result);
    bool select_drive_by_afsp(const std::string& drive_arg, AbstractDrive **pp, drive_number current) const;
    bool select_drive(drive_number drive, AbstractDrive **pp) const;
    void show_drive_configuration(std::ostream& os) const;

  private:
    std::map<drive_number, std::unique_ptr<AbstractDrive>> drives_;
  };
}

#endif
