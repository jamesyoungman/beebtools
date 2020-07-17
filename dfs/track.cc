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
#include "track.h"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <functional>
#include <sstream>

#include "crc.h"
#include "hexdump.h"

#undef ULTRA_VERBOSE

namespace
{
constexpr int normal_clock = 0xFF;
constexpr int address_mark_clock = 0xC7;
constexpr int id_address_mark = 0xFE;
constexpr int data_address_mark = 0xFB;

using std::optional;
using byte = unsigned char;


void self_test_crc()
{
  DFS::CCIT_CRC16 crc;
  uint8_t input[2];
  input[0] = uint8_t('T');
  input[1] = uint8_t('Q');

  crc.update(input, input+1);
  assert(crc.get() == 0xFB81);
  crc.update(input+1, input+2);
  assert(crc.get() == 0x95A0);

  DFS::CCIT_CRC16 crc2;
  crc2.update(input, input+2);
  assert(crc.get() == 0x95A0);


  const uint8_t id[7] = {
			 0xFE,
			 0x4F,	// cylinder
			 0,	// side
			 6,	// sector
			 1,	// size code
			 0xE1,	// CRC byte 1
			 0x07   // CRC byte 1
  };
  DFS::CCIT_CRC16 crc3;
  crc3.update(id, id+sizeof(id));
  assert(crc3.get() == 0);
}


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
  self_test_crc();

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
#if ULTRA_VERBOSE
			  std::cerr << std::dec
				    << "copy_bytes: " << bits_avail
				    << " bits still left in track data; "
				    << (nbytes * 16)
				    << " bits needed to consume " << nbytes*2 << " clocked bytes "
				    << "and yield " << nbytes
				    << " bytes of actual data\n";
#endif
			}

		      if (bits_avail / 16 < nbytes)
			{
			  if (verbose_)
			    {
			      std::cerr << "copy_bytes: not enough track data left to copy "
					<< nbytes << " bytes\n";
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
#if ULTRA_VERBOSE
			  if (verbose_)
			    {
			      std::cerr << std::hex
					<< "copy_bytes: got clock="
					<< std::setw(2) << unsigned(clock)
					<< ", data=" << std::setw(2)
					<< unsigned(data)
					<< "\n";
			    }
#endif
			  if (clock != 0xFF)
			    {
			      std::cerr << std::hex
					<< "copy_bytes: became desynchronised; got clock="
					<< std::setw(2) << unsigned(clock)
					<< " at track input byte " << (thisbit / 8u)
					<< "\n";
			      return false;
			    }
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

#if ULTRA_VERBOSE
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
#endif
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
#if ULTRA_VERBOSE
	      std::cerr << "Found AM1, reading sector address\n";
#endif
	    }
	  /* clock=0xC7, data=0xFE - this is the index address mark */
	  byte id[7] = {byte(0xFE)};
	  // Contents of the address
	  // byte 0 - mark (data, 0xFE)
	  // byte 1 - cylinder
	  // byte 2 - head (side)
	  // byte 3 - record (sector, starts from 0 in Acorn)
	  // byte 4 - size code (see switch below)
	  // byte 5 - CRC byte 1
	  // byte 6 - CRC byte 2
	  if (!copy_bytes(6u, &id[1]))
	    {
	      if (verbose_)
		{
		  std::cerr << "Failed to read sector address\n";
		}
	      state = Desynced;
	      continue;
	    }
	  DFS::CCIT_CRC16 crc;
	  crc.update(id, id + sizeof(id));
	  const auto addr_crc = crc.get();
	  if (addr_crc)
	    {
	      if (verbose_)
		{
		  std::cerr << "Sector address CRC mismatch: 0x"
			    << std::hex << addr_crc << " should be 0\n";
		}
	      state = Desynced;
	      continue;
	    }

	  const byte* addr = &id[1];
	  sec.address.cylinder = id[1];
	  sec.address.head = id[2];
	  sec.address.record = id[3];
	  switch (id[4])
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
	  // id[5] and id[6] are the CRC bytes, and these already got
	  // included in our evaluation of addr_crc.
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
	    }
	  else if (shifter == 0xF56F)
	    {
	      discard_record = false;  // AM2 for a data record (data = 0xFB).
	    }
	  else
	    {
	      continue;
	    }
	  if (verbose_)
	    {
	      std::cerr << "This record has address " << sec.address
			<< " and should contain "
			<< std::dec << sec_size << " bytes.  It is a "
			<< (discard_record ? "control" : "data")
			<< " record so we will "
			<< (discard_record ? "discard" : "keep")
			<< " it.\n";
	    }
	  // Read the sector itself.
	  auto size_with_crc = sec_size + 2;  // add two bytes for the CRC
	  sec.data.resize(size_with_crc);
	  byte data_mark[1] = { byte(discard_record ? 0xF8 : 0xFB) };
	  if (!copy_bytes(size_with_crc, sec.data.data()))
	    {
	      if (verbose_)
		{
		  std::cerr << "Lost sync in sector data\n";
		}
	      state = Desynced;
	      continue;
	    }
	  DFS::CCIT_CRC16 crc;
	  crc.update(data_mark, data_mark+1);
	  crc.update(sec.data.data(), sec.data.data() + size_with_crc);
	  // If we already know the record is a control record
	  // (deleted / faulty) then we might expect the CRC to be
	  // incorrect (for example, because this part of the disc
	  // doesn't provide reliable reads).
	  auto data_crc = crc.get();
	  if (data_crc != 0 && !discard_record)
	    {
	      if (verbose_)
		{
		  std::cerr << "Sector data CRC mismatch: 0x"
			    << std::hex << data_crc << " should be 0; "
			    << "dropping the sector\n";
		}
	      state = Desynced;
	      continue;
	    }
	  sec.crc[0] = sec.data[sec_size + 0];
	  sec.crc[1] = sec.data[sec_size + 1];
	  // Resize the sector data downward to drop the CRC.
	  sec.data.resize(sec_size);

	  if (!discard_record)
	    {
	      if (verbose_)
		{
		  std::cerr << "Accepting record/sector with address "
			    << sec.address << "; " << "it has "
			    << sec.data.size() << " bytes of data.\n";
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
