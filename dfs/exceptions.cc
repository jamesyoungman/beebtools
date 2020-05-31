#include "exceptions.h"

namespace
{
  std::string make_file_errno_message(const std::string& file_name, int errno_value)
  {
    return file_name + ": " + std::string(strerror(errno_value));
  }
}


namespace DFS {

BadFileSystem::BadFileSystem(const std::string& msg)
  : error_message_(std::string("bad disk image: ") + msg)
{
}

const char *
BadFileSystem::what() const noexcept
{
  return error_message_.c_str();
}

FileIOError::FileIOError(const std::string& file_name, int errno_value)
  : msg_(make_file_errno_message(file_name, errno_value))
{
}

const char *
FileIOError::what() const noexcept
{
  return msg_.c_str();
}

NonFileOsError::NonFileOsError(int errno_value)
  : errno_value_(errno_value)
{
}

const char* NonFileOsError::what() const noexcept
{
  return strerror(errno_value_);
}

}  // namespace DFS
