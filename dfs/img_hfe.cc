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
/*
  HFE file format support
  Based on Rev.1.1 - 06/20/2012 of the format specification.
  Documentation: https://hxc2001.com/download/floppy_drive_emulator/SDCard_HxC_Floppy_Emulator_HFE_file_format.pdf

  This code isn't really useful as a general HFE implementation, for
  the following reasons:
  1. Only some track encodings are supported.
  2. No support for images where tracks don't all have the same number of sectors.
  3. The RAND opcode is supported in a way that copy protection schemes won't like.
  4. Limited testing of double-sided image files.
  5. No support for HFEv2 (though this is unlikely to be an issue in practice).
*/
#include <assert.h>          // for assert
#include <exception>	     // for std::exception
#include <errno.h>           // for EIO
#include <string.h>          // for memcmp
#include <algorithm>         // for copy, min, sort, transform
#include <array>             // for array<>::iterator
#include <iomanip>           // for setfill, etc.
#include <iostream>          // for operator<<, basic_ostream, ostringstream
#include <iterator>          // for back_insert_iterator, back_inserter, adv...
#include <limits>            // for numeric_limits
#include <memory>            // for unique_ptr, make_unique, allocator_trait...
#include <optional>          // for optional, nullopt
#include <sstream>           // for ostringstream
#include <string>            // for char_traits, string, operator<<, allocator
#include <utility>           // for move
#include <vector>            // for vector, vector<>::iterator, ...

#include "abstractio.h"      // for SectorBuffer, FileAccess, SECTOR_BYTES
#include "dfs.h"             // for verbose
#include "dfs_format.h"      // for Format
#include "exceptions.h"      // for FileIOError
#include "geometry.h"        // for Geometry, Encoding, Encoding::FM, ...
#include "hexdump.h"         // for hexdump_bytes
#include "identify.h"        // for identify_file_system
#include "media.h"           // for AbstractImageFile, make_hfe_file
#include "storage.h"         // for DriveConfig, DriveAllocation, AbstractDrive
#include "track.h"           // for Sector, SectorAddress, IbmFmDecoder, ...

#undef ULTRA_VERBOSE
//#define ULTRA_VERBOSE 1

namespace
{
using Track::Sector;
using Track::decode_fm_track;
using Track::decode_mfm_track;
using Track::SectorAddress;
using Track::byte;

class InvalidHfeFile : public std::exception
{
public:
  InvalidHfeFile(const std::string& msg)
    : msg_(msg)
  {
  }

  const char *what() const noexcept override
  {
    return msg_.c_str();
  }

private:
  std::string msg_;
};

class UnsupportedHfeFile : public std::exception
{
public:
  UnsupportedHfeFile(const std::string& msg)
    : msg_(msg)
  {
  }

  const char *what() const noexcept override
  {
    return msg_.c_str();
  }

private:
  std::string msg_;
};

#define IBMPC_DD_FLOPPYMODE                  0x00
#define IBMPC_HD_FLOPPYMODE		     0x01
#define ATARIST_DD_FLOPPYMODE		     0x02
#define ATARIST_HD_FLOPPYMODE		     0x03
#define AMIGA_DD_FLOPPYMODE		     0x04
#define AMIGA_HD_FLOPPYMODE		     0x05
#define CPC_DD_FLOPPYMODE		     0x06
#define GENERIC_SHUGGART_DD_FLOPPYMODE	     0x07
#define IBMPC_ED_FLOPPYMODE		     0x08
#define MSX2_DD_FLOPPYMODE		     0x09
#define C64_DD_FLOPPYMODE		     0x0A
#define EMU_SHUGART_FLOPPYMODE		     0x0B
#define S950_DD_FLOPPYMODE		     0x0C
#define S950_HD_FLOPPYMODE		     0x0D
#define DISABLE_FLOPPYMODE		     0xFE

#define ISOIBM_MFM_ENCODING                  0x00
#define AMIGA_MFM_ENCODING		     0x01
#define ISOIBM_FM_ENCODING		     0x02
#define EMU_FM_ENCODING			     0x03
#define UNKNOWN_ENCODING		     0xFF

#define OPCODE_MASK       0xF0

#define NOP_OPCODE        0xF0
#define SETINDEX_OPCODE   0xF1
#define SETBITRATE_OPCODE 0xF2
#define SKIPBITS_OPCODE   0xF3
#define RAND_OPCODE       0xF4


struct picfileformatheader
{
  unsigned char HEADERSIGNATURE[8];
  unsigned char formatrevision;
  unsigned char number_of_track;
  unsigned char number_of_side;
  unsigned char track_encoding;
  unsigned short bitRate;
  unsigned short floppyRPM;
  unsigned char floppyinterfacemode;
  unsigned char v1_dnu_v3_write_protected; // in v1, unused, in v3, write_protected
  unsigned short track_list_offset;
  unsigned char write_allowed;
  unsigned char single_step;
  unsigned char track0s0_altencoding;
  unsigned char track0s0_encoding;
  unsigned char track0s1_altencoding;
  unsigned char track0s1_encoding;
};

unsigned short le_word(std::vector<byte>::const_iterator d)
{
  return static_cast<unsigned short>(d[0] | (d[1] << 8u));
}

unsigned short le_word(const byte *d)
{
  return static_cast<unsigned short>(d[0] | (d[1] << 8u));
}

  picfileformatheader decode_header(const std::vector<byte>& header)
{
  std::vector<byte>::const_iterator d = header.begin();
  auto nextbyte = [&d]() -> unsigned char
		  {
		    return *d++;
		  };
  auto nextshort = [&d]() -> unsigned short
		   {
		     auto val = le_word(d);
		     d += 2;
		     return val;
		   };

  picfileformatheader h;
  std::copy(d, d+sizeof(h.HEADERSIGNATURE), h.HEADERSIGNATURE);
  std::advance(d, sizeof(h.HEADERSIGNATURE));
  assert(sizeof(h.HEADERSIGNATURE) == 8);
  /* 0x00 ... 0x07 is HEADERSIGNATURE, above. */
  /* 0x08 */ h.formatrevision = nextbyte();
  /* 0x09 */ h.number_of_track = nextbyte();
  /* 0x0A */ h.number_of_side = nextbyte();
  /* 0x0B */ h.track_encoding = nextbyte();
  /* 0x0C */ h.bitRate = nextshort();
  /* 0x0D - second byte of bitRate */
  /* 0x0E */ h.floppyRPM = nextshort();
  /* 0x0F - second byte of floppyRPM */
  /* 0x10  */ h.floppyinterfacemode = nextbyte();
  /* 0x11  */ h.v1_dnu_v3_write_protected = nextbyte();
  /* 0x12  */ h.track_list_offset = nextshort();
  /* 0x13 - second byte of track_list_offset */
  /* 0x14 */ h.write_allowed = nextbyte();
  /* 0x15 */ h.single_step = nextbyte();
  /* 0x16 */ h.track0s0_altencoding = nextbyte();
  /* 0x17 */ h.track0s0_encoding = nextbyte();
  /* 0x18 */ h.track0s1_altencoding = nextbyte();
  /* 0x19 */ h.track0s1_encoding = nextbyte();
  return h;
}

const char* step_mode(unsigned char val)
{
  switch (val)
    {
    case 0xFF:
      return "single step";
    case 0x00:
      return "double step";
    default:
      return "unknown step mode";
    }
}

const char *encoding_name(unsigned char val)
{
  switch (val)
    {
    case ISOIBM_MFM_ENCODING:
      return "ISO/IBM MFM";
    case AMIGA_MFM_ENCODING:
      return "Amiga MFM";
    case ISOIBM_FM_ENCODING:
      return "ISO/IBM FM";
    case EMU_FM_ENCODING:
      return "EMU FM";
    case UNKNOWN_ENCODING:
    default:
      return "unknown";
    }
}

const char * alt_encoding(unsigned char val)
{
  return val == 0x00 ? "yes" : "no";
}

std::ostream& operator<<(std::ostream& os, const picfileformatheader& h)
{
  std::ostream::sentry s(os);
  if (s)
    {
      os << "HEADERSIGNATURE         " << h.HEADERSIGNATURE << "\n";
      os << "formatrevision          " << static_cast<unsigned int>(h.formatrevision) << "\n";
      os << "number_of_track         " << static_cast<unsigned int>(h.number_of_track) << "\n";
      os << "number_of_side          " << static_cast<unsigned int>(h.number_of_side) << "\n";
      os << "track_encoding          " << static_cast<unsigned int>(h.track_encoding)
	 << " = " << encoding_name(h.track_encoding) << "\n";
      os << "bitRate                 " << static_cast<unsigned int>(h.bitRate) << "\n";
      os << "floppyRPM               " << static_cast<unsigned int>(h.floppyRPM) << "kbit/s\n";
      os << "floppyinterfacemode     " << static_cast<unsigned int>(h.floppyinterfacemode) << "\n";
      os << "dnu/write-protected     " << static_cast<unsigned int>(h.v1_dnu_v3_write_protected) << "\n";
      os << "track_list_offset       " << h.track_list_offset
	 << " = " << (h.track_list_offset * 512) << " bytes\n";
      os << "write_allowed           " << std::boolalpha << bool(h.write_allowed) << "\n";
      os << "single_step             " << step_mode(h.single_step) << "\n";
      os << "track0s0_altencoding    " << alt_encoding(h.track0s0_altencoding) << "\n";
      os << "track0s0_encoding       "
	 << static_cast<unsigned int>(h.track0s0_encoding)
	 << " => " << encoding_name(h.track0s0_altencoding == 0 ?
				    h.track0s0_encoding : h.track_encoding) << "\n";
      os << "track0s1_altencoding    " << alt_encoding(h.track0s1_altencoding) << "\n";
      os << "track0s1_encoding       "
	 << static_cast<unsigned int>(h.track0s1_encoding)
	 << " => " << encoding_name(h.track0s1_altencoding == 0 ?
				    h.track0s1_encoding : h.track_encoding) << "\n";
    }
  return os;
}

struct PicTrack
{
public:
  PicTrack(const unsigned char* p)
    : offset_(le_word(p)),
      track_len_(le_word(p+2))
  {
  }

  unsigned long track_len() const
  {
    if (track_len_ & 0x1FF)
      return (track_len_ & (~0x1FFu))  + 0x200u;
    else
      return track_len_;
  }

  unsigned int offset() const
  {
    return offset_;
  }

  unsigned short offset_;
  unsigned short track_len_;
};

std::vector<PicTrack>
read_track_offset_lut(DFS::FileAccess* f, unsigned int tracks)
{
  std::vector<PicTrack> result;
  std::vector<unsigned char> buf = f->read(512, tracks * 4u);
  if (buf.size() != tracks * 4u)
    {
      std::ostringstream ss;
      ss << "file is too short to contain a LUT for "
	 << tracks << " tracks, but that's the number of tracks "
	 << "indicated in the HFE file header";
      throw InvalidHfeFile(ss.str());
    }
  const unsigned char *pos = buf.data();
  for (unsigned  i = 0; i < tracks; ++i)
    {
      result.push_back(PicTrack(pos));
      pos += 4u;
    }
  return result;
}


class HfeFile : public DFS::AbstractImageFile
{
public:
  explicit HfeFile(const std::string& name, bool compressed, std::unique_ptr<DFS::FileAccess>&& file);

  class DataAccessAdapter : public DFS::AbstractDrive
  {
  public:
    DataAccessAdapter(HfeFile* f,
		      DFS::Geometry geom,
		      unsigned int side,
		      const std::vector<Sector> sectors)
      : f_(f),
	geom_(geom),
	side_(side),
	sectors_(sectors)
    {
    }

    std::optional<DFS::SectorBuffer> read_block(unsigned long lba) override
    {
      if (lba >= sectors_.size())
	return std::nullopt;
      SectorAddress addr;
      const auto sectors_per_side = geom_.cylinders * geom_.sectors;
      addr.head = lba / sectors_per_side;
      lba = lba % sectors_per_side;
      addr.cylinder = lba / geom_.sectors;
      addr.record = lba % geom_.sectors;
      std::vector<Sector>::const_iterator it = find_sector(addr);
      if (it != sectors_.cend())
	{
	  DFS::SectorBuffer buf;
	  std::copy(it->data.begin(), it->data.end(), buf.begin());
	  return buf;
	}
      else
	{
	  return std::nullopt;
	}
    }

    std::string description() const override
    {
      std::ostringstream ss;
      ss << "side " << side_ << " of " << f_->description();
      return ss.str();
    }

    DFS::Geometry geometry() const override
    {
      return geom_;
    }

    std::vector<Sector>::const_iterator find_sector(const SectorAddress& want) const
    {
      std::vector<Sector>::const_iterator it = sectors_.cbegin();
      while (it != sectors_.cend())
	{
	  if (it->address == want)
	    {
	      break;
	    }
	  ++it;
	}
      return it;
    }

  private:
    HfeFile *f_;
    DFS::Geometry geom_;  // geom_ has just one side.
    unsigned int side_;
    std::vector<Sector> sectors_;
  };

  std::string description() const;
  bool connect_drives(DFS::StorageConfiguration* storage, DFS::DriveAllocation how,
		      std::string& error) override;

private:
  unsigned char encoding_of_track(int side, int track) const;
  SectorAddress lba_to_address(unsigned long lba);
  int encoding_of_track(int track) const;
  std::vector<Sector> read_all_sectors(const std::vector<PicTrack>& lut,
				       unsigned int side);
  std::string name_;
  std::unique_ptr<DFS::FileAccess> file_;
  const bool compressed_;
  int hfe_version_;
  picfileformatheader header_;
  DFS::Geometry geom_;
  std::vector<DataAccessAdapter> acc_;
};

HfeFile::HfeFile(const std::string& name, bool compressed, std::unique_ptr<DFS::FileAccess>&& file)
  : name_(name),
    file_(std::move(file)),
    compressed_(compressed),
    hfe_version_(0)
{
  try
    {
      std::vector<byte> header_data = file_->read(0, 512);
      if (header_data.size() < 512)
	{
	  throw InvalidHfeFile("file is too short to contain the HFE file header");
	}
      header_ = decode_header(header_data);

      if (DFS::verbose)
	{
	  std::cerr << name << ":\n" << header_ << "\n";
	}
      if (0 == memcmp(header_.HEADERSIGNATURE, "HXCPICFE", 8))
	{
	  hfe_version_ = 1;
	}
      else if (0 == memcmp(header_.HEADERSIGNATURE, "HXCHFEV3", 8))
	{
	  hfe_version_ = 3;
	}
      else
	{
	  std::ostringstream ss;
	  ss << "invalid header signature: ";
	  DFS::hexdump_bytes(ss,
			     0,
			     sizeof(header_.HEADERSIGNATURE),
			     static_cast<const unsigned char*>(&header_.HEADERSIGNATURE[0]),
			     static_cast<const unsigned char*>(&header_.HEADERSIGNATURE[sizeof(header_.HEADERSIGNATURE)]));

	  throw InvalidHfeFile(ss.str());
	}

      std::vector<PicTrack> track_lut = read_track_offset_lut(file_.get(), header_.number_of_track);

      for (unsigned int side = 0; side < header_.number_of_side; ++side)
	{
	  std::vector<Sector> sectors = read_all_sectors(track_lut, side);
	  DFS::Geometry geom = geom_;
	  geom.heads = 1;
	  acc_.emplace_back(this, geom, side, sectors);
	}
    }
  catch (std::ifstream::failure& e)
    {
      // TODO: I don't think we can get a real errno value here.
      throw DFS::FileIOError(name, EIO);
    }
}

unsigned char HfeFile::encoding_of_track(int side, int track) const
{
  if (track == 0)
    {
      if (side == 0)
	{
	  if (header_.track0s0_altencoding == 0)
	    return header_.track0s0_encoding;
	}
      else
	{
	  if (header_.track0s1_altencoding == 0)
	    return header_.track0s1_encoding;
	}
    }
  return header_.track_encoding;
}

std::string HfeFile::description() const
{
  std::ostringstream ss;
  if (compressed_)
    ss << "compressed ";
  ss << "HFE file " << name_;
  return ss.str();
}

bool is_hfe3_opcode(byte val)
{
  return (val & OPCODE_MASK) == OPCODE_MASK;
}

std::string opcode_name(byte op)
{
  switch (op)
    {
    case NOP_OPCODE: return "nop";
    case SETINDEX_OPCODE: return "setindex";
    case SETBITRATE_OPCODE: return "setbitrate";
    case SKIPBITS_OPCODE: return "skipbits";
    case RAND_OPCODE: return "rand";
    default: return "(unknown opcode)";
    }
}

void premature_stream_end(DFS::byte opcode)
{
  std::cerr << "warning: track data stream ends in the middle of an HFEv3 0x"
	    << std::hex << static_cast<unsigned>(opcode)
	    << " (" << opcode_name(opcode)
	    << ") instruction\n";
}

void copy_hfe(bool hfe3, const byte* begin, const byte* end,
	      std::back_insert_iterator<std::vector<byte>> dest)
{
  int got_bits = 0;
  byte out = 0;
  byte this_op = 0;
  while (begin != end)
    {
      int skipbits = 0;
      byte in = *begin++;
      if (this_op)
	{
	  /* this byte the operant to an HFEv3 opcode. */
	  switch (this_op)
	    {
	    case NOP_OPCODE:
	    case SETINDEX_OPCODE:
	      std::cerr << "HFEv3: unexpected this_op value "
			<< std::hex << std::setw(2) << std::setfill('0')
			<< static_cast<unsigned int>(this_op) << "\n";
	      this_op = 0;
	      continue;

	    case SETBITRATE_OPCODE:
	      // We only care about the sector contents, so ignore the
	      // change in bit rate.
	      this_op = 0;
	      if (DFS::verbose)
		{
		  std::cerr << "HFEv3: setbitrate: ignoring value 0x"
			    << std::setfill('0') << std::hex << skipbits << "\n";
		}
	      continue;

	    case SKIPBITS_OPCODE:
	      {
		skipbits = in;
		this_op = 0;
		if (DFS::verbose)
		  {
		  std::cerr << "HFEv3: skipbits: " << skipbits << " bits to skip\n";
		  }
		if (in >= 8)
		  {
		    std::cerr << "HFEv3: unexpected SKIPBITS argument " << in << "\n";
		    continue;
		  }
	      }
	      break;

	    case RAND_OPCODE:
	      /* The purpose of RAND_OPCODE is, I think, so that the
	       * data read from the disk changes each time the data is
	       * read, as if we were trying to read a weak bits area
	       * from the floppy.
	       *
	       * Weak bits will not matter to the data-processing
	       * layer if the affected part of the track is not within
	       * a sector it's going to try to read.  Therefore it
	       * would be inappropriate to unconditionally fail here;
	       * that would mean that we'd fail on the whole track.
	       *
	       * In all circumstances we're going to read the track
	       * data for each track exactly once, so returning data
	       * from rand() will still result in a "consistent" read,
	       * though perhaps with incorrect CRC.
	       *
	       * In an attempt to achieve a similar effect we simply
	       * return zero data, so that the clock bits are missing
	       * and we lose sync.  A general HFE3 implementation
	       * would not implement things this way as it would not
	       * be convincing to code implementing a copy-protection
	       * scheme which itself reads the track data directly.
	       * We on the other hand have the luxury of knowing our
	       * caller isn't trying to do that, as we know a-prori
	       * that the high-level code is not part of a
	       * copy-protection scheme.
	       */
	      in = 0;		// has no clock bits, see above.
	      this_op = 0;
	      break;

	    default:
	      {
		std::ostringstream ss;
		ss << "track contains an invalid HFE3 opcode 0x"
		   << std::hex << unsigned(in);
		throw InvalidHfeFile(ss.str());
	      }
	    }
	  this_op = 0;
	}
      else if (hfe3 && is_hfe3_opcode(in))
	{
	  if (DFS::verbose)
	    {
	      std::cerr << "HFEv3: processing opcode "
			<< std::hex << unsigned(in) << " ("
			<< opcode_name(in) << ")\n";
	    }
	  switch (in)
	    {
	    case NOP_OPCODE:
	      this_op = 0;  /* takes no argument, so nothing more to do. */
	      continue;

	    case SKIPBITS_OPCODE:
	      this_op = SKIPBITS_OPCODE;
	      continue;

	    case SETINDEX_OPCODE:
	      /* For now, we ignore this (i.e. we consume the opcode
		 but do nothing about it).

		 It's not clear how we would need to use it.  In a
		 physical floppy, detection of the index mark tells us
		 we've seen the whole track.  That allows us for
		 example to know when to give up searching for a
		 sector.  But we have a finite amount of input data
		 anyway, so we won't loop forever even if we don't
		 know where in the bitsteam the index mark is.
	      */
	      this_op = 0;  /* takes no argument */
	      continue;

	    default:
	      this_op = in;
	      /* Collect argument next time around the loop and
		 operate on it. */
	      continue;
	    }
	}
      else
	{
	  this_op = 0;
	}

      for (int bitnum = 0; bitnum < 8; ++bitnum)
	{
	  if (skipbits > 0)
	    {
	      --skipbits;
	      if (DFS::verbose)
		{
		  std::cerr << "HFEv3: skipping a bit ("
			    << std::dec << skipbits << " more to skip)\n";
		}
	      continue;
	    }
	  int mask = (1 << (7-bitnum));
	  const int bit = in & mask ? 0x80 : 0;
	  /* the output bit might be a clock bit or it might be
	     data, we worry about that separately. */
	  out = static_cast<byte>((out >> 1 ) | bit);
	  ++got_bits;
	}
      if (8 == got_bits)
	{
	  *dest++ = out;
	  out = 0;
	  got_bits = 0;
	}
    }
  if (this_op)
    {
      premature_stream_end(this_op);
    }
}

// Sort the sectors by address.
std::vector<Sector>
sorted_sectors(std::vector<Sector>&& track_sectors)
{
  std::vector<Sector> result;
  std::sort(track_sectors.begin(), track_sectors.end(),
	    [](const Sector& a, const Sector& b)
	    {
	      return a.address < b.address;
	    });
  std::swap(result, track_sectors);
  return result;
}

std::vector<Sector>
HfeFile::read_all_sectors(const std::vector<PicTrack>& lut,
			  unsigned int side)
{
  assert(side == 0 || side == 1);
   // offset_unit_size is the unit size of lut[i].offset_in_blocks.
  constexpr unsigned int offset_unit_size = 512;
  std::vector<Sector> result;
  std::optional<unsigned int> sectors_per_track;
  for (unsigned int track = 0 ; track < header_.number_of_track; ++track)
    {
      const unsigned char encoding = encoding_of_track(side, track);
      if (encoding != ISOIBM_FM_ENCODING && encoding != ISOIBM_MFM_ENCODING)
	{
	  std::ostringstream ss;
	  ss << "track " << track << " has unsupported track encoding value "
	     << unsigned(encoding) << " (" << encoding_name(encoding) << ")";
	  throw UnsupportedHfeFile(ss.str());
	}

      unsigned int offset_in_blocks = lut[track].offset();
      unsigned long track_len_in_bytes = lut[track].track_len();

      constexpr std::vector<byte>::size_type side_block_size = 256u;
      constexpr unsigned int raw_data_block_size = side_block_size * 2;
      const auto max_offset = std::numeric_limits<std::streamoff>::max();
      assert(max_offset / raw_data_block_size > offset_in_blocks);

      std::vector<byte> raw_data = file_->read(offset_in_blocks * offset_unit_size,
					       track_len_in_bytes);
      auto track_bytes_read = raw_data.size();
      if (DFS::verbose)
	{
	  std::cerr << "Track " << std::dec << track << " has " << track_len_in_bytes
		    << " bytes of data; we read " << track_bytes_read
		    << "\n";
	}
      // While we could simply deal with the bit ordering in the input file when
      // dealing with subsequent stages, that will make it harder to interpret
      // nuumeric arguments to HFEv3 opcodes.
      assert(Track::reverse_bit_order(0x43) == 0xC2);
      std::transform(raw_data.begin(), raw_data.end(),
		     raw_data.begin(), // transform in-place.
		     Track::reverse_bit_order);

      // The data is in side_block_size chunks (side 0 then side 1,
      // etc.) but we only want the data for one of the sides.
      std::vector<byte> track_stream;
      track_stream.reserve(track_len_in_bytes / 2);
      auto begin_offset = side_block_size * side;
      while (begin_offset < track_bytes_read)
	{
	  const auto end_offset = std::min(begin_offset + side_block_size,
					   track_bytes_read);
	  assert(end_offset <= raw_data.size());
	  if (DFS::verbose)
	    {
#if ULTRA_VERBOSE
	      std::cerr << "Track " << track << ": copying "
			<< (end_offset - begin_offset) << " bytes starting at "
			<< "offset " << begin_offset << " to position "
			<< track_stream.size() << " in the track stream\n";
	      std::cerr << "Input:\n";
	      DFS::hexdump_bytes(std::cerr, begin_offset, 16,
				 raw_data.data() + begin_offset,
				 raw_data.data() + end_offset);
#endif
	    }
#if ULTRA_VERBOSE
	  auto oldsize = track_stream.size();
#endif
	  copy_hfe(3 == hfe_version_,
		   raw_data.data() + begin_offset,
		   raw_data.data() + end_offset,
		   std::back_inserter(track_stream));
	  if (DFS::verbose)
	    {
#if ULTRA_VERBOSE
	      std::cerr << "Output:\n";
	      DFS::hexdump_bytes(std::cerr, oldsize, 16,
				 track_stream.data() + oldsize,
				 track_stream.data() + track_stream.size());
#endif
	    }
	  begin_offset += raw_data_block_size;
	}
#if ULTRA_VERBOSE
      if (DFS::verbose)
	{
	  std::cerr << std::dec << std::setfill(' ')
		    << "Track " << std::setw(2) << track << ": " << track_len_in_bytes
		    << " bytes at position "
		    << (offset_unit_size * offset_in_blocks)
		    << "; " << track_stream.size()
		    << " bytes seem to be for side " << side << "\n";
	}
#endif

      // Extract the encoded sectors.
      assert(encoding == ISOIBM_FM_ENCODING || encoding == ISOIBM_MFM_ENCODING);
      const bool is_fm = encoding == ISOIBM_FM_ENCODING;
      const size_t first_bit = is_fm ? 1 : 0;
      const size_t stride = is_fm ? 2 : 1;
      Track::BitStream bits(track_stream, first_bit, stride);
      auto decoder = (is_fm ? decode_fm_track : decode_mfm_track);
      const std::vector<Sector> track_sectors = sorted_sectors(decoder(bits, DFS::verbose));

      if (DFS::verbose)
	{
	  std::cerr << "Found " << track_sectors.size() << " sectors on track "
		    << track << "\n";
	}
      if (!sectors_per_track)
	{
	  sectors_per_track = track_sectors.size();
	}
      else if (track_sectors.size() != *sectors_per_track)
	{
	  std::ostringstream ss;
	  ss << "track " << track << " has " << track_sectors.size()
	     << " sectors but other tracks have " << *sectors_per_track
	     << " sectors; this is not supported";
	  throw UnsupportedHfeFile(ss.str());
	}

      std::string error;
      if (!DFS::check_track_is_supported(track_sectors, track, side, DFS::SECTOR_BYTES, DFS::verbose, error))
	{
	  throw UnsupportedHfeFile(error);
	}
      std::copy(track_sectors.begin(), track_sectors.end(),
		std::back_inserter(result));
    }

  assert(header_.number_of_track > 0);
  assert(sectors_per_track.has_value());
  DFS::Encoding enc;
  switch (header_.track_encoding)
    {
    case ISOIBM_MFM_ENCODING:
    case AMIGA_MFM_ENCODING:
      enc = DFS::Encoding::MFM;
      break;
    case ISOIBM_FM_ENCODING:
    case EMU_FM_ENCODING:
      enc = DFS::Encoding::FM;
      break;
    default:
      {
	std::ostringstream ss;
	ss << "disc has unsupported encoding "
	   << encoding_name(header_.track_encoding);
	throw UnsupportedHfeFile(ss.str());
      }
    }

  geom_ = DFS::Geometry(header_.number_of_track,
			header_.number_of_side,
			*sectors_per_track,
			enc);
  return result;
}

bool HfeFile::connect_drives(DFS::StorageConfiguration* storage,
			     DFS::DriveAllocation how,
			     std::string& error)
{
  std::vector<std::optional<DFS::DriveConfig>> drives;
  for (auto& accessor : acc_)
    {
      std::optional<DFS::Format> fmt =
	DFS::identify_file_system(accessor, accessor.geometry(), false, error);
      // TODO: detect unformatted drive (relevant because side 1 may be absent).
      //
      // TODO: decide how many devices to present when sides=2, presumably
      // based on the value of fmt, and bear this in mind when converting
      // the lba value in read_block back onto a track, side and sector number.
      DFS::DriveConfig dc(fmt, &accessor);
      drives.push_back(dc);
    }
  return storage->connect_drives(drives, how);
}

}  // namespace

namespace DFS
{
  std::unique_ptr<DFS::AbstractImageFile>
  make_hfe_file(const std::string& name, bool compressed, std::unique_ptr<FileAccess> file, std::string& error)
  {
    try
      {
	std::unique_ptr<HfeFile> result = std::make_unique<HfeFile>(name, compressed, std::move(file));
	return result;
      }
    catch (std::exception& e)
      {
	error = e.what();
	return 0;
      }
  }
}  // namespace DFS
