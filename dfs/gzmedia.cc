#include "media.h"

#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#include <zlib.h>

#include "cleanup.h"

namespace
{
  using std::numeric_limits;

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


  void check_zlib_error_code(int zerr)
  {
    switch (zerr)
      {
      case Z_OK:
	return;
      case Z_STREAM_END:
	throw FixedDecompressionError("reached end of comressed data");
      case Z_NEED_DICT:
	throw FixedDecompressionError("a pre-set zlib dictionary is required "
				      "to decompress this data");
      case Z_ERRNO:
	throw DFS::OsError(errno);
      case Z_STREAM_ERROR:
	throw FixedDecompressionError("invalid compressed data stream");
      case Z_DATA_ERROR:
	throw FixedDecompressionError("input data was corrupted, "
				      "are you sure it was created with gzip?");
      case Z_MEM_ERROR:
	throw out_of_memory;	// avoids allocation
      case Z_BUF_ERROR:
	throw FixedDecompressionError("compressed input is incomplete");
      case Z_VERSION_ERROR:
	throw FixedDecompressionError("incompatible version of zlib");
      default:
	throw FixedDecompressionError("unknown decompression error");
      }
  }

  std::vector<DFS::byte> decompress_image_file(const std::string& name)
  {
    std::vector<DFS::byte> compressed;
    errno = 0;
    FILE *f = fopen(name.c_str(), "rb");
    if (0 == f)
      {
	throw DFS::OsError(errno);
      }
    cleanup closer([&f]()
		   {
		     errno = 0;
		     if (EOF == fclose(f))
		       {
			 throw DFS::OsError(errno);
		       }
		   });

    std::vector<DFS::byte> result;
    z_stream stream;
    gz_header header;
    // We're decompressing a foo.gz file, so permit only gzip-compressed streams
    // by passing 16+MAX_WBITS as the second arg to inflateInit2.
    memset(&stream, 0, sizeof(stream));
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;

    int zerr = inflateInit2(&stream, 16+MAX_WBITS);
    check_zlib_error_code(zerr);

    cleanup de_init([&stream]()
		    {
		      check_zlib_error_code(inflateEnd(&stream));
		    });


    // Signal that we want zlib to check for and extract the gzip
    // header.
    header.done = 0;
    check_zlib_error_code(inflateGetHeader(&stream, &header));

    // zlib's inflate() requires to be able to write to a contiguous
    // buffer, so we have to provide one.
    const int input_buf_size = 409600;
    const int output_buf_size = 1024;
    unsigned char input_buffer[input_buf_size];
    unsigned char output_buffer[output_buf_size];
    // These static assertions ensure that the static casts within the
    // loop are safe.
    typedef decltype(stream.avail_in) avail_in_type;
    static_assert(input_buf_size <= std::numeric_limits<decltype(stream.avail_in)>::max());
    static_assert(output_buf_size <= std::numeric_limits<decltype(stream.avail_out)>::max());

    zerr = Z_OK;
    while (zerr != Z_STREAM_END)
      {
	errno = 0;
	stream.avail_in = static_cast<avail_in_type>(fread(input_buffer, 1, input_buf_size, f));
	if (ferror(f))
	  {
	    throw DFS::OsError(errno);
	  }
	if (stream.avail_in == 0)
	  break;
	stream.next_in = input_buffer;
	do  // decompress some data from the input buffer.
	  {
	    stream.next_out = output_buffer;
	    stream.avail_out = output_buf_size;
	    zerr = inflate(&stream, Z_NO_FLUSH);
	    if (zerr != Z_STREAM_END)
	      check_zlib_error_code(zerr);
	    auto end = output_buffer + (output_buf_size - stream.avail_out);
	    std::copy(output_buffer, end, std::back_inserter(result));
	  }
	while (stream.avail_out == 0);
      }
    return result;
  }

  class CompressedImageFile : public DFS::AbstractDrive
  {
  public:
    explicit CompressedImageFile(const std::string& name)
    {
      std::vector<DFS::byte> data = decompress_image_file(name);
      std::swap(data, data_);
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
