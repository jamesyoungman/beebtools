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

class OsError : public std::exception
{
  public:
    explicit OsError(int errno_value);
    const char *what() const throw();

  private:
    int errno_value_;
};

}  // namespace DFS
#endif
