#include "fileio.h"

#include "exceptions.h"

namespace DFS
{
  DataAccess::~DataAccess()
  {
  }

  namespace internal
  {
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

    std::optional<DFS::SectorBuffer> OsFile::read_block(unsigned long lba)
    {
      if (!f_)
	{
	  std::cerr << "OsFile::read_block: BUG? called on failed/bad file "
		    << file_name_ << "\n";
	}
      errno = 0;
      // TODO: use safe_unsigned_multiply here
      const unsigned long pos = lba * DFS::SECTOR_BYTES;
      if (!f_.seekg(pos, f_.beg))
	{
	  int saved_errno = errno;
	  std::cerr << "OsFile::read_block: failed to seek to position "
		    << pos << " in file " << file_name_ << "; errno="
		    << saved_errno << "\n";
	  f_.clear();
	  if (saved_errno)
	    throw DFS::FileIOError(file_name_, errno);
	  else
	    return std::nullopt;
	}
      DFS::SectorBuffer buf;
      if (!f_.read(reinterpret_cast<char*>(buf.data()), DFS::SECTOR_BYTES).good())
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
	    return std::nullopt;
	}
      return buf;
    }

    NarrowedFileView::NarrowedFileView(DataAccess& underlying,
				       unsigned long offset_sectors,
				       DFS::sector_count_type limit)
      : underlying_(underlying), offset_(offset_sectors), limit_(limit)
    {
    }

    std::optional<DFS::SectorBuffer> NarrowedFileView::read_block(unsigned long lba)
    {
      if (lba >= limit_)
	return std::nullopt;
      return underlying_.read_block(lba + offset_);
    }

    FileView::FileView(DataAccess& media,
		       const std::string& file_name,
		       // The geometry parameter describes this device, not all
		       // the devices in the file.  For example if an image
		       // contains two sides each having a seprate file system,
		       // the geometry for each of them describes one side.
		       const std::string& description,
		       DFS::Format format,
		       const DFS::Geometry& geometry,
		       unsigned long initial_skip,
		       sector_count_type take,
		       sector_count_type leave,
		       sector_count_type total)
      : media_(media), file_name_(file_name), description_(description),
	format_(format), geometry_(geometry),
	initial_skip_(initial_skip), take_(take), leave_(leave), total_(total)
    {
      // If take_ is 0, we could sequentially read an arbitrary amount
      // of data from the underlying file without seeing sector 0 of
      // the device we're presenting.  IOW, we would make no progress.
      assert(take_ > 0);
    }

    FileView::~FileView()
    {
    }

    DFS::Geometry FileView::geometry() const
    {
      return geometry_;
    }

    DFS::Format FileView::format() const
    {
      return format_;
    }

    std::string FileView::description() const
    {
      return description_;
    }

    std::optional<DFS::SectorBuffer> FileView::read_block(unsigned long sector)
    {
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

  }  // namespace internal
}  // namespace DFS
