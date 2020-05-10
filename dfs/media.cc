#include "media.h"

#include <string.h>

#include <fstream>
#include <memory>
#include <string>

#include "dfsimage.h"
#include "dfstypes.h"
#include "storage.h"

namespace 
{
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

  class ImageFile : public DFS::AbstractDrive
  {
  public:
    explicit ImageFile(const std::string& name, std::unique_ptr<std::ifstream>&& ifs)
      : name_(name), f_(std::move(ifs))
    {
      get_file_size(*f_, &file_size_);
    }
    
    virtual void read_sector(DFS::sector_count_type sector, DFS::AbstractDrive::SectorBuffer* buf) override
    {
      errno = 0;
      if (!f_->seekg(sector * DFS::SECTOR_BYTES, f_->beg))
	{
	  throw OsError(errno);
	}
      if (!f_->read(reinterpret_cast<char*>(buf->data()), DFS::SECTOR_BYTES).good())
	throw OsError(errno);
    }
    
    DFS::sector_count_type get_total_sectors() const override
    {
      ldiv_t division = ldiv(file_size_, DFS::SECTOR_BYTES);
      return division.quot + (division.rem ? 1 : 0);
    }
    
    std::string description() const override
    {
      return std::string("image file ") + name_;
    }

    

  private:
    std::string name_;
    std::unique_ptr<std::ifstream> f_;
    unsigned int file_size_;
  };

  DFS::AbstractDrive* dsd_unsupported()
  {
    std::cerr << "Double-sided image files are not supported, try splitting the file first\n";
    return 0;
  }

  inline bool ends_with(const std::string & s, const std::string& suffix)
  {
    if (suffix.size() > s.size())
      return false;
    return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
  }

}  // namespace

namespace DFS
{

  std::unique_ptr<AbstractDrive> make_image_file(const std::string& name)
  {
    std::unique_ptr<std::ifstream> infile(std::make_unique<std::ifstream>(name, std::ifstream::in));
    unsigned int len;
    if (!get_file_size(*infile, &len))
      throw OsError(errno);	// TODO: include file name

    if (len < DFS::SECTOR_BYTES * 2)
      throw DFS::BadFileSystem("disk image is too short to contain a valid catalog");
    std::unique_ptr<ImageFile> failed;
    if (ends_with(name, ".ssd"))
      {
	return std::make_unique<ImageFile>(name, std::move(infile));
      }
    if (ends_with(name, ".dsd"))
      {
	dsd_unsupported();
	return failed;
      }
    
    std::cerr << "Guessing whether " << name << " is a single- or a double-sided image based on the extension\n";
    switch (len)
      {
	// DFS 80 tracks * 10 sectors * 2 sides * 256 bytes/sector
      case 400 * 1024:
	dsd_unsupported();
	return failed;

	// DFS 40 tracks * 10 sectors * 2 sides * 256 bytes/sector
      case 200 * 1024:
	return std::make_unique<ImageFile>(name, std::move(infile));

      default:
	std::cerr << "Don't know how to handle an image file " << len
		  << " bytes long\n";
	return failed;
      }
  }
  
  
}  // namespace DFS

