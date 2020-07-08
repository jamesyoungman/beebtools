#include "img_sdf.h"

#include <optional>
#include <string>
#include <sstream>


#include "abstractio.h"
#include "identify.h"
#include "storage.h"

namespace DFS
{
  ViewFile::ViewFile(const std::string& name, std::unique_ptr<DFS::DataAccess>&& media)
    : name_(name), data_(std::move(media))
  {
  }

  ViewFile::~ViewFile()
  {
  }

  void ViewFile::add_view(const DFS::internal::FileView& v)
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
}  // namespace DFS
