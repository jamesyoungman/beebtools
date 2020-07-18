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
#include <deque>               // for deque
#include <fstream>             // for operator<<, basic_ostream, ostringstream
#include <memory>              // for unique_ptr, make_unique
#include <sstream>             // ostringstream
#include <string>              // for string, operator==, operator<<, ...
#include <utility>             // for move

#include "abstractio.h"        // for FileAccess
#include "dfs_format.h"        // for Format
#include "dfstypes.h"          // for sector_count_type
#include "exceptions.h"        // for Unrecognized
#include "img_fileio.h"        // for OsFile
#include "img_sdf.h"           // for make_interleaved_file, make_mmb_file
#include "media.h"             // for AbstractImageFile, make_decompressed_file
#include "stringutil.h"        // for split

namespace DFS { struct Geometry; }

namespace
{
  using DFS::Format;
  using DFS::Geometry;
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

    std::unique_ptr<FileAccess> fa;
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
	fa = DFS::make_decompressed_file(name);
      }
    else
      {
	fa = std::make_unique<DFS::internal::OsFile>(name);
      }

    const std::string ext(extensions.back());
    try
      {
	if (extensions.back() == "hfe")
	  {
	    return make_hfe_file(name, compressed, std::move(fa), error);
	  }
	if (ext == "ssd" || ext == "sdd")
	  {
	    return make_noninterleaved_file(name, compressed, std::move(fa));
	  }
	if (ext == "dsd" || ext == "ddd")
	  {
	    return make_interleaved_file(name, compressed, std::move(fa));
	  }
	if (ext == "mmb")
	  {
	    return make_mmb_file(name, compressed, std::move(fa));
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
