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

#include <algorithm>	        // for is_sorted
#include <functional>	        // for function<>
#include <stdint.h>	        // for uint8_t
#include <stddef.h>             // for size_t
#include <cassert>              // for assert
#include <iomanip>              // for operator<<, setw
#include <iostream>             // for operator<<, basic_ostream, ostream
#include <optional>             // for optional
#include <string>               // for operator<<
#include <utility>              // for make_pair, pair

#include "crc.h"                // for CCITT_CRC16
#include "hexdump.h"

#undef ULTRA_VERBOSE

namespace
{
constexpr int normal_fm_clock = 0xFF;
constexpr int id_address_mark = 0xFE;
constexpr int data_address_mark = 0xFB;
constexpr int deleted_data_address_mark = 0xF8;

using std::optional;
using byte = unsigned char;


void self_test_crc()
{
  DFS::CCITT_CRC16 crc;
  uint8_t input[2];
  input[0] = uint8_t('T');
  input[1] = uint8_t('Q');

  crc.update(input, input+1);
  assert(crc.get() == 0xFB81);
  crc.update(input+1, input+2);
  assert(crc.get() == 0x95A0);

  DFS::CCITT_CRC16 crc2;
  crc2.update(input, input+2);
  assert(crc.get() == 0x95A0);


  const uint8_t id[7] = {
			 id_address_mark,
			 0x4F,	// cylinder
			 0,	// side
			 6,	// sector
			 1,	// size code
			 0xE1,	// CRC byte 1
			 0x07   // CRC byte 1
  };
  DFS::CCITT_CRC16 crc3;
  crc3.update(id, id+sizeof(id));
  assert(crc3.get() == 0);
}

std::optional<int> decode_sector_size(unsigned char code)
{
  switch (code)
    {
    case 0x00: return 128;
    case 0x01: return 256;
    case 0x02: return 512;
    case 0x03: return 1024;
    default: return std::nullopt;
    }
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

  std::optional<std::pair<byte, byte>> read_byte(size_t& start) const
  {
    // An FM-encoded byte occupies 16 bits on the disc, and looks like
    // this (in the order bits appear on disc):
    //
    // first       last
    // cDcDcDcDcDcDcDcD (c are clock bits, D data)
    unsigned int clock=0, data=0;
    for (int bitnum = 0; bitnum < 8; ++bitnum)
      {
	if (start + 2 >= size_)
	  return std::nullopt;

	clock = (clock << 1) | getbit(start++);
	data  = (data  << 1) | getbit(start++);
      }
    return std::make_pair(static_cast<unsigned char>(clock),
			  static_cast<unsigned char>(data));
  }

  std::optional<std::pair<size_t, unsigned int>> scan_for(size_t start, unsigned int val, unsigned int mask) const
  {
    const unsigned needle = mask & val;
    unsigned int shifter = 0, got = 0;
    for (size_t i = start; i < size_; ++i)
      {
	shifter = (shifter << 1u) | (getbit(i) ? 1u : 0u);
	got = (got << 1u) | 1u;
	if ((mask & got) == mask)
	  {
	    // We have enough bits for the comparison to be valid.
	    if ((mask & shifter) == needle)
	      return std::make_pair(i, shifter);
	  }
      }
    return std::nullopt;
  }

  bool copy_fm_bytes(size_t& thisbit, size_t n, byte* out, bool verbose) const
  {
    while (n--)
      {
	auto clock_and_data = read_byte(thisbit);
	if (clock_and_data && clock_and_data->first == normal_fm_clock)
	  {
	    *out++ = clock_and_data->second;
	    continue;
	  }
	if (verbose)
	  {
	    if (!clock_and_data)
	      {
		std::cerr << "end-of-track while reading data bytes\n";
	      }
	    else
	      {
		std::cerr << "desynced while reading data bytes\n";
	      }
	  }
	return false;
      }
    return true;
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
  const BitStream bits(raw_data);
  size_t bits_avail = bits.size();
  size_t thisbit = 0;

  auto find_record_address_mark =
    [&thisbit, &bits, bits_avail]() -> std::optional<std::pair<size_t, unsigned int>>
    {
     while (thisbit < bits_avail)
       {
	/* We're searching for one of these:
	   0xF56A: control record
	   0xF56F: data record
	   But, (0xF56A & 0xF56F) == 0xF56A, so we scan for that and check what we got.
	   0xA == binary 1010
	*/
	auto searched_from = thisbit;
	auto found = bits.scan_for(thisbit, 0xF56A, 0xFFFA);
	if (!found)
	  {
	    thisbit = bits_avail;
	    break;
	  }
	found->second &= 0xFFFF;
	if (found->second == 0xF56A || found->second == 0xF56F)
	  {
	    return std::make_optional(std::make_pair(found->first+1, found->second));
	  }
	/* We could have seen some third bit pattern, for
	   example 0xF56B.  A match exactly at the next bit
	   is impossible, but advancing by just one bit
	   keeps the code simpler and this case will be very
	   rare.
	*/
	thisbit = searched_from + 1;
       }
     return std::nullopt;
    };

  enum DecodeState { Desynced, LookingForAddress, LookingForRecord };
  enum DecodeState state = Desynced;
  Sector sec;
  int sec_size;
  while (thisbit < bits_avail)
    {
      if (state == Desynced)
	{
	  // The FM-encoded bit sequence 1010101010101010 (hex 0xAAAA)
	  // has all clock bits set to 1 and all data bits set to 0.
	  // It represents clock=0xFF, data=0x00.  This is the sync
	  // field.
	  auto found = bits.scan_for(thisbit, 0xAAAA, 0xFFFF);
	  if (!found)
	    break;
	  state = LookingForAddress;
	  thisbit = found->first + 1u;
	}
      else if (state == LookingForAddress)
	{
	  // The FM-encoded sequence bit sequence 1111010101111110 has
	  // the hex value 0xF57E.  Dividing it into 4 nibbles we can
	  // visualise the clock and data bits:
	  //
	  // Hex  binary  clock  data
	  // F    1111     11..   11..
	  // 5    0101     ..00   ..11
	  //
	  // that is, for the top nibbles we have clock 1100=0xC and
	  // data 1111=0xF.
	  //
	  // Hex  binary  clock  data
	  // 7    0111     01..   11..
	  // E    1110     ..11   ..10
	  //
	  // that is, for the bottom nibbles we have clock 0111=0x7
	  // and data 1110=0xE.
	  //
	  // Putting together the nibbles we have data 0xC7, clock
	  // 0xFE.  That is address mark 1, which introduces the
	  // sector ID.  Address marks are unusual in that the clock
	  // bits are not all set to 1.
	  auto found = bits.scan_for(thisbit, 0xF57E, 0xFFFF);
	  if (!found)
	    break;
	  thisbit = found->first + 1u;
	  if (verbose_)
	    {
#if ULTRA_VERBOSE
	      std::cerr << "Found AM1, reading sector address\n";
#endif
	    }
	  /* clock=0xC7, data=0xFE - this is the index address mark */
	  byte id[7] = {byte(id_address_mark)};
	  // Contents of the address
	  // byte 0 - mark (data, 0xFE)
	  // byte 1 - cylinder
	  // byte 2 - head (side)
	  // byte 3 - record (sector, starts from 0 in Acorn)
	  // byte 4 - size code (see switch below)
	  // byte 5 - CRC byte 1
	  // byte 6 - CRC byte 2
	  if (!bits.copy_fm_bytes(thisbit, 6u, &id[1], verbose_))
	    {
	      if (verbose_)
		{
		  std::cerr << "Failed to read sector address\n";
		}
	      state = Desynced;
	      continue;
	    }
	  DFS::CCITT_CRC16 crc;
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
	  std::optional<int> ssiz = decode_sector_size(id[4]);
	  if (!ssiz)
	    {
	      if (verbose_)
		{
		  std::cerr << "saw unexpected sector size code " << addr[3] << "\n";
		}
	      sec_size = addr[3];
	      state = Desynced;
	      continue;
	    }
	  sec_size = *ssiz;
	  // id[5] and id[6] are the CRC bytes, and these already got
	  // included in our evaluation of addr_crc.
	  state = LookingForRecord;
	}
      else if (state == LookingForRecord)
	{
	  std::optional<std::pair<size_t, unsigned int>> found = find_record_address_mark();
	  if (!found)
	    break;
	  thisbit = found->first;
	  const bool discard_record = (found->second & 0xFFFF) == 0xF56A;
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
	  byte data_mark[1] =
	    {
	     byte(discard_record ? deleted_data_address_mark : data_address_mark)
	    };
	  if (!bits.copy_fm_bytes(thisbit, size_with_crc, sec.data.data(), verbose_))
	    {
	      if (verbose_)
		{
		  std::cerr << "Lost sync in sector data\n";
		}
	      state = Desynced;
	      continue;
	    }
	  DFS::CCITT_CRC16 crc;
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


IbmMfmDecoder::IbmMfmDecoder(bool verbose)
  : verbose_(verbose)
{
}

bool read_and_check_crc(size_t nbytes,
			std::function<bool(size_t, byte *, std::string&)> reader,
			byte* out,
			std::string& error)
{
  if (!reader(nbytes, out, error))
    return false;
  DFS::CCITT_CRC16 crc;
  crc.update(out, out + nbytes);
  if (crc.get())
    {
      std::ostringstream ss;
      ss << "CRC mismatch in block of " << std::dec << nbytes
	 << " bytes: 0x" << std::hex << crc.get() << " should be 0\n";
      error = ss.str();
      return false;
    }
  return true;
}


std::vector<Sector> IbmMfmDecoder::decode(const std::vector<byte>& raw_data)
{
  self_test_crc();

  std::vector<Sector> result;
  unsigned int shifter = 0x0000;
  const BitStream bits(raw_data);
  size_t bits_avail = bits.size();
  size_t thisbit = 0;

  int bits_at_once = 1;
  enum MfmDecodeState { LookingForAddressA1, LookingForRecord };
  enum MfmDecodeState state = LookingForAddressA1;

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
  auto copy_bytes = [&bits_avail, &thisbit, &bits, this](size_t nbytes,
							 byte* out) -> bool
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
			  int clock=0, data=0, allbits=0;
			  for (int bitnum = 0; bitnum < 8; ++bitnum)
			    {
			      bool clock_bit = bits.getbit(thisbit++);
			      bool data_bit = bits.getbit(thisbit++);
			      clock = (clock << 1) | (clock_bit ? 1 : 0);
			      data  = (data  << 1) | (data_bit ? 1 : 0);
			      allbits = (allbits << 2) | ((clock_bit ? 2 : 0) | (data_bit ? 1 : 0));
			      bits_avail -= 2;
			    }
#if ULTRA_VERBOSE
			  if (verbose_)
			    {
			      std::cerr << std::hex
					<< "copy_bytes: from bits " << std::setw(4)
					<< unsigned(allbits)
					<< ", got clock="
					<< std::setw(2) << unsigned(clock)
					<< " (expected "
					<< std::setw(2) << unsigned(clock_expected)
					<< "), data=" << std::setw(2)
					<< unsigned(data)
					<< "\n";
			    }
#endif
#if 0
			  if (clock != clock_expected)
			    {
			      std::cerr << std::hex
					<< "copy_bytes: became desynchronised; got clock="
					<< std::setw(2) << unsigned(clock)
					<< ", data=" << std::setw(2)
					<< unsigned(data)
					<< " at track input byte " << (thisbit / 8u)
					<< "\n";
			      return false;
			    }
#endif
			  *out++ = static_cast<byte>(data);
			}
		      return true;
		    };

  auto reader = [copy_bytes, this](size_t len, byte *out, std::string& error)
		{
		  if (!copy_bytes(len, out))
		    {
		      error = "lost clock synchrionization";
		      return false;
		    }
		  if (verbose_)
		    {
		      std::cerr << "read " << std::dec << len << " bytes of data: ";
		      DFS::hexdump_bytes(std::cerr, len, 16, out, out+len);
		    }
		  return true;
		};

  constexpr int offset_field_width = 4;

  Sector sec;
  int sec_size;
  while (bits_avail)
    {
      for (int i = 0; i < 1; ++i)
	{
	  if (!bits_avail)
	    break;
	  const int newbit = getbit();
	  shifter = ((shifter << 1) | newbit) & 0xFFFF;

	  if (verbose_)
	    {
	      auto prevbit = thisbit-1;
	      auto n = prevbit/8;
	      auto mask = 1 << (prevbit % 8);
	      auto bit_val = (raw_data[n] & mask) ? 1 : 0;

	      if (n % 4 == 0 && mask == 1)
		{
		  auto bytes_remaining = bits_avail / 8; // underestimate, since truncation
		  DFS::hexdump_bytes(std::cerr, n, 16,
				     raw_data.data() + n,
				     raw_data.data() + n + std::min(16uL, bytes_remaining));

		}
	      std::cerr << "MFM track: " << std::hex << std::setfill('0')
			<< "newbit=" << newbit
			<< " [" << bit_val << "]"
			<< " from bit " << prevbit << " (= bit " << (prevbit % 8)
			<< " of byte at offset " << std::dec
			<< std::setw(offset_field_width)
			<< std::setfill(' ') << n
			<< ", having value " << std::hex << std::setw(2)
			<< unsigned(raw_data[n]) << ")\n";
	    }
	}
      if (!bits_avail)
	break;

      if (verbose_)
	{
	  auto [clock, data] = declock(shifter);
	  std::cerr << "MFM track: " << std::hex << std::setfill('0')
		    << "bits_at_once=" << bits_at_once
		    << ", shifter=" << std::setw(4) << shifter
		    << ", clock=" << std::setw(2) << unsigned(clock)
		    << ", data=" << std::setw(2) << unsigned(data)
		    << ", state="
		    << (state == LookingForAddressA1 ? "LookingForAddressA1" :
			(state == LookingForRecord ? "LookingForRecord" :
			 "unknown"))
		    << "\n";
	}
      if (shifter == 0xAAAA)  // clock 0xFF, data 0x00.
	{
	  if (verbose_)
	    {
	      std::cerr << "(We could switch to larger reads now)\n";
	    }
	  continue;
	}

      if (shifter != 0x4489)
	continue;

      if (verbose_)
	{
	  std::cerr << "We're now synchronised to the byte boundary.\n";
	}

      const std::string gap_name = (state == LookingForAddressA1) ?
	"gap3 (before address)" : "gap2 (before record)";
      byte gap_tail[2];
      if (!copy_bytes(2, gap_tail))
	{
	  std::cerr << "Desynced while reading the end of " << gap_name << "\n";
	  state = LookingForAddressA1;
	  continue;
	}
      if (verbose_)
	{
	  std::cerr << "We have found the end of " << gap_name << "\n";
	}

      switch (state)
	{
	case LookingForAddressA1:
	  {
	    // Contents of header
	    // byte 0 - mark (data, 0xFE)
	    // byte 1 - cylinder
	    // byte 2 - head (side)
	    // byte 3 - record (sector, starts from 0 in Acorn)
	    // byte 4 - size code (see switch below)
	    // byte 5 - CRC byte 1
	    // byte 6 - CRC byte 2
	    byte header[7];
	    std::string error;
	    if (!read_and_check_crc(7, reader, header, error))
	      {
		if (verbose_)
		  {
		    std::cerr << error << "\n";
		  }
		continue;
	      }
	    if (header[0] == 0xFE)
	      {
		if (verbose_)
		  {
		    std::cerr << "Sector ID address mark found:\n";
		    DFS::hexdump_bytes(std::cerr, 0, sizeof(header), &header[0], &header[sizeof(header)]);
		  }
	      }
	    else
	      {
		// This is not a sector ID but still, somehow, the CRC matched.
		if (verbose_)
		  {
		    std::cerr << "Address mark is " << std::hex << unsigned(header[0])
			      << " so this is not a sector ID address mark\n";
		    continue;
		  }
	      }
	    std::optional<int> ssiz = decode_sector_size(header[4]);
	    if (!ssiz)
	      {
		if (verbose_)
		  {
		    std::cerr << "saw unexpected sector size code " << header[4] << "\n";
		  }
		state = LookingForAddressA1;
		continue;
	      }
	    sec_size = *ssiz;
	    state = LookingForRecord;
	  }
	  continue;

	case LookingForRecord:
	  {
	    auto size_with_crc = sec_size + 3;  // add two bytes for the CRC, 1 for address mark
	    std::vector<byte> mark_and_data;
	    mark_and_data.resize(size_with_crc);
	    std::string error;
	    if (!read_and_check_crc(size_with_crc, reader, mark_and_data.data(), error))
	      {
		if (verbose_)
		  std::cerr << error << "\n";
		state = LookingForAddressA1;
		continue;
	      }
	    const bool discard_record = mark_and_data[0] == data_address_mark ? false : true;
	    sec.crc[0] = mark_and_data[sec_size + 1];
	    sec.crc[1] = mark_and_data[sec_size + 2];
	    if (discard_record)
	      {
		if (verbose_)
		  {
		    std::cerr << "Dropping the control record\n";
		  }
		state = LookingForAddressA1;
		continue;
	      }
	    if (verbose_)
	      {
		std::cerr << "Accepting record/sector with address "
			  << sec.address << "; " << "it has "
			  << sec.data.size() << " bytes of data.\n";
	      }
	    sec.data.resize(sec_size);
	    std::copy(mark_and_data.begin() + 1,
		      mark_and_data.begin() + 1 + sec_size,
		      sec.data.begin());
	    result.push_back(sec);
	    state = LookingForAddressA1;
	    continue;
	  }
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

namespace DFS
{
  bool check_track_is_supported(const std::vector<Sector> track_sectors,
				unsigned int track,
				unsigned int side,
				unsigned int sector_bytes,
				bool verbose,
				std::string& error)
  {
    assert(std::is_sorted(track_sectors.begin(), track_sectors.end()));

    // Validate the sectors themselves.
    std::optional<int> prev_rec_num;
    for (const Sector& sect : track_sectors)
      {
	// Many of the possible issues detected here are more likely
	// to be a bug in our code than something weird about the
	// HFE file.
	std::ostringstream ss;
	if (sect.address.head != side)
	  {
	    ss << "found sector with address " << sect.address
	       << " in the data for side " << side;
	    error = ss.str();
	    return false;
	  }
	if (sect.address.cylinder != track)
	  {
	    ss << "found sector with address " << sect.address
	       << " in the data for track " << track;
	    error = ss.str();
	    return false;
	  }
	if (prev_rec_num)
	  {
	    if (*prev_rec_num == sect.address.record)
	      {
		ss << "sector with address " << sect.address
		   << " has a duplicate record number ";
		error = ss.str();
		return false;
	      }
	    else if (*prev_rec_num + 1 < sect.address.record)
	      {
		ss << "before sector with address " << sect.address
		   << " there is no sector with record number "
		   << (*prev_rec_num + 1);
		error = ss.str();
		return false;
	      }
	  }
	else
	  {
	    // This is the first record (numerically, not in
	    // physical order).  The IBM format specification
	    // numbers records from 1, but Acorn DFS uses 0 as the
	    // lowest sector number.
	    if (sect.address.record != 0)
	      {
		if (verbose)
		  {
		    std::cerr << "warning: the lowest-numbered sector of "
			      << "track " << track << " has address "
			      << sect.address
			      << " but it should have record number 0 "
			      << "instead of "
			      << unsigned(sect.address.record) << "\n";
		  }
	      }
	  }

	if (sect.data.size() != sector_bytes)
	  {
	    ss << "track " << track
	       << " contains a sector with address " << sect.address
	       << " but it has unsupported size " << sect.data.size()
	       << " (the supported size is " << sector_bytes << ")";
	    error = ss.str();
	    return false;
	  }

	prev_rec_num = sect.address.record;
      }
    return true;
  }

}  // namespace DFS
