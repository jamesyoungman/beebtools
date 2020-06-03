#include "media.h"

#include <string.h>

#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "dfsimage.h"
#include "dfstypes.h"
#include "storage.h"

namespace
{
  using DFS::sector_count_type;

  template <typename T> T safe_unsigned_multiply(T a, T b)
    {
      // Ensure T is unsigned.
      static_assert(std::numeric_limits<T>::is_integer
		    && !std::numeric_limits<T>::is_signed);
      if (a == 0 || b == 0)
	return T(0);
      if (std::numeric_limits<T>::max() / a < b)
	throw std::range_error("overflow in safe_unsigned_multiply");
      return T(a * b);
    }

  unsigned long int get_file_size(const std::string& name, std::ifstream& infile)
  {
    if (!infile.seekg(0, infile.end))
      throw DFS::FileIOError(name, errno);
    const unsigned long int result = infile.tellg();
    if (!infile.seekg(0, infile.beg))
      throw DFS::FileIOError(name, errno);
    return result;
  }

  class FileView : public DFS::AbstractDrive
  {
  public:
    // See the comment in read_sector for an explanation of the
    // constructor parameters.
    FileView(std::ifstream* f,
	     const std::string& file_name,
	     const std::string& description,
	     sector_count_type initial_skip,
	     sector_count_type take,
	     sector_count_type leave,
	     sector_count_type total)
      : f_(f), file_name_(file_name), description_(description),
      initial_skip_(initial_skip), take_(take), leave_(leave), total_(total)
    {
      // If take_ is 0, we could sequentially read an arbitrary amount
      // of data from the underlying file without seeing sector 0 of
      // the device we're presenting.  IOW, we would make no progress.
      assert(take_ > 0);
    }

    ~FileView() override
      {
      }

    sector_count_type get_total_sectors() const
    {
      return total_;
    }

    std::string description() const
    {
      return description_;
    }

    void read_sector(sector_count_type sector, SectorBuffer *buf,
		     bool& beyond_eof) override
    {
      if (sector >= total_)
	{
	  beyond_eof = true;
	  return;
	}
      beyond_eof = false;

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
      // |initial_leave_  | take_ | leave_ | take_ | leave_ | take_ | leave_ |
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
      // initial_leave_ + (x / take_) * (take_ + leave_) + (x % take_)
      //
      // initial_leave_ is the size of the initial part of the file we
      // need to skip to read sector 0 of the emulated device.  At
      // that offset we can read |take_| emulated secors, but then
      // would need to skip |leave_| emulated sectors before we can
      // read another.  So the three terms in our expression above are
      // the initial skip, the number of "strides" we have to take
      // over take_/leave_ sections to reach the take section our
      // sector is in, and then the position within that section where
      // x lives.
      //
      // For initial_leave_ = 0 and leave_ = 0, this turns into an
      // identity mapping.
      //
      // The units here are sectors, of course.
      const unsigned long pos =
	initial_skip_ +
	safe_unsigned_multiply(DFS::sector_count(sector / take_),
			       DFS::sector_count(take_ + leave_)) +
	(sector % take_));
      errno = 0;
      if (!f_->seekg(pos * DFS::SECTOR_BYTES, f_->beg))
	{
	  throw DFS::FileIOError(file_name_, errno);
	}
      if (!f_->read(reinterpret_cast<char*>(buf->data()), DFS::SECTOR_BYTES).good())
	throw DFS::FileIOError(file_name_, errno);
    }

  private:
    std::ifstream* f_;
    std::string file_name_;
    std::string description_;
    sector_count_type initial_skip_;
    sector_count_type take_;
    sector_count_type leave_;
    sector_count_type total_;
  };

  // A ViewFile is a disc image file which contains the sectors of one
  // or more emulated devices, in order, but with regular gaps.
  // Examples include a DSD file (which contains all the sectors from
  // one side of a cylinder, then all the sectors of the other side of
  // a cylinder) or MMB files (which contains a concatenation of many
  // disc images).  An SSD file is a degenarate example, in the sense
  // that it can be described in the same way but has no gaps.
  class ViewFile : public DFS::AbstractImageFile
  {
  public:
    ViewFile(const std::string& name, std::unique_ptr<std::ifstream>&& ifs)
      : name_(name), f_(std::move(ifs))
      {
      }

    ~ViewFile() override
      {
      }

    bool connect_drives(DFS::StorageConfiguration* storage, DFS::DriveAllocation how) override
    {
      std::vector<DFS::AbstractDrive*> drives;
      for (auto& view : views_)
	drives.push_back(&view);
      return storage->connect_drives(drives, how);
    }

    std::ifstream* get_file() const
      {
	std::ifstream* result = f_.get();
	assert(result != 0);
	return result;
      }

    void add_view(const FileView& v)
    {
      views_.push_back(v);
    }

    unsigned long int file_size() const
    {
      return get_file_size(name_, *get_file());
    }

  private:
    std::string name_;
    std::vector<FileView> views_;
    std::unique_ptr<std::ifstream> f_;
  };

  class SsdFile : public ViewFile
  {
  public:
    explicit SsdFile(const std::string& name, std::unique_ptr<std::ifstream>&& ifs)
      : ViewFile(name, std::move(ifs))
    {
      std::ifstream* f = get_file();
      const DFS::sector_count_type total_sectors = DFS::sector_count(file_size() / DFS::SECTOR_BYTES);
      std::string desc = std::string("SSD file ") + name;
      FileView v(f, name, desc, 0, total_sectors, 0, total_sectors);
      add_view(v);
    }
  };

  class DsdFile : public ViewFile
  {
  public:
    explicit DsdFile(const std::string& name, std::unique_ptr<std::ifstream>&& ifs)
      : ViewFile(name, std::move(ifs))
    {
      std::ifstream* f = get_file();
      const DFS::sector_count_type one_side = DFS::sector_count(file_size() / (2 * DFS::SECTOR_BYTES));
      const sector_count_type track_len = 10;
      FileView side0(f, name, std::string("side 0 of DSD file ") + name,
		     0,		/* side 0 begins immediately */
		     track_len, /* read the whole of the track */
		     track_len, /* ignore track data for side 1 */
		     one_side);
      add_view(side0);
      FileView side1(f, name, std::string("side 1 of DSD file ") + name,
		     track_len, /* side 1 begins after the first track of side 0 */
		     track_len, /* read the whole of the track */
		     track_len, /* ignore track data for side 0 */
		     one_side);
      add_view(side1);
    }
  };

  inline bool ends_with(const std::string & s, const std::string& suffix)
  {
    if (suffix.size() > s.size())
      return false;
    return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
  }
}  // namespace

namespace DFS
{
  DFS::AbstractImageFile::~AbstractImageFile()
    {
    }

  std::unique_ptr<AbstractImageFile> make_image_file(const std::string& name)
  {
#if USE_ZLIB
    if (ends_with(name, ".ssd.gz"))
      {
	return compressed_image_file(name);
      }
#endif

    std::unique_ptr<std::ifstream> infile(std::make_unique<std::ifstream>(name, std::ifstream::in));
    unsigned long int len = get_file_size(name, *infile);
    if (len < DFS::SECTOR_BYTES * 2)
      throw DFS::eof_in_catalog();

    if (ends_with(name, ".ssd"))
      {
	return std::make_unique<SsdFile>(name, std::move(infile));
      }
    if (ends_with(name, ".dsd"))
      {
	return std::make_unique<DsdFile>(name, std::move(infile));
      }
    std::cerr << "Image file " << name << " does not seem to be of a supported type.\n";
    return 0;
  }


}  // namespace DFS
