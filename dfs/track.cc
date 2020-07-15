#include "track.h"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <functional>
#include <sstream>

namespace
{
constexpr int normal_clock = 0xFF;
constexpr int address_mark_clock = 0xC7;
constexpr int id_address_mark = 0xFE;
constexpr int data_address_mark = 0xFB;

using std::optional;
using byte = unsigned char;

class BitStream
{
public:
  BitStream(const std::vector<byte>& data)
    : input_(data), size_(data.size() * 8)
  {
  }

  bool getbit(size_t bitpos) const
  {
    auto i = bitpos / 8;
    auto b = bitpos % 8;
    return input_[i] & (1 << b);
  }

  size_t size() const
  {
    return size_;
  }

private:
  const std::vector<byte>& input_;
  const size_t size_;
};



}  // namespace


bool SectorAddress::operator<(const SectorAddress& a) const
{
  if (cylinder < a.cylinder)
    return true;
  if (cylinder > a.cylinder)
    return false;
  if (head < a.head)
    return true;
  if (head > a.head)
    return false;
  if (record < a.record)
    return true;
  if (record > a.record)
    return false;
  return false;			// equal.
}

bool SectorAddress::operator==(const SectorAddress& a) const
{
 if (*this < a)
   return false;
 if (a < *this)
   return false;
 return true;
}

bool SectorAddress::operator!=(const SectorAddress& a) const
{
 if (*this == a)
   return false;
 return true;
}

IbmFmDecoder::IbmFmDecoder(bool verbose)
  : verbose_(verbose)
{
}


std::pair<unsigned char, unsigned char> declock(unsigned int word)
{
  int clock = 0, data = 0;
  int bitnum = 0;
  while (bitnum < 16)
    {
      int clockbit = word & 0x8000 ? 1 : 0;
      clock = (clock << 1) | clockbit;
      word  = (word << 1) & 0xFFFF;
      ++bitnum;
      int databit = word & 0x8000 ? 1 : 0;
      data = (data << 1) | databit;
      word = (word << 1) & 0xFFFF;

      ++bitnum;
    }
  return std::make_pair(static_cast<unsigned char>(clock),
			static_cast<unsigned char>(data));
}

// Decode a train of FM clock/data bits into a sequence of zero or more
// sectors.
std::vector<Sector> IbmFmDecoder::decode(const std::vector<byte>& raw_data)
{
  std::vector<Sector> result;
  // The initial value of shifter has no particular significance
  // except for the fact that we won't mistake it for part of the sync
  // sequence (which is all just clock bits, all the data bits are
  // zero) or an address mark.
  unsigned int shifter = 0x0000;
  const BitStream bits(raw_data);
  size_t bits_avail = bits.size();
  size_t thisbit = 0;

  auto getbit = [&bits_avail, &thisbit, &bits]()
		{
		  const int result = bits.getbit(thisbit) ? 1 : 0;
		  --bits_avail;
		  ++thisbit;
		  return result;
		};
  // copy_bytes must only be called when the next bit to be read (that
  // is, the one at position i) is the clock bit which begins a new
  // byte.
  auto copy_bytes = [&bits_avail, &thisbit, &bits, this](size_t nbytes, byte* out) -> bool
		    {
		      if (verbose_)
			{
			  std::cerr << std::dec
				    << "copy_bytes: " << bits_avail << " bits still left in track data\n"
				    << "copy_bytes: " << (nbytes * 16) << " bits needed to consume " << nbytes << " bytes\n";
			}
		      if (bits_avail / 16 < nbytes)
			{
			  if (verbose_)
			    {
			      std::cerr << "copy_bytes: not enough track data left to copy " << nbytes
					<< " bytes\n";
			    }
			  return false;
			}
		      while (nbytes--)
			{
			  // An FM-encoded byte occupies 16 bits on
			  // the disc, and looks like this (in the
			  // order bits appear on disc):
			  //
			  // first       last
			  // cDcDcDcDcDcDcDcD (c are clock bits, D data)
			  //
			  // The FM-encoded bit sequence
			  // 1010101010101010 (hex 0xAAAA) has all
			  // clock bits set to 1 and all data bits set
			  // to 0.  It represents clock=0xFF,
			  // data=0x00.
			  //
			  // The FM-encoded sequence bit sequence
			  // 1111010101111110 has the hex value
			  // 0xF57E.  Dividing it into 4 nibbles we
			  // can visualise the clock and data bits:
			  //
			  // Hex  binary  clock  data
			  // F    1111     11..   11..
			  // 5    0101     ..00   ..11
			  //
			  // that is, for the top nibbles we have
			  // clock 1100=0xC and data 1111=0xF.
			  //
			  // Hex  binary  clock  data
			  // 7    0111     01..   11..
			  // E    1110     ..11   ..10
			  //
			  // that is, for the bottom nibbles we have
			  // clock 0111=0x7 and data 1110=0xE.
			  //
			  // Putting together the nibbles we have data
			  // 0xC7, clock 0xFE.  That happens to be
			  // address mark 1, which introduces the
			  // sector ID (address marks are unusual in
			  // that the clock bits are not all-1).
			  int clock=0, data=0;
			  for (int bitnum = 0; bitnum < 8; ++bitnum)
			    {
			      clock = (clock << 1) | bits.getbit(thisbit++);
			      data  = (data  << 1) | bits.getbit(thisbit++);
			      bits_avail -= 2;
			    }
			  if (verbose_)
			    {
			      std::cerr << std::hex
					<< "copy_bytes: got clock=" << std::setw(2) << unsigned(clock)
					<< ", data=" << std::setw(2) << unsigned(data) << "\n";
			    }
			  if (clock != 0xFF)
			    return false;
			  *out++ = static_cast<byte>(data);
			}
		      return true;
		    };

  enum DecodeState { Desynced, LookingForAddress, LookingForRecord };
  enum DecodeState state = Desynced;
  Sector sec;
  int sec_size;
  size_t countdown = 0u;
  while (bits_avail)
    {
      const int newbit = getbit();
      assert(newbit == 0 || newbit == 1);
      shifter = ((shifter << 1) | newbit) & 0xFFFF;
      if (countdown)
	--countdown;

      if (verbose_)
	{
	  auto [clock, data] = declock(shifter);
	  auto prevbit = thisbit-1;
	  auto n = prevbit/8;
	  auto mask = 1 << (prevbit % 8);
	  auto bit_val = (raw_data[n] & mask) ? 1 : 0;
	  std::cerr << "FM track: " << std::hex << std::setfill('0')
		    << "newbit=" << newbit
		    << " [" << bit_val << "]"
		    << " (from bit " << (prevbit % 8) << " at "
		    << "offset " << std::dec << n << ", byte value " << std::hex << unsigned(raw_data[n]) << ")"
		    << ", shifter=" << std::setw(4) << shifter
		    << ", clock=" << std::setw(2) << unsigned(clock)
		    << ", data=" << std::setw(2) << unsigned(data)
		    << ", countdown=" << countdown
		    << ", state="
		    << (state == Desynced ? "Desynced" :
			(state == LookingForAddress ? "LookingForAddress" :
			 (state == LookingForRecord ? "LookingForRecord" :
			  "unknown")))
		    << "\n";
	}
      if (state == Desynced)
	{
	  // Gap 1 is lots of 0xFF data (with 0xFF clocks too)
	  // followed by the sync field, which is six bytes
	  // (i.e. 48 bits, with clocks) of zero data.
	  if (shifter == 0xAAAA)
	    {
	      // We found the sync field.
	      state = LookingForAddress;
	      // There should be four more bytes of sync.
	      countdown = bits_avail;
	    }
	}
      else if (state == LookingForAddress)
	{
	  if (shifter != 0xF57E)
	    {
	      // There is a limit to how long we can reasonably stay
	      // in the "LookingForAddress" state based on the maximum
	      // length of gap 1.  But we don't implement such a
	      // limit.
	      continue;
	    }
	  if (verbose_)
	    {
	      std::cerr << "Found AM1, reading sector address\n";
	    }
	  /* clock=0xC7, data=0xFE - this is the index address mark */
	  byte addr[6];
	  if (!copy_bytes(sizeof(addr), addr))
	    {
	      if (verbose_)
		{
		  std::cerr << "Failed to read sector address\n";
		}
	      state = Desynced;
	      continue;
	    }
	  sec.address.cylinder = addr[0];
	  sec.address.head = addr[1];
	  sec.address.record = addr[2];
	  switch (addr[3])
	    {
	    case 0x00: sec_size = 128; break;
	    case 0x01: sec_size = 256; break;
	    case 0x02: sec_size = 512; break;
	    case 0x03: sec_size = 1024; break;
	    default:
	      if (verbose_)
		{
		  std::cerr << "saw unexpected sector size code " << addr[3] << "\n";
		}
	      sec_size = addr[3];
	      state = Desynced;
	      continue;
	    }
	  if (verbose_)
	    {
	      std::cerr << "The address field is " << sec.address
			<< " and the data record should contain "
			<< std::dec << sec_size << " bytes\n";
	    }
	  // TODO: verify the CRC.
	  state = LookingForRecord;
	}
      else if (state == LookingForRecord)
	{
	  bool discard_record;
	  if (shifter == 0xF56A)
	    {
	      // A deleted (initial byte of record is 'D' or defective
	      // (initial byte 'F') non-data record.
	      discard_record = true;  // control record (data = 0xF8).
	      if (verbose_)
		{
		  std::cerr << "This is a control record, we will discard it.\n";
		}
	    }
	  else if (shifter == 0xF56F)
	    {
	      discard_record = false;  // AM2 for a data record (data = 0xFB).
	      if (verbose_)
		{
		  std::cerr << "This is a data record, we will use it.\n";
		}
	    }
	  else
	    {
	      continue;
	    }
	  // Read the sector itself.
	  if (verbose_)
	    {
	      std::cerr << "Accepting " << std::dec << sec_size << " bytes of sector data\n";
	    }
	  sec.data.resize(sec_size);
	  if (!copy_bytes(sec_size, sec.data.data()))
	    {
	      if (verbose_)
		{
		  std::cerr << "Lost sync in sector data\n";
		}
	      state = Desynced;
	      continue;
	    }
	  if (verbose_)
	    {
	      std::cerr << "Got the sector data\n";
	    }
	  byte data_crc[2];
	  if (!copy_bytes(sizeof(data_crc), data_crc))
	    {
	      if (verbose_)
		{
		  std::cerr << "Lost sync in CRC\n";
		}
	      state = Desynced;
	      continue;
	    }
	  if (verbose_)
	    {
	      std::cerr << "Got the CRC\n";
	    }
	  // TODO: verify the data CRC.
	  if (!discard_record)
	    {
	      if (verbose_)
		{
		  std::cerr << "Accepting the record/sector; "
			    << "it has " << sec.data.size()
			    << " bytes of data.\n";
		}
	      result.push_back(sec);
	    }
	  else
	    {
	      std::cerr << "Dropping the control record\n";
	    }
	  state = Desynced;
	}
    }
  return result;
}

namespace std
{
  ostream& operator<<(ostream& os, const SectorAddress& addr)
  {
    ostream::sentry s(os);
    if (s)
      {
	ostringstream ss;
	ss << '('
	   << unsigned(addr.cylinder) << ','
	   << unsigned(addr.head) << ','
	   << unsigned(addr.record)
	   << ')';
	os << ss.str();
      }
    return os;
  }

}  // namespace std
