#ifndef INC_MEDIA_H
#define INC_MEDIA_H 1

#include <exception>
#include <memory>
#include <stdio.h>
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
  class DecompressedFile : public DataAccess
  {
  public:
    explicit DecompressedFile(const std::string& name);
    virtual ~DecompressedFile();
    std::optional<SectorBuffer> read_block(unsigned long lba) override;

  private:
    FILE *f_;
    std::string name_;
  };
#endif
}  // namespace DFS
#endif
