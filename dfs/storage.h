#ifndef INC_STORAGE_H
#define INC_STORAGE_H 1

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "dfsimage.h"
#include "dfstypes.h"

namespace DFS
{
  enum
    {
     MAX_DRIVES = 4
    };

  class StorageConfiguration
  {
  public:
    StorageConfiguration() {}
    bool connect_drive(const std::string& name);

    bool is_drive_connected(unsigned drive) const
    {
      if (drive >= drives_.size())
	return false;
      return drives_[drive].get();
    }

    unsigned int get_num_drives() const
    {
      return drives_.size();
    }

    bool select_drive_by_number(const std::string& drive_arg, const FileSystemImage **pim) const;
    bool select_drive_by_afsp(const std::string& drive_arg, const FileSystemImage **pim, int current) const;
    bool select_drive(unsigned int drive, const FileSystemImage **pim) const;

    void show_drive_configuration(std::ostream& os) const
    {
      unsigned drive;
      using std::cout;
      for (drive = 0; drive < drives_.size(); ++drive)
	{
	  os << "Drive " << drive << ": "
	     << (drives_[drive] ? "occupied" : "empty")
	     << "\n";
	}
    }

  private:
    bool connect_single(const byte* data, const byte* end);
    bool connect_double(const byte* data, const byte* end);
    std::array<std::unique_ptr<FileSystemImage>, MAX_DRIVES> drives_;
  };
}

#endif
