#include "dfs_format.h"
#include "exceptions.h"

namespace DFS
{
  Format identify_format(AbstractDrive* drive)
  {
    return identify_format([drive](DFS::sector_count_type sector_num,
				   DFS::SectorBuffer* buf,
				   bool& beyond_eof)
			   {
			     drive->read_sector(sector_num, buf, beyond_eof);
			   });
  }


  Format identify_format(std::function<void(DFS::sector_count_type,
					    DFS::SectorBuffer*,
					    bool&)> sector_reader)
  {
    DFS::SectorBuffer buf;
    bool beyond_eof = false;
    sector_reader(1, &buf, beyond_eof);
    if (beyond_eof)
      throw DFS::eof_in_catalog();

    if (buf[0x06] & 8)
      return Format::HDFS;

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
	    return Format::DFS;
	  }
      }

    // Look for the Watford DFS recognition string
    // in the initial entry in its extended catalog.
    sector_reader(2, &buf, beyond_eof);
    if (!beyond_eof)
      {
	if (std::all_of(buf.cbegin(), buf.cbegin()+0x08,
			[](byte b) { return b == 0xAA; }))
	  {
	    return Format::WDFS;
	  }
      }
    // Either the recognition bytes were not there (meaning it's not a
    // Watford DFS 62 file catalog) or the disk image is too short to
    // contain sector 2 (meaning that the recognition bytes cannot be
    // there beyond the end of the "media").
    return Format::DFS;
  }

}  // namespace DFS
