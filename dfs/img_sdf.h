/* Declarations for code relating to sector dump image file formats
 * for example SSD, SDD, DSD, DDD, MMB.
 */
#ifndef INC_IMG_SDF_H
#define INC_IMG_SDF_H 1

#include <memory>
#include <string>
#include <vector>

#include "abstractio.h"
#include "img_fileio.h"
#include "media.h"
#include "storage.h"

namespace DFS
{
  // A ViewFile is a disc image file which contains the sectors of one
  // or more emulated devices, in order, but with regular gaps.
  // Examples include a DSD file (which contains all the sectors from
  // one side of a cylinder, then all the sectors of the other side of
  // a cylinder) or MMB files (which contains a concatenation of many
  // disc images).  An SSD file is a degenarate example, in the sense
  // that it can be described in the same way but has no gaps.
  class ViewFile : public AbstractImageFile
  {
  public:
    ViewFile(const std::string& name, std::unique_ptr<DataAccess>&& media);
    ~ViewFile() override;
    bool connect_drives(StorageConfiguration* storage, DriveAllocation how, std::string& error) override;
    void add_view(const DFS::internal::FileView& v);
    DFS::DataAccess& media();

  private:
    std::string name_;
    std::vector<DFS::internal::FileView> views_;
    std::unique_ptr<DataAccess> data_;
  };

  std::unique_ptr<AbstractImageFile> make_mmb_file(const std::string& name,
						   bool compressed,
						   std::unique_ptr<DFS::DataAccess>&& da);
  std::unique_ptr<AbstractImageFile>
  make_interleaved_file(const std::string& name,
			bool compressed,
			std::unique_ptr<DataAccess>&& media);
  std::unique_ptr<AbstractImageFile>
  make_noninterleaved_file(const std::string& name,
			   bool compressed,
			   std::unique_ptr<DataAccess>&& media);
}  // namespace DFS

#endif
