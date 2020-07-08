#include "img_sdf.h"

#include <optional>
#include <string>
#include <sstream>


#include "abstractio.h"
#include "exceptions.h"
#include "img_fileio.h"
#include "identify.h"
#include "storage.h"

namespace
{
  using DFS::internal::FileView;

  class NonInterleavedFile : public DFS::ViewFile
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
	throw DFS::Unrecognized(error);

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

  class InterleavedFile : public DFS::ViewFile
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
	throw DFS::Unrecognized(error);
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

}  // namespace

namespace DFS
{
  ViewFile::ViewFile(const std::string& name, std::unique_ptr<DFS::DataAccess>&& media)
    : name_(name), data_(std::move(media))
  {
  }

  ViewFile::~ViewFile()
  {
  }

  void ViewFile::add_view(const FileView& v)
  {
    views_.push_back(v);
  }

  DFS::DataAccess& ViewFile::media()
  {
    return *data_;
  }

  bool ViewFile::connect_drives(DFS::StorageConfiguration* storage,
				DFS::DriveAllocation how,
				std::string& error)
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

  std::unique_ptr<AbstractImageFile> make_noninterleaved_file(const std::string& name,
							      bool compressed,
							      std::unique_ptr<DataAccess>&& media)
  {
    return std::make_unique<NonInterleavedFile>(name, compressed, std::move(media));
  }

  std::unique_ptr<AbstractImageFile> make_interleaved_file(const std::string& name,
							   bool compressed,
							   std::unique_ptr<DataAccess>&& media)
  {
    return std::make_unique<InterleavedFile>(name, compressed, std::move(media));
  }

}  // namespace DFS
