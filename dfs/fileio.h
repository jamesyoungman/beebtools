#ifndef INC_FILEIO_H
#define INC_FILEIO_H 1

#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include "abstractio.h"
#include "dfs.h"
#include "dfs_format.h"
#include "geometry.h"
#include "storage.h"

namespace DFS
{
  namespace internal
  {
    class OsFile : public DFS::DataAccess
    {
    public:
      OsFile(const std::string& name);
      std::optional<DFS::SectorBuffer> read_block(unsigned long lba) override;

    private:
      std::string file_name_;
      std::ifstream f_;
    };

    class NarrowedFileView : public DFS::DataAccess
    {
    public:
      NarrowedFileView(DataAccess& underlying,
		       unsigned long offset_sectors,
		       DFS::sector_count_type limit);
    std::optional<DFS::SectorBuffer> read_block(unsigned long lba) override;

    private:
      DataAccess& underlying_;
      unsigned long offset_;
      DFS::sector_count_type limit_;
    };

    class FileView : public DFS::AbstractDrive
    {
    public:
      FileView(DataAccess& media,
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
	       sector_count_type total);

      // See the comment in read_sector for an explanation of the
      // constructor parameters.
      virtual ~FileView();

      DFS::Geometry geometry() const override;
      DFS::Format format() const;
      std::string description() const override;
      std::optional<DFS::SectorBuffer> read_block(unsigned long sector) override;

    private:
      DataAccess& media_;
      std::string file_name_;
      std::string description_;
      DFS::Format format_;
      DFS::Geometry geometry_;
      // initial_skip_ is wider than sector_count_type because MMB files
      // are much larger than a single disc image.
      unsigned long initial_skip_;
      sector_count_type take_;
      sector_count_type leave_;
      sector_count_type total_;
    };
  }
}

#endif
// Local Vartiables:
// Mode: C++
// End: