//
//   Copyright 2020 James Youngman
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
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

#include <cstdint>		// for uint2_t, uint32_t.
#include <stddef.h>		// for size_t

namespace DFS
{
  class CRC16Base
  {
  public:
    explicit CRC16Base(uint16_t init);
    void update(const uint8_t* start, const uint8_t *end);
    void update_bit(bool bitval);
    unsigned long get() const;
  private:
    unsigned long crc_;
  };


  class CCITT_CRC16 : public CRC16Base
  {
    static constexpr unsigned long init = 0xFFFFuL;
  public:
    CCITT_CRC16();
  };

  // TapeCRC appears to be the same as the XMODEM CRC, but the
  // authoritative reference is page 348 of the BBC Microcmputer
  // Advanced User Guide.
  class TapeCRC : public CRC16Base
  {
    static constexpr unsigned long init = 0uL;
  public:
    TapeCRC();
  };

}  // namespace DFS


#endif
