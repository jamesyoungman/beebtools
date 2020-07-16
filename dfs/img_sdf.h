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
  class FilePresentedBlockwise : public DataAccess
  {
  public:
    explicit FilePresentedBlockwise(FileAccess& f);
    std::optional<SectorBuffer> read_block(unsigned long lba) override;

  private:
    FileAccess& f_;
  };

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
    ViewFile(const std::string& name, std::unique_ptr<FileAccess>&& file);
    ~ViewFile() override;
    bool connect_drives(StorageConfiguration* storage, DriveAllocation how, std::string& error) override;
    void add_view(const DFS::internal::FileView& v);
    DFS::DataAccess& block_access();

  private:
    std::string name_;
    std::vector<DFS::internal::FileView> views_;
    std::unique_ptr<FileAccess> data_;
    FilePresentedBlockwise blocks_;
  };

  std::unique_ptr<AbstractImageFile> make_mmb_file(const std::string& name,
						   bool compressed,
						   std::unique_ptr<DFS::FileAccess>&& fa);
  std::unique_ptr<AbstractImageFile>
  make_interleaved_file(const std::string& name,
			bool compressed,
			std::unique_ptr<FileAccess>&& media);
  std::unique_ptr<AbstractImageFile>
  make_noninterleaved_file(const std::string& name,
			   bool compressed,
			   std::unique_ptr<FileAccess>&& media);
}  // namespace DFS

#endif
