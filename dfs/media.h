#ifndef INC_MEDIA_H
#define INC_MEDIA_H 1

#include <exception>
#include <memory>
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
    virtual bool connect_to(DFS::StorageConfiguration* storage, DFS::DriveAllocation how) = 0;
  };

  std::unique_ptr<AbstractImageFile> make_image_file(const std::string& file_name);
#if USE_ZLIB
  std::unique_ptr<AbstractImageFile> compressed_image_file(const std::string& name);
#endif
}  // namespace DFS
#endif
