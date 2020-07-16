#include <assert.h>

#include "crc.h"

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
    // initialise the crc_ state to 0, as for the XMODEM CRC.
    //
    // In the context of tape storage in the BBC Micro, the high byte
    // of the result is presented first.
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
