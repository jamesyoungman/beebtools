#include "exceptions.h"

namespace DFS {

BadFileSystem::BadFileSystem(const std::string& msg)
  : error_message_(std::string("bad disk image: ") + msg)
{
}

const char *
BadFileSystem::what() const throw()
{
  return error_message_.c_str();
}

OsError::OsError(int errno_value)
  : errno_value_(errno_value)
{
}

const char *
OsError::what() const throw()
{
  return strerror(errno_value_);
}

}  // namespace DFS
