#if !defined(INC_DFSCONTEXT_H)
#define INC_DFSCONTEXT_H 1

#include <string>

struct DFSContext
{
  DFSContext(char dir, int drive)
    : current_directory(dir), current_drive(drive)
  {
  }

  DFSContext()
    : current_directory('$'), current_drive(0)
   {
   }

  char current_directory;
  int current_drive;
};

#endif
