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
#ifndef INC_MEDIA_H
#define INC_MEDIA_H 1

#include <exception>
#include <memory>
#include <stdio.h>
#include <stdexcept>
#include <string>

#include <string.h>

#include "dfstypes.h"
#include "exceptions.h"
#include "storage.h"

namespace DFS
{
  class AbstractImageFile
  {
  public:
    virtual ~AbstractImageFile();
    virtual bool connect_drives(DFS::StorageConfiguration* storage, DFS::DriveAllocation how, std::string& error) = 0;
  };

  std::unique_ptr<AbstractImageFile> make_image_file(const std::string& file_name, std::string& error);

#if USE_ZLIB
  std::unique_ptr<FileAccess> make_decompressed_file(const std::string& name);
#endif

  std::unique_ptr<AbstractImageFile> make_hfe_file(const std::string& name,
						   bool compressed,
						   std::unique_ptr<DFS::FileAccess> file,
						   std::string& error);

}  // namespace DFS
#endif
