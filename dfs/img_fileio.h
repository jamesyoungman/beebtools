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
#ifndef INC_FILEIO_H
#define INC_FILEIO_H 1

#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "abstractio.h"
#include "dfs.h"
#include "dfs_format.h"
#include "geometry.h"
#include "storage.h"

namespace DFS
{
  namespace internal
  {
    class OsFile : public DFS::FileAccess
    {
    public:
      OsFile(const std::string& name);
      std::vector<byte> read(unsigned long offset, unsigned long len) override;

    private:
      std::string file_name_;
      std::ifstream f_;
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
	       const DFS::Geometry& geometry,
	       unsigned long initial_skip,
	       sector_count_type take,
	       sector_count_type leave,
	       sector_count_type total);
      static FileView unformatted_device(const std::string& file_name,
					 const std::string& description,
					 const DFS::Geometry& geometry);
      bool is_formatted() const;

      // See the comment in read_sector for an explanation of the
      // constructor parameters.
      virtual ~FileView();

      DFS::Geometry geometry() const override;
      std::string description() const override;
      std::optional<DFS::SectorBuffer> read_block(unsigned long sector) override;

    private:
      DataAccess& media_;
      std::string file_name_;
      std::string description_;
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
