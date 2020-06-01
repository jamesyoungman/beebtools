#ifndef INC_STORAGE_H
#define INC_STORAGE_H 1

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "dfstypes.h"

namespace DFS
{
  class FileSystem;
  class AbstractDrive;

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

  constexpr unsigned int SECTOR_BYTES = 256;

  class AbstractDrive
  {
  public:
    using SectorBuffer = std::array<byte, DFS::SECTOR_BYTES>;
    // read a sector.  Allow reads beyond EOF when zero_beyond_eof
    // is true.  Image files can be shorter than the disc image.
    virtual void read_sector(sector_count_type sector, SectorBuffer *buf,
			     bool& beyond_eof) = 0;
    virtual sector_count_type get_total_sectors() const = 0;
    virtual std::string description() const = 0;
  };

  class StorageConfiguration
  {
  public:
    StorageConfiguration();
    bool connect_drive(DFS::AbstractDrive*, DriveAllocation how);

    bool is_drive_connected(drive_number drive) const
    {
      auto it = drives_.find(drive);
      if (it == drives_.end())
	return false;
      assert(it->second != 0);
      return true;
    }

    static bool decode_drive_number(const std::string& s, drive_number *result);
    bool select_drive_by_afsp(const std::string& drive_arg, AbstractDrive **pp, drive_number current) const;
    bool select_drive(drive_number drive, AbstractDrive **pp) const;
    void show_drive_configuration(std::ostream& os) const;

  private:
    std::map<drive_number, AbstractDrive*> drives_;
  };
}

#endif
