// Acord DFS ambiguous file specification matcher
#include <memory>

#include "dfscontext.h"

namespace DFS
{

class AFSPMatcher
{
  class FactoryKey {};		// see the constructor.

 public:
  static std::unique_ptr<AFSPMatcher>
  make_unique(const DFSContext& ctx, const std::string& pattern, std::string* error);

  bool matches(const std::string& name);
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
  DFSContext context_;
  void *implementation_;
};

  namespace internal
  {
    bool extend_wildcard(const DFSContext& ctx, const std::string& wild,
			 std::string* out, std::string* error_message);
  }  // namespace DFS::internal

  namespace internal
  {
    bool qualify(const DFSContext& ctx, const std::string& filename, std::string* out, std::string* error_message);
    bool extend_wildcard(const DFSContext& ctx, const std::string& wild, std::string* out, std::string* error_message);
  }

}  // namespace DFS
