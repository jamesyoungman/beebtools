#include "storage.h"

#include <string.h>		// strerror

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "dfsimage.h"
#include "dfstypes.h"

using std::vector;

using DFS::byte;

namespace 
{
  bool dsd_unsupported()
  {
    std::cerr << "Double-sided image files are not supported, try splitting the file first\n";
    return false;
  }
  
  class OsError : public std::exception
  {
  public:
    explicit OsError(int errno_value)
      : errno_value_(errno_value)
    {
    }
    const char *what() const throw()
    {
      return strerror(errno_value_);
    }
  private:
    int errno_value_;
  };

  bool get_file_size(std::ifstream& infile, unsigned int* size_in_bytes)
  {
    if (!infile.seekg(0, infile.end))
      return false;
    *size_in_bytes = infile.tellg();
    if (!infile.seekg(0, infile.beg))
      return false;
    return true;
  }
  
  void load_image(const char *filename, vector<byte>* image)
  {
    std::ifstream infile(filename, std::ifstream::in);
    unsigned int len;
    if (!get_file_size(infile, &len))
      throw OsError(errno);	// TODO: include file name
    image->resize(len);
    if (!infile.read(reinterpret_cast<char*>(image->data()), len).good())
      throw OsError(errno);
  }
  
  void connect_internal(std::unique_ptr<DFS::FileSystemImage>* img,
			const byte* begin, const byte* end)
  {
    vector<byte> data(begin, end);
    *img = std::make_unique<DFS::FileSystemImage>(data);
  }
  
  bool full()
  {
    std::cerr << "There is not enough room to attach more drives.\n";
    return false;
  };

  inline bool ends_with(const std::string & s, const std::string& suffix)
  {
    if (suffix.size() > s.size())
      return false;
    return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
  }
  
}


namespace DFS
{
  bool StorageConfiguration::connect_single(const byte* data, const byte* end)
  {
    // Attach at the next free spot.
    for (unsigned i = 0; i < drives_.size(); ++i)
      {
	if (!drives_[i])
	  {
	    connect_internal(&drives_[i], data, end);
	    return true;
	  }
      }
    return full();
  }
  
  bool StorageConfiguration::connect_drive(const std::string& name) 
  {
    std::vector<byte> data;
    load_image(name.c_str(), &data);
    if (data.size() < DFS::SECTOR_BYTES * 2)
      throw BadFileSystemImage("disk image is too short to contain a valid catalog");

    if (ends_with(name, ".ssd"))
      {
	return connect_single(data.data(), data.data() + data.size());
      }
    if (ends_with(name, ".dsd"))
      {
	return dsd_unsupported();
      }

    std::cerr << "Guessing whether " << name << " is a single- or a double-sided image based on the extension\n";
    switch (data.size()) 
      {
	// DFS 80 tracks * 10 sectors * 2 sides * 256 bytes/sector
      case 400 * 1024:
	return dsd_unsupported();
	
	// DFS 40 tracks * 10 sectors * 2 sides * 256 bytes/sector
      case 200 * 1024:
	return connect_single(data.data(), data.data() + data.size());

      default:
	std::cerr << "Don't know how to handle an image file " << data.size()
		  << " bytes long\n";
	return false;
      }
  }

  bool StorageConfiguration::select_drive(unsigned int drive, const FileSystemImage **pim) const
  {
    const FileSystemImage* image;
    if (drive >= drives_.size())
      {
	std::cerr << "There is no drive " << drive << "\n";
	return false;
      }
    image = drives_[drive].get();
    if (0 == image)
      {
	std::cerr << "There is no disc in drive " << drive << "\n";
	return false;
      }
    *pim = image;
    return true;
  }

  bool StorageConfiguration::select_drive_by_number(const std::string& drive_arg, const FileSystemImage** pim) const
  {
    if (drive_arg.size() == 1 && isdigit(drive_arg[0])) 
      {
	unsigned drive_wanted = drive_arg[0] - '0';
	return select_drive(drive_wanted, pim);
      }
    else
      {
	std::cerr << "Please specify a drive number\n";
	return false;
      }
  }
  
  bool StorageConfiguration::select_drive_by_afsp(const std::string& afsp, const FileSystemImage **pim, int current) const
  {
    unsigned drive_wanted;
    if (afsp.size() > 1 && afsp[0] == ':' && isdigit(afsp[1])) 
      {
	drive_wanted = afsp[1] - '0';
      }
    else
      {
	drive_wanted = current;
      }
    return select_drive(drive_wanted, pim);
  }
  
}  // namespace DFS


