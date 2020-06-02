// Acord DFS ambiguous file specification matcher
#include <memory>

#include "dfscontext.h"
#include "dfstypes.h"

namespace DFS
{

class AFSPMatcher
{
  class FactoryKey {};		// see the constructor.

 public:
  static std::unique_ptr<AFSPMatcher>
  make_unique(const DFSContext& ctx, const std::string& pattern, std::string* error);

  bool matches(DFS::drive_number, char directory, const std::string& name);

  // Acorn DFS wildcards can include a drive number, but the drive
  // number field itself cannot be a wildcard.  That is, :*.$.!BOOT is
  // not a valid wildcard.  Hence an AFSP (wildcard) has zero or one
  // associated drive numbers.
  DFS::drive_number get_drive_number() const;
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
  // TODO: move the drive number out of the AFSPMatcher's regex.  It
  // knows which drive it is going to match on, and so there is no
  // point offering it file names from other drives.
  DFS::drive_number drive_num_;
  void *implementation_;
};

  namespace internal
  {
    bool extend_wildcard(DFS::drive_number drive_num, char dir, const std::string& wild,
			 std::string* out, std::string* error_message);
    bool qualify(DFS::drive_number, char dir, const std::string& filename,
		 std::string* out, std::string* error_message);
  }  // namespace DFS::internal

}  // namespace DFS
