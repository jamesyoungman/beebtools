#ifndef INC_CRC_H
#define INC_CRC_H 1

#include "dfstypes.h"

namespace DFS
{

  class CRC
  {
  public:
    CRC();
    void update(const byte* start, const byte *end);
    unsigned long get() const;
  private:
    unsigned long crc_;
  };

}  // namespace DFS

#endif
