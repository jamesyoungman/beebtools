#ifndef INC_STORAGE_H
#define INC_STORAGE_H 1

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "dfsimage.h"
#include "dfstypes.h"
#include "media.h"

namespace DFS
{
  enum
    {
     MAX_DRIVES = 4
    };


  class FileSystem;

  class StorageConfiguration
  {
  public:
    StorageConfiguration() {}
    bool connect_drive(std::unique_ptr<AbstractDrive>&&);

    bool is_drive_connected(unsigned drive) const
    {
      if (drive >= drives_.size())
	return false;
      return drives_[drive].get();
    }

    static bool decode_drive_number(const std::string& s, unsigned int *result);
    bool select_drive_by_afsp(const std::string& drive_arg, AbstractDrive **pp, int current) const;
    bool select_drive(unsigned int drive, AbstractDrive **pp) const;

    void show_drive_configuration(std::ostream& os) const
    {
      unsigned drive;
      using std::cout;
      for (drive = 0; drive < drives_.size(); ++drive)
	{
	  os << "Drive " << drive << ": "
	     << (drives_[drive] ? "occupied" : "empty");
	  auto image = drives_[drive].get();
	  if (0 != image)
	    {
	      os << ", " << image->description();
	    }
	  os << "\n";
	}
    }

  private:
    std::array<std::unique_ptr<AbstractDrive>, MAX_DRIVES> drives_;
  };
}

#endif
