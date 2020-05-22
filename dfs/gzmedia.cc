#include "media.h"

#include <string>
#include <vector>

#include <zlib.h>

namespace
{
  class FixedDecompressionError : public std::exception
  {
  public:
    explicit FixedDecompressionError(const char *msg)
      : msg_(strdup(msg))
    {
    }

    ~FixedDecompressionError()
    {
      free(msg_);
    }

    const char *what() const throw()
    {
      return msg_;
    }

  private:
    char *msg_;
  };

  class OutOfMemoryError : public std::exception
  {
  public:
    OutOfMemoryError() {}
    const char *what() const throw()
    {
      return "not enough available memory";
    }
  };
  const OutOfMemoryError out_of_memory;

  class DecompressionError : public std::exception
  {
  public:
    explicit DecompressionError(gzFile f)
      : msg_(get_err(f))
    {
    }

    static std::string get_err(gzFile f)
    {
      int dummy;
      return std::string(gzerror(f, &dummy));
    }


    const char *what() const throw()
    {
      return msg_.c_str();
    }

  private:
    std::string msg_;
  };

  class CompressedImageFile : public DFS::AbstractDrive
  {
  public:
    explicit CompressedImageFile(const std::string& name)
    {
      errno = 0;
      gzFile f = gzopen(name.c_str(), "r");
      if (f == 0)
	throw DFS::OsError(errno);
      static char buf[DFS::SECTOR_BYTES];
      for (;;)
	{
	  int count = gzread(f, buf, DFS::SECTOR_BYTES);
	  if (count < 0)
	    {
	      throw DecompressionError(f);
	    }
	  else if (count)
	    {
	      const bool short_read = static_cast<unsigned>(count) < DFS::SECTOR_BYTES;
	      if (short_read)
		{
		  std::fill(buf+count, buf+DFS::SECTOR_BYTES, '\0');
		  std::copy(buf, buf+DFS::SECTOR_BYTES, std::back_inserter(data_));
		  break;	// EOF
		}
	      else
		{
		  std::copy(buf, buf+DFS::SECTOR_BYTES, std::back_inserter(data_));
		}
	    }
	  else
	    {
	      break;		// EOF
	    }
	}
      const int err = gzclose_r(f);
      if (err == Z_OK)
	return;
      if (err == Z_STREAM_ERROR)
	throw FixedDecompressionError("invalid compressed data stream");
      if (err == Z_ERRNO)
	throw DFS::OsError(errno);
      if (err == Z_MEM_ERROR)
	throw out_of_memory;	// avoids allocation
      if (err == Z_BUF_ERROR)
	throw FixedDecompressionError("compressed input is incomplete");
      throw FixedDecompressionError("unknown decompression error");
    }

    virtual void read_sector(DFS::sector_count_type sector, DFS::AbstractDrive::SectorBuffer* buf,
			     bool& beyond_eof) override
    {
      if ((data_.size() / DFS::SECTOR_BYTES) < sector)
	{
	  beyond_eof = true;
	  return;
	}
      beyond_eof = false;
      auto start_pos = sector * DFS::SECTOR_BYTES;
      auto end_pos = start_pos + DFS::SECTOR_BYTES;
      std::copy(data_.begin() + start_pos, data_.begin() + end_pos, buf->begin());
    }

    DFS::sector_count_type get_total_sectors() const override
    {
      ldiv_t division = ldiv(data_.size(), DFS::SECTOR_BYTES);
      assert(division.quot < std::numeric_limits<DFS::sector_count_type>::max());
      return DFS::sector_count(division.quot + (division.rem ? 1 : 0));
    }

    std::string description() const override
    {
      return std::string("compressed image file ") + name_;
    }

  private:
    std::string name_;
    std::vector<DFS::byte> data_;
  };
}

namespace DFS
{
  std::unique_ptr<AbstractDrive> compressed_image_file(const std::string& name)
  {
    return std::make_unique<CompressedImageFile>(name);
  }
}
