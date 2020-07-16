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
