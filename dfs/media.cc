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
#include "img_sdf.h"
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
	    return make_mmb_file(name, compressed, std::move(da));
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
