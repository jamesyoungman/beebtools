// This file contains implementations of various CRCs we use.
// Here are some references about CRCs:
//
// https://www.zlib.net/crc_v3.txt
//   - covers the basics of the idea and implementation techniques.
//
// https://barrgroup.com/embedded-systems/how-to/crc-math-theory
//   - a perhaps-gentler introduction
//
// http://reveng.sourceforge.net/crc-catalogue/
//   - a catalogue of CRC polynomials
//
// https://www.lammertbies.nl/comm/info/crc-calculation
//   - try some out, online.
//
#ifndef INC_CRC_H
#define INC_CRC_H 1

#include "dfstypes.h"

namespace DFS
{
  // TapeCRC appears to be the same as the XMODEM CRC, but the
  // authoritative reference is page 348 of the BBC Microcmputer
  // Advanced User Guide.
  class TapeCRC
  {
  public:
    TapeCRC();
    void update(const byte* start, const byte *end);
    unsigned long get() const;
  private:
    unsigned long crc_;
  };

}  // namespace DFS

#endif
