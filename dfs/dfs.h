#ifndef INC_DFS_H
#define INC_DFS_H 1

#include <exception>
#include <limits>
#include <map>
#include <string>

#include "dfstypes.h"

namespace DFS {
  extern bool verbose;
  unsigned long sign_extend(unsigned long address);
  unsigned long compute_crc(const byte* start, const byte *end);
  const std::map<std::string, std::string>& get_option_help();

  template <typename T> T safe_unsigned_multiply(T a, T b)
  {
    // Ensure T is unsigned.
    static_assert(std::numeric_limits<T>::is_integer
		  && !std::numeric_limits<T>::is_signed);
    if (a == 0 || b == 0)
      return T(0);
    if (std::numeric_limits<T>::max() / a < b)
      throw std::range_error("overflow in safe_unsigned_multiply");
    return T(a * b);
  }


}  // namespace DFS

#endif
