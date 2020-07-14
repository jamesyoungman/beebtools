#ifndef INC_EXCEPTIONS_H
#define INC_EXCEPTIONS_H

#include <exception>
#include <sstream>
#include <stdexcept>
#include <string>

#include <string.h>

namespace DFS {

class BaseException : public std::exception
{
public:
  explicit BaseException(const std::string& msg)
    : msg_(msg)
  {
  }

  const char* what() const noexcept
  {
    return msg_.c_str();
  }

private:
  std::string msg_;
};


class Unrecognized : public BaseException
{
public:
  explicit Unrecognized(const std::string& cause)
    : BaseException(make_msg(cause))
  {
  }

private:
  static std::string make_msg(const std::string& cause);
};

// OpusDDOS support is incomplete.  We throw this exception when
// encountering a case where the format makes a difference.
class OpusUnsupported : public BaseException
{
public:
  OpusUnsupported()
    : BaseException("Opus DDOS is not yet supported")
  {
  }
};

class FailedToGuessFormat : public BaseException
{
 public:
  explicit FailedToGuessFormat(const std::string& msg)
    : BaseException(msg)
  {
  }
};

class MediaNotPresent : public BaseException
{
public:
  explicit MediaNotPresent(const std::string& s)
    : BaseException(s)
  {
  }
};

class BadFileSystem : public BaseException
{
 public:
  BadFileSystem(const std::string& msg);
};

BadFileSystem eof_in_catalog();

class FileIOError : public BaseException
{
public:
  explicit FileIOError(const std::string& file_name, int errno_value);
};

class NonFileOsError : public BaseException
{
public:
  // NonFileOsError is only for operations which don't involve a
  // file. For operations involving a file, use FileIOError instead.
  explicit NonFileOsError(int errno_value);
};

}  // namespace DFS
#endif
