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
#include "img_sdf.h"     // for ViewFile, make_mmb_file

#include <array>         // for array
#include <iomanip>       // for operator<<, setw, setbase, setfill, uppercase
#include <iostream>      // for operator<<, basic_ostream, basic_ostream::op...
#include <memory>        // for unique_ptr, allocator, make_unique
#include <optional>      // for optional
#include <string>        // for string, char_traits, operator<<
#include <utility>       // for move
#include "abstractio.h"  // for DataAccess, SECTOR_BYTES
#include "dfstypes.h"    // for sector_count
#include "exceptions.h"  // for BadFileSystem
#include "geometry.h"    // for Encoding, Geometry, Encoding::FM
#include "img_fileio.h"  // for FileView
#include "media.h"       // for AbstractImageFile

namespace
{
  using DFS::ViewFile;
  using DFS::internal::FileView;

  class MmbFile : public ViewFile
  {
  public:
    static constexpr unsigned long MMB_ENTRY_BYTES = 16;

    explicit MmbFile(const std::string& name, bool compressed,
		     std::unique_ptr<DFS::FileAccess>&& file)
      : ViewFile(name, std::move(file))
    {
      const DFS::Geometry disc_image_geom = DFS::Geometry(80, 1, 10, DFS::Encoding::FM);
      const auto disc_image_sectors = disc_image_geom.total_sectors();
      const unsigned long mmb_sectors = 32;
      const unsigned entries_per_sector = DFS::SECTOR_BYTES/MMB_ENTRY_BYTES;
      for (unsigned sec = 0; sec < mmb_sectors; ++sec)
	{
	  auto got = block_access().read_block(sec);
	  if (!got)
	    throw DFS::BadFileSystem("MMB file is too short");
	  for (unsigned i = 0; i < entries_per_sector; ++i)
	    {
	      if (sec == 0 && i == 0)
		{
		  // We don't actually care about the header; it specifies
		  // which drives are loaded at boot time, but we don't
		  // need to know that.
		  continue;
		}
	      const int slot = (sec * entries_per_sector) + i - 1;
	      const unsigned char *entry = got->data() + (i * MMB_ENTRY_BYTES);
	      const auto slot_status = entry[0x0F];
	      std::string slot_status_desc;
	      bool present = false;
	      int fill = 0;
	      switch (slot_status)
		{
		case 0x00:		// read-only
		  slot_status_desc = "read-only";
		  present = true;
		  fill = 2;
		  break;
		case 0x0F:		// read-write
		  slot_status_desc = "read-write";
		  present = true;
		  fill = 1;
		  break;
		case 0xF0:		// unformatted
		  slot_status_desc = "unformatted";
		  present = false;
		  fill = 0;
		  break;
		case 0xFF:		// invalid, perhaps missing
		  slot_status_desc = "missing";
		  present = false;
		  fill = 4;
		  break;
		default:
		  slot_status_desc = "unknown";
		  present = false;
		  fill = 5;
		  // TODO: provide infrastructure for issuing warnings
		  std::cerr << "MMB entry " << i << " has unexpected type 0x"
			    << std::setw(2) << std::uppercase << std::setbase(16)
			    << entry[0x0F] << "\n";
		  break;
		}
	      std::ostringstream ss;
	      ss << std::setfill(' ') << std::setw(fill) << "" << slot_status_desc
		 << " slot " << std::setw(3) << slot << " of "
		 << (compressed ? "compressed " : "")
		 << "MMB file " << name;
	      const std::string disc_name = ss.str();
	      if (present)
		{
		  auto initial_skip_sectors = mmb_sectors + (slot * disc_image_sectors);
		  add_view(FileView(block_access(), name, disc_name,
				    disc_image_geom,
				    initial_skip_sectors,
				    disc_image_sectors,
				    DFS::sector_count(0),
				    disc_image_sectors));
		}
	      else
		{
		  add_view(FileView::unformatted_device(name, disc_name, disc_image_geom));
		}
	    }
	}
    }
  };
}  // namespace

namespace DFS
{
  std::unique_ptr<AbstractImageFile> make_mmb_file(const std::string& name,
						   bool compressed,
						   std::unique_ptr<DFS::FileAccess>&& fa)
  {
    return std::make_unique<MmbFile>(name, compressed, std::move(fa));
  }
}  // namespace DFS
