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
#include "crc.h"

#include <assert.h>             // for assert
#include <stdint.h>		// for uint8_t, uint16_t

namespace
{
  inline unsigned long crc_cycle(unsigned long crc)
  {
    // Our polynomial is 0x10000 + (0x810<<1) + 1 = 0x11021 This CRC
    // is known as CRC16-CCITT.
    //
    // When computing CRCs for use in reading disc tracks
    // (i.e. CCIT_CRC16), we initialise the cec_ state to 0xFFFF, as
    // required for CRC16-CCIT.
    //
    // When computing CRCs for the INF files (i.e. TapeCRC), we
    // initialise the crc_ state to 0, as for the XModem CRC.
    //
    // The high byte of the result is presented first (in both the
    // XModem and our case).
    if (crc & 32768)
      return  (((crc ^ 0x0810) & 32767) << 1) + 1;
    else
      return crc << 1;
  }
}  // namespace

namespace DFS
{
  void CRC16Base::update(const uint8_t* start, const uint8_t *end)
  {
    for (const uint8_t* p = start; p < end; )
      {
	const auto in = *p++;
	crc_ ^= in << 8;
	for(int k = 0; k < 8; k++)
	  crc_ = crc_cycle(crc_);
	assert((crc_ & ~0xFFFF) == 0);
      }
  }

  void CRC16Base::update_bit(bool bitval)
  {
    crc_ ^= (bitval ? 0x8000 : 0);
    crc_ = crc_cycle(crc_);
    assert((crc_ & ~0xFFFF) == 0);
  }

  unsigned long CRC16Base::get() const
  {
    return crc_;
  }

  CRC16Base::CRC16Base(uint16_t init)
    : crc_(init) {}

  CCIT_CRC16::CCIT_CRC16()
    : CRC16Base(CCIT_CRC16::init) {}

  TapeCRC::TapeCRC()
    : CRC16Base(TapeCRC::init) {}

}  // namespace DFS
