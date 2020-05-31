#include "media.h"

#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#include <zlib.h>

#include "cleanup.h"
#include "exceptions.h"

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

    const char *what() const noexcept
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
    const char *what() const noexcept
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


    const char *what() const noexcept
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
	// We should never see this case, because the loop in which
	// we call inflate should terminate when we see Z_STREAM_END
	// without callign check_zlib_error_code().
	throw FixedDecompressionError("reached end of comressed data");
      case Z_NEED_DICT:
	// I don't think this occurs in gzip-formatted inputs,
	// which is all we support.
	throw FixedDecompressionError("a pre-set zlib dictionary is required "
				      "to decompress this data");
      case Z_ERRNO:
	// This is not a file I/O error because zlib isn't performing the
	// file I/O.
	throw DFS::NonFileOsError(errno);
      case Z_STREAM_ERROR:
	// This generally means that the data in the z_stream struct
	// is inconsistent with the data in the incoming stream, or
	// bad parameters were passed to a zlib gunction.  It seems
	// hard to reproduce these problems in a correct program.
	throw FixedDecompressionError("invalid compressed data stream or "
				      "inconsistent program state (i.e. bug)");
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
    errno = 0;
    // We use both a local buffer for zlib and a stdio buffer here.
    // See the comment at input_buf_size for an explanation.  Before
    // moving this declaration, note that this must live longer than
    // the FILE object which references it.
    std::vector<char> readbuf(32768, '\0');
    FILE *f = fopen(name.c_str(), "rb");
    if (0 == f)
      {
	throw DFS::FileIOError(name, errno);
      }
    // NOTE: We rely on C++ destroying |closer| (which closes f)
    // before destroying |readbuf| (which f holds a pointer to).
    // Section 6.6 [stmt.jump] of the ISO C++ 2003 standard specifies
    // that objects with automatic storage duration are destroyed in
    // reverse order of their declaration.
    //
    // NOTE: the capture for readbuf in the lambda used by closer is
    // there so that we get a compiler error if the declaration (and
    // thus destruction) order becomes incorrect.
    cleanup closer([&f, &name, &readbuf]()
		   {
		     (void)readbuf; // see NOTE above
		     errno = 0;
		     if (EOF == fclose(f))
		       {
			 throw DFS::FileIOError(name, errno);
		       }
		   });
    setbuffer(f, readbuf.data(), readbuf.size());

    std::vector<DFS::byte> result;
    z_stream stream;
    gz_header header;
    memset(&stream, 0, sizeof(stream));
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;

    // We're decompressing a foo.gz file, so permit only gzip-compressed streams
    // by passing 16+MAX_WBITS as the second arg to inflateInit2.
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

    // If input_buf_size (and the stdio buffer size) is too small, we
    // perform too many read operations.  But keeping input_buf_size
    // low ensures that we handle all the various cases around buffer
    // filling and emptying.  Therefore to avoid input_buf_size
    // causing I/O read performance problems, we use a large stdio
    // buffer.
    const int input_buf_size = 512;
    // If output_buf_size is too small, we just call inflate() too
    // many times.
    const int output_buf_size = 1024;
    unsigned char input_buffer[input_buf_size];
    // zlib's inflate() requires to be able to write to a contiguous
    // buffer, so we have to provide one.
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
	    throw DFS::FileIOError(name, errno);
	  }
	// We rely on zlib to detect the end of the input stream.  If
	// there is no more input here we will pass avail_in=0 to
	// inflate() which tells it there is no more input.  If that
	// means the input is incomplete, it will return zerr != Z_OK
	// and we will issue a diagnostic.  However, if we were
	// reading a file compressed with compress(1) (e.g. foo.ssd.Z)
	// then we might have to recognise the end of the input stream
	// with physical EOF.   I don't think it's possible to identify
	// when a foo.Z file has been truncated.
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
      : name_(name)
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
