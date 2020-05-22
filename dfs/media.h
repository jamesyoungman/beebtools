#ifndef INC_MEDIA_H
#define INC_MEDIA_H 1

#include <exception>
#include <memory>
#include <string>

#include <string.h>

#include "dfstypes.h"


namespace DFS
{
  constexpr unsigned int SECTOR_BYTES = 256;

  class OsError : public std::exception
  {
  public:
    explicit OsError(int errno_value)
      : errno_value_(errno_value)
    {
      assert(errno_value_ != 0);
    }
    const char *what() const throw()
    {
      return strerror(errno_value_);
    }
  private:
    int errno_value_;
  };

  class MediaReadFailure : public std::exception
  {
  public:
    explicit MediaReadFailure(unsigned sector)
      : msg_(make_message(sector, "read")), sector_(sector)
    {
    }

    const char *what() const throw()
    {
      return msg_.c_str();
    }
  private:
    static std::string make_message(unsigned sector, const char *activity);
    std::string msg_;
    unsigned sector_;
  };

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

  std::unique_ptr<AbstractDrive> make_image_file(const std::string& file_name);
#if USE_ZLIB
  std::unique_ptr<AbstractDrive> compressed_image_file(const std::string& name);
#endif
}  // namespace DFS
#endif
