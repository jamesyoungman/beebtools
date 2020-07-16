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
#include "img_fileio.h"

#include "exceptions.h"

namespace DFS
{
  DataAccess::~DataAccess()
  {
  }

  FileAccess::~FileAccess()
  {
  }

  namespace internal
  {
    class NoIo : public DFS::DataAccess
    {
    public:
      NoIo()
      {
      }

      ~NoIo()
      {
      }

      std::optional<DFS::SectorBuffer> read_block(unsigned long) override
      {
	return std::nullopt;
      }
    };

    NoIo no_io;

    OsFile::OsFile(const std::string& name)
      : file_name_(name), f_(name, std::ifstream::binary)
    {
      if (f_.fail())
	{
	  // failbit is set if f_ could not be opened.
	  throw DFS::FileIOError(name, errno);
	}
      f_.exceptions(std::ifstream::badbit);
    }

    std::vector<byte> OsFile::read(unsigned long pos, unsigned long len)
    {
      if (!f_)
	{
	  std::cerr << "OsFile::read: BUG? called on failed/bad file "
		    << file_name_ << "\n";
	}
      errno = 0;
      std::vector<byte> buf;
      if (!f_.seekg(pos, f_.beg))
	{
	  int saved_errno = errno;
	  std::cerr << "OsFile::read: failed to seek to position "
		    << pos << " in file " << file_name_ << "; errno="
		    << saved_errno << "\n";
	  f_.clear();
	  if (saved_errno)
	    throw DFS::FileIOError(file_name_, errno);
	  else
	    return buf;
	}
      buf.resize(len);
      f_.read(reinterpret_cast<char*>(buf.data()), len);
      buf.resize(f_.gcount());
      if (!f_.good())
	{
	  const int saved_errno = errno;
	  // POSIX permits a seek beyond end-of-file (at least for
	  // read/write files), so if pos was larger than the file
	  // size, we may come to here rather than fail the seekg()
	  // call.  However, it's also not a failure to read beyond
	  // EOF, so reading from beyond EOF just returns zero bytes.
	  f_.clear();
	  if (errno)
	    throw DFS::FileIOError(file_name_, saved_errno); // a real error
	  else
	    return buf;		// short read
	}
      return buf;
    }

    FileView::FileView(DataAccess& media,
		       const std::string& file_name,
		       // The geometry parameter describes this device, not all
		       // the devices in the file.  For example if an image
		       // contains two sides each having a seprate file system,
		       // the geometry for each of them describes one side.
		       const std::string& description,
		       const DFS::Geometry& geometry,
		       unsigned long initial_skip,
		       sector_count_type take,
		       sector_count_type leave,
		       sector_count_type total)
      : media_(media), file_name_(file_name), description_(description),
	geometry_(geometry),
	initial_skip_(initial_skip), take_(take), leave_(leave), total_(total)
    {
      // If take_ is 0, this signals that the device is unformatted.
      // Hence that is valid.
    }

    FileView::~FileView()
    {
    }

    DFS::Geometry FileView::geometry() const
    {
      return geometry_;
    }

    std::string FileView::description() const
    {
      return description_;
    }

    std::optional<DFS::SectorBuffer> FileView::read_block(unsigned long sector)
    {
      if (0 == take_)
	{
	  // Device is unformatted.
	  return std::nullopt;
	}

      if (sector >= total_)
	{
	  return std::nullopt;
	}

      // Device view:
      //
      //+------------------------+
      //|  take_ | take_ | take_ |
      //|  0     | 1     | 2     |
      //+------------------------+
      //|        |  x    |       |
      //+------------------------+
      //
      // We want to read sector number x, in the represented device.
      // Although the client isn't aware of this, |x| is in the second
      // "group" of sectors in the underlying device, labeled take_ 1.
      //
      // In the underlyng file these sectors are laid out like this:
      //
      // +-------------------------------------------------------------------+
      // |initial_skip_   | take_ | leave_ | take_ | leave_ | take_ | leave_ |
      // |                | 0     | 0      | 1     | 1      | 2     | 2      |
      // +-------------------------------------------------------------------+
      // |                |       |        |  p    |        |       |        |
      // +-------------------------------------------------------------------+
      //
      // |p| is the position of the sector that we want to read (whose
      // offset in the emulated device is |x|). The distance between
      // the start-of-file (the far-left edge of the box) and the
      // sector we want is
      //
      // initial_skip_ + (x / take_) * (take_ + leave_) + x % (take_ + leave_)
      //
      // initial_skip_ is the size of the initial part of the file we
      // need to skip to read sector 0 of the emulated device.  At
      // that offset we can read |take_| emulated secors, but then
      // would need to skip |leave_| emulated sectors before we can
      // read another.  So the three terms in our expression above are
      // the initial skip, the number of "strides" we have to take
      // over take_/leave_ sections to reach the take section our
      // sector is in, and then the position within that section where
      // x lives.
      //
      // For initial_skip_ = 0 and leave_ = 0, this turns into an
      // identity mapping.
      //
      // The units here are sectors, of course.
      const unsigned long pos =
	initial_skip_ +
	safe_unsigned_multiply(sector / take_, static_cast<unsigned long>(take_) + leave_) +
	sector % take_;
      return media_.read_block(pos);
    }


    bool FileView::is_formatted() const
    {
      return take_ != 0;
    }

    FileView FileView::unformatted_device(const std::string& file_name,
					  const std::string& description,
					  const DFS::Geometry& geometry)
    {
      // setting take=0 signals that I/O to the device is impossible.
      return FileView(no_io, file_name, description, geometry, 0, 0, 1, 1);
    }


  }  // namespace internal
}  // namespace DFS
