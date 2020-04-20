// Acord DFS ambiguous file specification matcher
#include <memory>

#include "dfscontext.h"

class AFSPMatcher 
{
  class FactoryKey {};		// see the constructor.

 public:
  static std::unique_ptr<AFSPMatcher>
  MakeUnique(const DFSContext& ctx, const std::string& pattern, std::string* error);

  bool Matches(const std::string& name);
  static bool SelfTest();

  // The point of FactoryKey is to ensure that only MakeUnique() can
  // create instances.  We can't just make the constructor pivate since
  // std::make_unique() needs to be able to call the constructor, and
  // std::make_unique() is not itself a member.
  explicit AFSPMatcher(const FactoryKey&, const DFSContext&, const std::string& pattern,
		       std::string* error_message);
  ~AFSPMatcher();

 private:
  bool Valid() const 
  {
    return valid_;
  }
  
  bool valid_;
  DFSContext context_;
  void *implementation_;
};

