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
// Acord DFS ambiguous file specification matcher
#include <memory>

#include "dfscontext.h"
#include "dfstypes.h"
#include "driveselector.h"

namespace DFS
{

class AFSPMatcher
{
  class FactoryKey {};		// see the constructor.

 public:
  static std::unique_ptr<AFSPMatcher>
  make_unique(const DFSContext& ctx, const std::string& pattern, std::string* error);

  bool matches(DFS::VolumeSelector vol, char directory, const std::string& name);

  // Acorn DFS wildcards can include a drive number, but the drive
  // number field itself cannot be a wildcard.  That is, :*.$.!BOOT is
  // not a valid wildcard.  Hence an AFSP (wildcard) has zero or one
  // associated drive numbers.  We extend the idea to cover Opus DDOS
  // volumes.
  DFS::VolumeSelector get_volume() const;
  static bool self_test();

  // The point of FactoryKey is to ensure that only MakeUnique() can
  // create instances.  We can't just make the constructor pivate since
  // std::make_unique() needs to be able to call the constructor, and
  // std::make_unique() is not itself a member.
  explicit AFSPMatcher(const FactoryKey&, const DFSContext&, const std::string& pattern,
		       std::string* error_message);
  ~AFSPMatcher();

 private:
  bool valid() const
  {
    return valid_;
  }

  bool valid_;
  // TODO: move the volume selector out of the AFSPMatcher's regex.
  // It knows which volume it is going to match on (because we can ask
  // for the value of vol_), and so there is no point offering it file
  // names from other drives.
  DFS::VolumeSelector vol_;
  void *implementation_;
};

  namespace internal
  {
    bool extend_wildcard(DFS::VolumeSelector vol, char dir, const std::string& wild,
			 std::string* out, std::string* error_message);
    bool qualify(DFS::VolumeSelector, char dir, const std::string& filename,
		 std::string* out, std::string* error_message);
  }  // namespace DFS::internal

}  // namespace DFS
