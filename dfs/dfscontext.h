#if !defined(INC_DFSCONTEXT_H)
#define INC_DFSCONTEXT_H 1

#include <string>
#include <optional>

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
    };

class FileSystem;

struct DFSContext
{
public:
  DFSContext(char dir, int drive)
    : current_directory(dir), current_drive(drive), ui_(UiStyle::Default)
  {
  }

  DFSContext(char dir, int drive, UiStyle style)
    : current_directory(dir), current_drive(drive), ui_(style)
  {
  }

  DFSContext()
    : current_directory('$'), current_drive(0), ui_(UiStyle::Default)
   {
   }

  char current_directory;
  int current_drive;

private:
  friend class FileSystem;
  // ui is accessed via FileSystem so that it can also take into
  // account the type of image we are working with.
  UiStyle ui_;
};

}  // namespace DFS

#endif
