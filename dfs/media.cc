#include "media.h"

#include <string.h>

#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "abstractio.h"
#include "dfstypes.h"
#include "fileio.h"
#include "identify.h"
#include "storage.h"
#include "stringutil.h"

namespace
{
  using DFS::Format;
  using DFS::Geometry;
  using DFS::ImageFileFormat;
  using DFS::internal::FileView;
  using DFS::sector_count_type;

  class Unrecognized : public std::exception
  {
  public:
    Unrecognized(const std::string& cause)
      : msg_(make_msg(cause))
    {
    }

    const char* what() const noexcept
    {
      return msg_.c_str();
    }

  private:
    static std::string make_msg(const std::string& cause)
    {
      std::ostringstream ss;
      ss <<  "file format was not recognized: " << cause;
      return ss.str();
    }
    std::string msg_;
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
    ViewFile(const std::string& name, std::unique_ptr<DFS::DataAccess>&& media)
      : name_(name), data_(std::move(media))
      {
      }

    ~ViewFile() override
      {
      }

    bool connect_drives(DFS::StorageConfiguration* storage, DFS::DriveAllocation how, std::string& error) override
    {
      std::vector<std::optional<DFS::DriveConfig>> drives;
      for (auto& view : views_)
	{
	  if (!view.is_formatted())
	    {
	      drives.push_back(std::nullopt);
	      continue;
	    }
	  std::string cause;
	  std::optional<Format> fmt = identify_file_system(view, view.geometry(), false, cause);
	  if (!fmt)
	    {
	      std::ostringstream ss;
	      ss << "unable to connect " << view.description() << ": " << cause;
	      error = ss.str();
	      return false;
	    }
	  DFS::DriveConfig dc(*fmt, &view);
	  drives.emplace_back(dc);
	}
      return storage->connect_drives(drives, how);
    }

    void add_view(const FileView& v)
    {
      views_.push_back(v);
    }

    DFS::DataAccess& media()
    {
      return *data_;
    }

  private:
    std::string name_;
    std::vector<FileView> views_;
    std::unique_ptr<DFS::DataAccess> data_;
  };

  class NonInterleavedFile : public ViewFile
  {
  public:
    explicit NonInterleavedFile(const std::string& name, bool compressed,
				std::unique_ptr<DFS::DataAccess>&& access)
      : ViewFile(name, std::move(access))
    {
      // TODO: name != the file inside media for the case where the
      // input file was foo.ssd.gz.  It might be better to keep the
      // original name.
      std::string error;
      auto probe_result = identify_image(media(), name, error);
      if (!probe_result)
	throw Unrecognized(error);

      const DFS::Geometry geometry = probe_result->geometry;
      DFS::sector_count_type skip = 0;
      const DFS::Geometry single_side_geom = DFS::Geometry(geometry.cylinders,
							   1,
							   geometry.sectors,
							   geometry.encoding);
      const DFS::sector_count_type side_len = single_side_geom.total_sectors();
      for (int surface_num = 0; surface_num < geometry.heads; ++surface_num)
	{
	  std::ostringstream os;
	  if (compressed)
	    os << "compressed ";
	  os << "non-interleaved file " << name;
	  if (geometry.heads > 1)
	    {
	      os << " side " << surface_num;
	    }
	  std::string desc = os.str();
	  FileView v(media(), name, desc, single_side_geom,
		     skip, side_len, 0, side_len);
	  skip = DFS::sector_count(skip + side_len);
	  add_view(v);
	}
    }
  };

  class InterleavedFile : public ViewFile
  {
  public:
    explicit InterleavedFile(const std::string& name, bool compressed,
			     std::unique_ptr<DFS::DataAccess>&& access)
      : ViewFile(name, std::move(access))
    {
      auto make_desc = [compressed, &name](int side) -> std::string
		       {
			 std::ostringstream os;
			 os << "side " << side
			    << " of ";
			 if (compressed)
			   os << "compressed ";
			 os << "interleaved file " << name;
			 return os.str();
		       };

      std::string error;
      auto probe_result = identify_image(media(), name, error);
      if (!probe_result)
	throw Unrecognized(error);
      const DFS::Geometry geometry = probe_result->geometry;
      const DFS::Geometry single_side_geom = DFS::Geometry(geometry.cylinders,
							   1,
							   geometry.sectors,
							   geometry.encoding);
      const DFS::sector_count_type track_len = single_side_geom.sectors;
      FileView side0(media(), name, make_desc(0),
		     single_side_geom,
		     0,		/* side 0 begins immediately */
		     track_len, /* read the whole of the track */
		     track_len, /* ignore track data for side 1 */
		     single_side_geom.total_sectors());
      add_view(side0);
      FileView side1(media(), name, make_desc(1),
		     single_side_geom,
		     track_len, /* side 1 begins after the first track of side 0 */
		     track_len, /* read the whole of the track */
		     track_len, /* ignore track data for side 0 */
		     single_side_geom.total_sectors());
      add_view(side1);
    }
  };

  class MmbFile : public ViewFile
  {
  public:
    static constexpr unsigned long MMB_ENTRY_BYTES = 16;

    explicit MmbFile(const std::string& name, bool compressed,
		     std::unique_ptr<DFS::DataAccess>&& access)
      : ViewFile(name, std::move(access))
    {
      const DFS::Geometry disc_image_geom = DFS::Geometry(80, 1, 10, DFS::Encoding::FM);
      const auto disc_image_sectors = disc_image_geom.total_sectors();
      const unsigned long mmb_sectors = 32;
      const unsigned entries_per_sector = DFS::SECTOR_BYTES/MMB_ENTRY_BYTES;
      for (unsigned sec = 0; sec < mmb_sectors; ++sec)
	{
	  auto got = media().read_block(sec);
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
		  continue;
		default:
		  slot_status_desc = "unknown";
		  present = false;
		  fill = 5;
		  // TODO: provide infrsstructure for issuing warnings
		  std::cerr << "MMB entry " << i << " has unexpected type 0x"
			    << std::setw(2) << std::uppercase << std::setbase(16)
			    << entry[0x0F] << "\n";
		  continue;
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
		  DFS::internal::NarrowedFileView narrow(media(), initial_skip_sectors, disc_image_sectors);
		  add_view(FileView(media(), name, disc_name,
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

  std::deque<std::string> split_extensions(const std::string& file_name)
  {
    std::deque<std::string> parts = DFS::stringutil::split(file_name, '.');
    if (!parts.empty())
      {
	parts.pop_front();
      }
    return parts;
  }

}  // namespace

namespace DFS
{
  DFS::AbstractImageFile::~AbstractImageFile()
    {
    }

  std::unique_ptr<AbstractImageFile> make_image_file(const std::string& name, std::string& error)
  {
    std::deque<std::string> extensions = split_extensions(name);
    bool compressed = false;
    if (extensions.empty())
      {
	std::ostringstream ss;
	ss << "Image file " << name
	   << " has no extension, we cnanot tell what kind of image file it is.\n";
	error = ss.str();
	return 0;
      }
    std::unique_ptr<DataAccess> da;
    if (extensions.back() == "gz")
      {
	compressed = true;
	extensions.pop_back();
	if (extensions.empty())
	  {
	    std::ostringstream ss;
	    ss << "Compressed image file " << name
	       << " has no additional extension, "
	       << "we cnanot tell what kind of image file it contains.\n";
	    error = ss.str();
	    return 0;
	  }
	da = std::make_unique<DecompressedFile>(name);
      }
    else
      {
	da = std::make_unique<DFS::internal::OsFile>(name);
      }

    const std::string ext(extensions.back());
    try
      {
	if (ext == "ssd" || ext == "sdd")
	  {
	    return std::make_unique<NonInterleavedFile>(name, compressed, std::move(da));
	  }
	if (ext == "dsd" || ext == "ddd")
	  {
	    return std::make_unique<InterleavedFile>(name, compressed, std::move(da));
	  }
	if (ext == "mmb")
	  {
	    return std::make_unique<MmbFile>(name, compressed, std::move(da));
	  }
	std::ostringstream ss;
	ss << "Image file " << name << " does not seem to be of a supported type; "
	   << "the extension " << ext << " is not recognised.\n";
	error = ss.str();
	return 0;
      }
    catch (Unrecognized& e)
      {
	// TODO: this breaks the convention that only the UI is
	// allowed to interace with the input/output.
	error = e.what();
	return 0;
      }
  }

}  // namespace DFS
