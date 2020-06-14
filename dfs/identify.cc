#include "dfs_format.h"

#include <algorithm>

#include "abstractio.h"
#include "exceptions.h"

namespace
{
  DFS::sector_count_type get_hdfs_sector_count(const DFS::SectorBuffer& sec1)
  {
    auto sectors_per_side = sec1[0x07] // bits 0-7
      | (sec1[0x06] & 3) << 8;     // bits 8-9
    auto side_shift = 1 + (sec1[0x06] & 4) ? 1 : 0;
    return DFS::sector_count_type(sectors_per_side << side_shift);
  }

  DFS::sector_count_type get_dfs_sector_count(const DFS::SectorBuffer& sec1)
  {
    return DFS::sector_count_type(sec1[0x07] // bits 0-7
				  | (sec1[0x06] & 7) << 8); // bits 8-10
  }

}  // namespae


namespace DFS
{
  std::pair<Format, DFS::sector_count_type>
  identify_image_format(const DataAccess& io)
  {
    std::optional<SectorBuffer> sec = io.read_block(1);
    if (!sec)
      throw DFS::eof_in_catalog();
    SectorBuffer buf(*sec);

    if (buf[0x06] & 8)
      return std::make_pair(Format::HDFS, get_hdfs_sector_count(buf));

    const auto sectors = get_dfs_sector_count(buf);

    // DFS provides 31 file slots, and Watford DFS 62.  Watford DFS does
    // this by doubling the size of the catalog into sectors 2 and 3 (as
    // well as DFS's 0 and 1).  It puts recognition bytes in sector 2.
    // However, it's possible for a DFS-format file to contain the
    // recognition bytes in its body.  We don't want to be fooled if
    // that happens.  To avoid it, we check whether the body of any file
    // (of the standard DFS 31 files) starts in sector 2.  If so, this
    // cannot be a Watford DFS format disc.
    const byte last_catalog_entry_pos = buf[0x05];
    unsigned pos = 8;
    for (pos = 8; pos <= last_catalog_entry_pos; pos += 8)
      {
	auto start_sector = buf[pos + 7];
	if (start_sector == 2)
	  {
	    /* Sector 2 is used by a file, so not Watford DFS. */
	    return std::make_pair(Format::DFS, sectors);
	  }
      }

    // Look for the Watford DFS recognition string
    // in the initial entry in its extended catalog.
    sec = io.read_block(2);
    if (sec)
      {
	buf = *sec;
	if (std::all_of(buf.cbegin(), buf.cbegin()+0x08,
			[](byte b) { return b == 0xAA; }))
	  {
	    return std::make_pair(Format::WDFS, sectors);
	  }
      }
    // Either the recognition bytes were not there (meaning it's not a
    // Watford DFS 62 file catalog) or the disk image is too short to
    // contain sector 2 (meaning that the recognition bytes cannot be
    // there beyond the end of the "media").
    return std::make_pair(Format::DFS, sectors);
  }

}  // namespace DFS
