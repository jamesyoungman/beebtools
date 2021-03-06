//
//   Copyright 2020 James Youngman
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
#include "media.h"           // for make_decompressed_file

#include <exception>         // for exception
#include <errno.h>           // for errno
#include <stdio.h>           // for fread, FILE, fclose, ferror, fopen, fseek
#include <stdlib.h>          // for free
#include <string.h>          // for memset, strdup
#include <zconf.h>           // for MAX_WBITS
#include <zlib.h>            // for z_stream, Z_NULL, Z_STREAM_END, gz_header
#include <functional>        // for function
#include <limits>            // for numeric_limits
#include <memory>            // for make_unique, unique_ptr
#include <string>            // for string, allocator, operator+
#include <vector>            // for vector

#include "abstractio.h"      // for FileAccess, SectorBuffer
#include "cleanup.h"         // for cleanup
#include "dfstypes.h"        // for byte
#include "exceptions.h"      // for FileIOError, NonFileOsError

namespace
{
  using std::numeric_limits;
  using DFS::DataAccess;
  using DFS::SectorBuffer;
  using DFS::NonFileOsError;
  using DFS::FileIOError;

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

  void write_decompressed_data(const std::string& name, FILE* fout)
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

    gz_header header;
    memset(&header, 0, sizeof(header));
    z_stream stream;
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
	auto got = stream.avail_in = static_cast<avail_in_type>(fread(input_buffer, 1, input_buf_size, f));
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
	    const size_t bytes_to_write = output_buf_size - stream.avail_out;
	    errno = 0;
	    const size_t bytes_written = fwrite(output_buffer, 1, bytes_to_write, fout);
	    if (bytes_written != bytes_to_write)
	      throw DFS::FileIOError(name, errno);
	    if (zerr == Z_BUF_ERROR && got)
	      {
		// Want more input data.
		break;
	      }
	    if (zerr != Z_STREAM_END)
	      check_zlib_error_code(zerr);
	  }
	while (stream.avail_out == 0);
      }
  }

  FILE* open_temporary_file()
  {
    errno = 0;
    return tmpfile();
  }


  class DecompressedFile : public DFS::FileAccess
  {
  public:
    explicit DecompressedFile(const std::string& name);
    virtual ~DecompressedFile();
    std::vector<DFS::byte> read(unsigned long pos, unsigned long len) override;

  private:
    FILE *f_;
    std::string name_;
  };

  DecompressedFile::DecompressedFile(const std::string& name)
    : f_(open_temporary_file()),
      name_(std::string("decompressed version of " + name))
  {
    if (0 == f_)
      throw new NonFileOsError(errno);
    write_decompressed_data(name, f_);
  }

  DecompressedFile::~DecompressedFile()
  {
  }

  std::vector<DFS::byte> DecompressedFile::read(unsigned long pos, unsigned long len)
  {
    auto fail = [this]() -> std::vector<DFS::byte>
      {
       if (errno)
	 throw FileIOError(name_, errno);
       else
	 return std::vector<DFS::byte>();
      };
    errno = 0;
    if (0 != fseek(f_, pos, SEEK_SET))
      return fail();
    std::vector<DFS::byte> buf;
    buf.resize(len);
    const size_t bytes_read = fread(buf.data(), 1, buf.size(), f_);
    if (bytes_read == 0)
      return fail();
    buf.resize(bytes_read);
    return buf;
  }

}  // namespace


namespace DFS
{
  std::unique_ptr<FileAccess> make_decompressed_file(const std::string& name)
  {
    return std::make_unique<DecompressedFile>(name);
  }
}  // namespace DFS
