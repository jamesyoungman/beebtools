#ifndef INC_EXCEPTIONS_H
#define INC_EXCEPTIONS_H

#include <exception>
#include <sstream>
#include <string>

#include <string.h>

namespace DFS {

class Unrecognized : public std::exception
{
public:
  Unrecognized(const std::string& cause)
    : msg_(make_msg(cause))
  {
  }

  const char* what() const noexcept
  {
    return msg_.c_str();
  }

private:
  static std::string make_msg(const std::string& cause)
  {
    std::ostringstream ss;
    ss <<  "file format was not recognized: " << cause;
    return ss.str();
  }
  std::string msg_;
};

// OpusDDOS support is incomplete.  We throw this exception when
// encountering a case where the format makes a difference.
class OpusUnsupported : public std::exception
{
public:
  OpusUnsupported() {};
  const char *what() const noexcept override
  {
    return "Opus DDOS is not yet supported";
  }
};

class FailedToGuessFormat : public std::exception
{
 public:
  FailedToGuessFormat(const std::string& msg);
  const char *what() const noexcept override;

 private:
  std::string error_message_;
};

class BadFileSystem : public std::exception
{
 public:
 BadFileSystem(const std::string& msg);
  const char *what() const noexcept override;

 private:
  std::string error_message_;
};

BadFileSystem eof_in_catalog();

class FileIOError : public std::exception
{
public:
  explicit FileIOError(const std::string& file_name, int errno_value);
  const char *what() const noexcept override;

private:
  const std::string msg_;
};

class NonFileOsError : public std::exception
{
public:
  // NonFileOsError is only for operations which don't involve a
  // file. For operations involving a file, use FileIOError instead.
  explicit NonFileOsError(int errno_value);
  const char *what() const noexcept override;

private:
  const int errno_value_;
};

}  // namespace DFS
#endif
