#include "exceptions.h"

namespace
{
  std::string make_errno_message(int errno_value)
  {
    const char* s = strerror(errno_value);
    if (s)
      return std::string(s);
    std::ostringstream ss;
    ss << "unknown errno value " << errno_value;
    return ss.str();
  }

  std::string make_file_errno_message(const std::string& file_name, int errno_value)
  {
    return file_name + ": " + make_errno_message(errno_value);
  }
}


namespace DFS {

std::string Unrecognized::make_msg(const std::string& cause)
{
  std::ostringstream ss;
  ss <<  "file format was not recognized: " << cause;
  return ss.str();
}


BadFileSystem::BadFileSystem(const std::string& msg)
  : BaseException(std::string("bad disk image: ") + msg)
{
}

DFS::BadFileSystem eof_in_catalog()
{
  return BadFileSystem("file system image is too short to contain a catalog");
}

FileIOError::FileIOError(const std::string& file_name, int errno_value)
  : BaseException(make_file_errno_message(file_name, errno_value))
{
}

NonFileOsError::NonFileOsError(int errno_value)
  : BaseException(make_errno_message(errno_value))
{
}

}  // namespace DFS
