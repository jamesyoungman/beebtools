#include "dfs_format.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "abstractio.h"
#include "dfs.h"
#include "dfs_catalog.h"
#include "dfs_format.h"
#include "exceptions.h"
#include "stringutil.h"
#include "identify.h"

namespace
{
  void eliminated_geometry(const DFS::Geometry& g, const std::string& reason)
  {
    if (!DFS::verbose)
      return;
    std::cerr << "Eliminated geometry " << g.description()
	      << " because " << reason << "\n";
  }

  void eliminated_format(const DFS::ImageFileFormat& ff, const std::string& reason)
  {
    if (!DFS::verbose)
      return;
    std::cerr << "Eliminated file format " << ff.description()
	      << " because " << reason << "\n";
  }

  void eliminated_format(const DFS::Format& df, const std::string& reason)
  {
    if (!DFS::verbose)
      return;
    std::cerr << "Eliminated file system format " << format_name(df)
	      << " because " << reason << "\n";
  }

  DFS::sector_count_type get_hdfs_sector_count(const DFS::SectorBuffer& sec1)
  {
    auto sectors_per_side = sec1[0x07] // bits 0-7
      | (sec1[0x06] & 3) << 8;     // bits 8-9
    auto side_shift = 1 + (sec1[0x06] & 4) ? 1 : 0;
    return DFS::sector_count_type(sectors_per_side << side_shift);
  }

  void show_possible(const std::string& intro,
		     const std::vector<DFS::ImageFileFormat>& candidates)
  {
    if (!DFS::verbose)
      return;
    int i = 1;
    std::cerr << intro << "\n";
    for (const auto& cand : candidates)
      {
	std::cerr << std::setfill(' ') << std::setw(2) << std::dec << std::right
		  << i++ << ". " << cand.description() << "\n";
      }
  }


  DFS::sector_count_type get_dfs_sector_count(const DFS::SectorBuffer& sec1)
  {
    return DFS::sector_count_type(sec1[0x07] // bits 0-7
				  | (sec1[0x06] & 7) << 8); // bits 8-10
  }

  bool smells_like_hdfs(const DFS::SectorBuffer& sec1)
  {
    // The cycle count byte of the root catalog is, apparently, a
    // checksum.  TODO: check the checksum.
    return sec1[0x06] & 8;
  }

  bool smells_like_watford(DFS::DataAccess& access,
			   const DFS::SectorBuffer& buf1) // sector 1
  {

    // DFS provides 31 file slots, and Watford DFS 62.  Watford DFS
    // does this by doubling the size of the catalog into sectors 2
    // and 3 (as well as DFS's 0 and 1).  It puts recognition bytes in
    // sector 2.  However, it's possible for a DFS-format file to
    // contain the recognition bytes in its body.  We don't want to be
    // fooled if that happens.  To avoid it, we check whether the body
    // of any file (of the standard DFS 31 files) starts in sector 2.
    // If so, this cannot be a Watford DFS format disc.
    const DFS::byte last_catalog_entry_pos = buf1[0x05];
    unsigned pos = 8;
    for (pos = 8; pos <= last_catalog_entry_pos; pos += 8)
      {
	auto start_sector = buf1[pos + 7];
	if (start_sector == 2)
	  {
	    /* Sector 2 is used by a file, so not Watford DFS. */
	    eliminated_format(DFS::Format::WDFS, "sector 2 is in use by a file");
	    return false;
	  }
      }

    // Look for the Watford DFS recognition string
    // in the initial entry in its extended catalog.
    auto got = access.read_block(2);
    if (!got)
      {
	eliminated_format(DFS::Format::WDFS, "media is not long enough for a 62-file catalog");
	return false;
      }
    else
      {
	if (std::all_of(got->cbegin(), got->cbegin()+0x08,
			[](DFS::byte b) { return b == 0xAA; }))
	  {
	    return true;
	  }
	eliminated_format(DFS::Format::WDFS, "Watford marker bytes are not present");
	return false;
      }
  }

  bool smells_like_opus_ddos(DFS::DataAccess& media, DFS::sector_count_type* sectors)
  {
    // If this is an Opus single-density disk, it is identical in
    // format to an Acorn DFS disk.  If it's an Opus DDOS
    // double-density disk, it may have additional volumes B-H listed
    // in track 0.
    std::optional<DFS::SectorBuffer> got = media.read_block(16);
    if (!got)
       {
        eliminated_format(DFS::Format::OpusDDOS,
                          "the disc is too short to contain an Opus DDOS volume disc catalogue");
        return false;
       }
    const DFS::SectorBuffer& sector16(*got);
    int total_disk_sectors = (sector16[1] << 8) | sector16[2];
    switch (total_disk_sectors)
      {
      case 720: // 40 tracks, 18 sectors per track
      case 1440: // 80 tracks, 18 sectors per track
        break;
      default:
        {
          std::ostringstream ss;
          ss << "total sectors field of sector 16 is " << total_disk_sectors
             << " but we assume only 720 (40 tracks) or 1440 (80 tracks)"
             << " is actually possible for the Opus DDOS format\n";
          eliminated_format(DFS::Format::OpusDDOS, ss.str().c_str());
          return false;
        }
      }

    // This is perhaps over-cautious.  But, reject an image file which
    // is physically shorter than the metadata says it should be.
    // Sometimes emulators produce these (and consuming programs
    // generally assume the data "off the end" of the disk image is
    // all-zero).
    auto last = media.read_block(DFS::sector_count(total_disk_sectors - 1u));
    if (!last)
      {
        std::ostringstream ss;
        ss << "total sectors field of sector 16 is " << total_disk_sectors
           << " but the disc image itself has fewer sectors.\n";
        eliminated_format(DFS::Format::OpusDDOS, ss.str().c_str());
        return false;
      }
    if (sector16[3] != 18)      // must be 18 sectors-per-track
      {
        std::ostringstream ss;
        ss << "the sectors-per-track field of sector 16 is " << int(sector16[3])
           << " but for Opus DDOS we expect 18\n";
        return false;
      }
    // &04 is apparently the number of tracks in the disc, but I see
    // disc images with 0 there.

    // TODO: check that all the file positions in the A-H catalogs (if
    // present) are within the bounds of the relevant volume.  We
    // already know that the catalog for volume A is consistent being
    // Opus (i.e. is not HDFS or Watford).
    *sectors = DFS::sector_count(total_disk_sectors);
    return true;
  }

  bool has_valid_dfs_catalog(DFS::DataAccess& media,
			     unsigned long location,
			     std::string& error)
  {
    std::optional<DFS::SectorBuffer> s0 = media.read_block(location);
    std::optional<DFS::SectorBuffer> s1 = media.read_block(location + 1uL);
    if (!s0 || !s1)
      {
	std::ostringstream os;
	os << "media is too short to contain a catalog at location "
	   << std::dec << location;
	error = os.str();
	return false;
      }
    DFS::CatalogFragment f(DFS::Format::DFS, *s0, *s1);
    const bool valid = f.valid(error);
    if (valid)
      {
	if (DFS::verbose)
	  {
	    std::cerr << "catalog fragment is valid: " << f << "\n";
	  }
      }
    return valid;
  }

  std::vector<DFS::ImageFileFormat>
  filter_formats(const std::vector<DFS::ImageFileFormat>& candidates,
		 std::function<bool(const DFS::ImageFileFormat&)> pred)
  {
    std::vector<DFS::ImageFileFormat> result;
    result.reserve(candidates.size());
    std::copy_if(candidates.begin(), candidates.end(),
		 std::back_inserter(result), pred);
    return result;
  }

  DFS::ImageFileFormat
  probe_geometry(DFS::DataAccess& media,
		 DFS::Format fmt, DFS::sector_count_type total_sectors,
		 const std::vector<DFS::ImageFileFormat>& candidates)
  {
    show_possible("probe_geometry initial possibilities", candidates);
    auto large_enough =
      [total_sectors, fmt](const DFS::ImageFileFormat& ff) -> bool
      {
	// We do not use ff.geometry.total_sectors for the comparison
	// below because it would lead us to accept a double-sided
	// 40-track geometry (having enough sectors for the file
	// system counting both sides) where in reality the only
	// acceptable option is a single sided 80-track geometry
	// (because the file system actually only reads from one side
	// of the disc).
	DFS::sector_count_type available_sectors;
	std::string sides_desc;
	if (DFS::single_sided_filesystem(fmt))
	  {
	    available_sectors = DFS::sector_count(ff.geometry.cylinders * ff.geometry.sectors);
	    sides_desc = "single-sided";
	  }
	else
	  {
	    available_sectors = ff.geometry.total_sectors();
	    sides_desc = "two-sided";
	  }
	if (available_sectors >= total_sectors)
	  {
	    if (DFS::verbose)
	      {
		std::cerr << "Candidate format " << ff.description()
			  << " has " << available_sectors << " available sectors "
			  << "for a " << sides_desc << " filesystem and so "
			  << "is large enough to hold a filesystem "
			  << "contaiing " << total_sectors << " sectors.\n";
	      }
	    return true;
	  }
	std::ostringstream os;
	os << "that geometry has " << available_sectors
	   << " sectors available to a " << sides_desc
	   << " file system and so it is too small to hold a "
	   << "file system of " << total_sectors << " sectors";
	eliminated_geometry(ff.geometry, os.str());
	return false;
      };
    std::vector<DFS::ImageFileFormat> possible = filter_formats(candidates, large_enough);
    show_possible("probe_geometry after eliminating under-sized geometries", possible);

    // other_side_has_catalog_too eliminates geometries in which the
    // other side of the media should also have a catalog, but where we
    // cannot find such a catalog in the implied location.  This helps
    // us distinguish 40 track two-sided SSD files from 80 track
    // one-sided SSD files, for example.
    auto other_side_has_catalog_too =
      [total_sectors, &media](const DFS::ImageFileFormat& ff) -> bool
      {
	// TODO: figure out how to handle two-sided formats such as HDFS
	// which can (sometimes) occupy both sides of the disc.
	if (ff.geometry.heads == 1)
	  return true;
	DFS::sector_count_type other =
	  DFS::sector_count(ff.geometry.sectors
			    * (ff.interleaved ? 1 : ff.geometry.cylinders));
	std::string error;
	if (has_valid_dfs_catalog(media, other, error))
	  {
	    return true;
	  }

	std::ostringstream os;
	os << "this two-sided format should also have a catalog at sector "
	   << other << " but the data at that location is not a valid catalog: "
	   << error;
	eliminated_format(ff, os.str());
	return false;
      };
    possible = filter_formats(possible, other_side_has_catalog_too);
    show_possible("probe_geometry after removing two-sided geometries lacking a catalog on the other side", possible);

    if (possible.size() > 1 && DFS::verbose)
      {
	show_possible("The remaining possible formats cannot be conclusively rejected", possible);
      }
    auto compare_capacity =
      [](const DFS::ImageFileFormat& left,
	 const DFS::ImageFileFormat& right)
      {
	return left.geometry.total_sectors() < right.geometry.total_sectors();
      };
    auto it = std::min_element(possible.cbegin(), possible.cend(), compare_capacity);
    if (it == possible.cend())
      {
	throw DFS::FailedToGuessFormat("all known formats have been eliminated");
      }
    if (DFS::verbose)
      {
	std::cerr << "Selected the "
		  << (possible.size() == 1 ? "only" : "smallest")
		  << " remaining format: "
		  << it->description() << "\n";
      }
    return *it;
  }

  std::pair<DFS::Format, DFS::sector_count_type>
  probe_format(DFS::DataAccess& access)
  {
    DFS::SectorBuffer buf1;
    auto got = access.read_block(1);
    if (!got)
      throw DFS::eof_in_catalog();
    buf1 = *got;

    if (smells_like_hdfs(buf1))
      {
	return std::make_pair(DFS::Format::HDFS, get_hdfs_sector_count(buf1));
      }

    if (smells_like_watford(access, buf1))
      {
       return std::make_pair(DFS::Format::WDFS, get_dfs_sector_count(buf1));
      }

    DFS::sector_count_type opus_sectors;
    if (smells_like_opus_ddos(access, &opus_sectors))
      {
	return std::make_pair(DFS::Format::OpusDDOS, opus_sectors);
      }

    return std::make_pair(DFS::Format::DFS, get_dfs_sector_count(buf1));
  }

  std::pair<DFS::Format, DFS::ImageFileFormat>
  probe(DFS::DataAccess& access,
	const std::vector<DFS::ImageFileFormat>& candidates)
  {
    DFS::Format fmt;
    DFS::sector_count_type total_sectors;
    std::tie(fmt, total_sectors) = probe_format(access);
    if (DFS::verbose)
      {
	std::cerr << "File system format appears to be " << format_name(fmt)
		  << " occupying " << std::dec << total_sectors
		  << " sectors.\n";
      }
    DFS::ImageFileFormat ff = probe_geometry(access, fmt, total_sectors, candidates);
    return std::make_pair(fmt, ff);
  }

  std::vector<DFS::sector_count_type> sectors_per_track_options(DFS::Encoding e)
  {
    if (e == DFS::Encoding::FM)
      return std::vector<DFS::sector_count_type>{DFS::sector_count(10)};
    else
      return std::vector<DFS::sector_count_type>
	{ DFS::sector_count(16), DFS::sector_count(18) };
  }

  std::vector<DFS::Encoding> encoding_options(std::optional<DFS::Encoding> hint)
  {
    if (hint)
      return std::vector<DFS::Encoding>{*hint};
    else
      return std::vector<DFS::Encoding>{DFS::Encoding::FM, DFS::Encoding::MFM};
  }

  std::vector<bool> interleaving_options(std::optional<bool> hint)
  {
    if (hint)
      return std::vector<bool>{*hint};
    else
      return std::vector<bool>{false, true};
  }

  std::vector<int> sides_options(std::optional<int> hint)
  {
    if (hint)
      return std::vector<int>{*hint};
    else
      return std::vector<int>{2, 1};
  }

  std::vector<DFS::ImageFileFormat> make_candidate_list(const std::string& name)
  {
    std::optional<DFS::Encoding> encoding_hint;
    std::optional<bool> interleaving_hint;
    std::optional<int> sides_hint;
    if (DFS::stringutil::ends_with(name, ".ssd"))
      {
	interleaving_hint = false;
      }
    else if (DFS::stringutil::ends_with(name, ".dsd"))
      {
	interleaving_hint = true;
	sides_hint = 2;
      }

    // Some combinations are documented at
    // http://mdfs.net/Docs/Comp/Disk/Format/Formats, but it doesn't
    // include the Opus 35-track variant.
    //
    // HDFS has a format which occupies both sides which we don't yet
    // cope with here.
    std::vector<DFS::ImageFileFormat> candidates;
    candidates.reserve(48);
    for (DFS::Encoding encoding : encoding_options(encoding_hint))
      {
	for (int sides : sides_options(sides_hint))
	  {
	    for (int tracks : {40,80}) // TODO: Opus, 35
	      {
		for (DFS::sector_count_type sectors : sectors_per_track_options(encoding))
		  {
		    const DFS::Geometry g(tracks, sides, sectors, encoding);
		    for (bool interleave : interleaving_options(interleaving_hint))
		      {
			candidates.emplace_back(g, interleave);
		      }
		  }
	      }
	  }
      }
    return candidates;
  }

}  // namespace


namespace DFS
{
  std::pair<Format, ImageFileFormat>
  identify_image(DataAccess& access, const std::string& name)
  {
    std::vector<ImageFileFormat> candidates = make_candidate_list(name);
    return probe(access, candidates);
  }

  Format identify_file_system(DataAccess& access, Geometry geom, bool interleaved)
  {
    const std::vector<ImageFileFormat> only{ImageFileFormat(geom, interleaved)};
    return probe(access, only).first;
  }

  std::string ImageFileFormat::description() const
  {
    std::ostringstream os;
    os << (interleaved ? "" : "non-") << "interleaved file, " << geometry.description();
    return os.str();
  }

}  // namespace DFS