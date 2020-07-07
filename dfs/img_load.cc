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
#include "exceptions.h"
#include "img_fileio.h"
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

    if (extensions.back() == "hfe")
      {
	return make_hfe_file(name, error);
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
	       << "we cannot tell what kind of image file it contains.\n";
	    error = ss.str();
	    return 0;
	  }
	da = DFS::make_decompressed_file(name);
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
	    return make_noninterleaved_file(name, compressed, std::move(da));
	  }
	if (ext == "dsd" || ext == "ddd")
	  {
	    return make_interleaved_file(name, compressed, std::move(da));
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
