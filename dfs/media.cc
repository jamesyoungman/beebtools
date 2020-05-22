#include "media.h"

#include <string.h>

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "dfsimage.h"
#include "dfstypes.h"
#include "storage.h"

namespace
{
  using DFS::sector_count_type;

  bool get_file_size(std::ifstream& infile, unsigned long int* size_in_bytes)
  {
    if (!infile.seekg(0, infile.end))
      return false;
    *size_in_bytes = infile.tellg();
    if (!infile.seekg(0, infile.beg))
      return false;
    return true;
  }

  class SectorCache
  {
  public:
    explicit SectorCache(DFS::sector_count_type initial_sectors)
    {
      cache_.resize(initial_sectors);
    }

    bool has(DFS::sector_count_type sec) const
    {
      return sec < cache_.size() && cache_[sec].get() != 0;
    }

    bool get(DFS::sector_count_type sec, DFS::AbstractDrive::SectorBuffer *buf) const
    {
      if (!has(sec))
	return false;
      *buf = *(cache_[sec].get());
      return true;
    }

    void put(DFS::sector_count_type sec, const DFS::AbstractDrive::SectorBuffer *buf)
    {
      if (sec >= cache_.size())
	return;
      cache_[sec] = std::make_unique<DFS::AbstractDrive::SectorBuffer>();
      std::copy(buf->begin(), buf->end(), cache_[sec]->begin());
    }

  private:
    std::vector<std::unique_ptr<DFS::AbstractDrive::SectorBuffer>> cache_;
  };

  class ImageFile : public DFS::AbstractDrive
  {
  public:
    explicit ImageFile(const std::string& name, std::unique_ptr<std::ifstream>&& ifs)
      : name_(name), f_(std::move(ifs))
    {
      get_file_size(*f_, &file_size_);
    }

    virtual void read_sector(DFS::sector_count_type sector, DFS::AbstractDrive::SectorBuffer* buf,
			     bool& beyond_eof) override
    {
      const unsigned long int pos = sector * DFS::SECTOR_BYTES;
      if (pos >= file_size_)
	{
	  beyond_eof = true;
	  return;
	}
      beyond_eof = false;
      errno = 0;
      if (!f_->seekg(pos, f_->beg))
	{
	  throw DFS::OsError(errno);
	}
      if (!f_->read(reinterpret_cast<char*>(buf->data()), DFS::SECTOR_BYTES).good())
	throw DFS::OsError(errno);
    }

    sector_count_type get_total_sectors() const override
    {
      ldiv_t division = ldiv(file_size_, DFS::SECTOR_BYTES);
      assert(division.quot < std::numeric_limits<sector_count_type>::max());
      return DFS::sector_count(division.quot + (division.rem ? 1 : 0));
    }

    std::string description() const override
    {
      return std::string("image file ") + name_;
    }

  private:
    std::string name_;
    std::unique_ptr<std::ifstream> f_;
    unsigned long int file_size_;
  };

  class CachedDevice : public DFS::AbstractDrive
  {
  public:
    CachedDevice(std::unique_ptr<AbstractDrive> underlying,
		 DFS::sector_count_type cached_sectors)
      : underlying_(std::move(underlying)),
	cache_(cached_sectors)
    {
    }

    virtual void read_sector(DFS::sector_count_type sector, DFS::AbstractDrive::SectorBuffer* buf,
			     bool& beyond_eof) override
    {
      if (cache_.get(sector, buf))
	return;
      underlying_->read_sector(sector, buf, beyond_eof);
      cache_.put(sector, buf);
    }

    sector_count_type get_total_sectors() const override
    {
      return underlying_->get_total_sectors();
    }

    std::string description() const override
    {
      return underlying_->description();
    }

  private:
    std::unique_ptr<DFS::AbstractDrive> underlying_;
    SectorCache cache_;
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
  std::unique_ptr<AbstractDrive> cached_device(std::unique_ptr<AbstractDrive> underlying,
					       DFS::sector_count_type cached_sectors)
  {
    return std::make_unique<CachedDevice>(std::move(underlying), cached_sectors);
  }

  std::unique_ptr<AbstractDrive> cached_image_file(const std::string& name,
						   std::unique_ptr<std::ifstream>&& infile,
						   DFS::sector_count_type cached_sectors)
  {
    return cached_device(std::make_unique<ImageFile>(name, std::move(infile)), cached_sectors);
  }

  std::unique_ptr<AbstractDrive> make_image_file(const std::string& name)
  {
#if USE_ZLIB
    if (ends_with(name, ".ssd.gz"))
      {
	return compressed_image_file(name);
      }
#endif

    std::unique_ptr<std::ifstream> infile(std::make_unique<std::ifstream>(name, std::ifstream::in));
    const unsigned cache_sectors = 4;
    unsigned long int len;
    if (!get_file_size(*infile, &len))
      throw OsError(errno);	// TODO: include file name

    if (len < DFS::SECTOR_BYTES * 2)
      throw DFS::BadFileSystem("disk image is too short to contain a valid catalog");
    std::unique_ptr<ImageFile> failed;
    if (ends_with(name, ".ssd"))
      {
	return cached_image_file(name, std::move(infile), cache_sectors);
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
	return cached_image_file(name, std::move(infile), cache_sectors);

      default:
	std::cerr << "Don't know how to handle an image file " << len
		  << " bytes long\n";
	return failed;
      }
  }


}  // namespace DFS

