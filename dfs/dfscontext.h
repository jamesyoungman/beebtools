#if !defined(INC_DFSCONTEXT_H)
#define INC_DFSCONTEXT_H 1

#include <string>
#include <optional>

#include "driveselector.h"

namespace DFS
{

  // UiStyle means, approximately "which ROM are we trying to behave
  // like?".  This affects mostly incidental aspects of behaviour
  // (such as printing the "Work file" item in the catalog).
  //
  // Some important behaviours (such as the number of supported
  // catalog entries) do _not_ depend on a UiStyle value, they depend
  // on the type of file system in the image.
  //
  // By default, the UI style is determined by the loaded image, and
  // defaults to Acorn.
  enum class UiStyle
    {
     Default,
     // Acorn means "Acorn 1770 DFS" as opposed to the 8271 DFS, so
     // for example we show the disc encoding after the title in the
     // "cat" command.
     Acorn,
     Watford,
     Opus
     // If you add an entry to this enum, update
     // DFS::FileSystem::ui_style() and (at least) cmd_cat.cc and its
     // tests.
    };

class FileSystem;

struct DFSContext
{
public:
  DFSContext(char dir, DFS::VolumeSelector vol)
    : current_directory(dir), current_drive(vol), ui_(UiStyle::Default)
  {
  }

  DFSContext(char dir, DFS::VolumeSelector vol, UiStyle style)
    : current_directory(dir), current_drive(vol), ui_(style)
  {
  }

  DFSContext()
    : current_directory('$'), current_drive(0), ui_(UiStyle::Default)
   {
   }

  char current_directory;
  DFS::VolumeSelector current_drive; // TODO: rename?

private:
  friend class FileSystem;
  // ui is accessed via FileSystem so that it can also take into
  // account the type of image we are working with.
  UiStyle ui_;
};

}  // namespace DFS

#endif
