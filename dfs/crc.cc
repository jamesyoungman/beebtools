#include <assert.h>

#include "crc.h"

namespace
{
  inline unsigned long tapecrc_cycle(unsigned long crc)
  {
    // Our polynomial is 0x10000 + (0x810<<1) + 1 = 0x11021 This CRC
    // is known as CRC16-CCITT, but because we initialise the crc_
    // state to 0 (instead of 0xFFFF) it is probably more accurate to
    // describe this as the XMODEM CRC.
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
  TapeCRC::TapeCRC()
    : crc_(0uL) {}

  void TapeCRC::update(const DFS::byte* start, const DFS::byte *end)
  {
    for (const DFS::byte* p = start; p < end; ++p)
      {
	crc_ ^= *p++ << 8;
	for(int k = 0; k < 8; k++)
	  crc_ = tapecrc_cycle(crc_);
	assert((crc_ & ~0xFFFF) == 0);
      }
  }

  unsigned long TapeCRC::get() const
  {
    return crc_;
  }

}  // namespace DFS
