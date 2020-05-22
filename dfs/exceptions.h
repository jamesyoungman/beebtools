#ifndef INC_EXCEPTIONS_H
#define INC_EXCEPTIONS_H

#include <exception>
#include <string>
#include <string.h>

namespace DFS {

class BadFileSystem : public std::exception
{
 public:
 BadFileSystem(const std::string& msg);
  const char *what() const throw();

 private:
  std::string error_message_;
};

class FileIOError : public std::exception
{
public:
  explicit FileIOError(const std::string& file_name, int errno_value);
    const char *what() const throw();

private:
  const std::string msg_;
};

class NonFileOsError : public std::exception
{
public:
  // NonFileOsError is only for operations which don't involve a
  // file. For operations involving a file, use FileIOError instead.
  explicit NonFileOsError(int errno_value);
  const char *what() const throw();

private:
  const int errno_value_;
};

}  // namespace DFS
#endif
