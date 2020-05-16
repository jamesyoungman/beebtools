#ifndef INC_MEDIA_H
#define INC_MEDIA_H 1

#include <exception>
#include <memory>
#include <string>

#include "dfstypes.h"


namespace DFS
{
  constexpr unsigned int SECTOR_BYTES = 256;

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
    virtual void read_sector(sector_count_type sector, SectorBuffer *buf) = 0;
    virtual sector_count_type get_total_sectors() const = 0;
    virtual std::string description() const = 0;
  };

  std::unique_ptr<AbstractDrive> make_image_file(const std::string& file_name);

}  // namespace DFS
#endif
