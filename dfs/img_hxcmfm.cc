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
  HxC MFM file format support
*/
#include <iomanip>		// for setw, hex, dec
#include <set>			// for set
#include <sstream>		// for ostringstream
#include <string>		// for string
#include <vector>		// for vector<>

#include "dfs.h"		// for verbose
#include "hexdump.h"		// for DFS::hexdump_bytes
#include "identify.h"		// for identify_file_system
#include "media.h"		// for AbstractImageFile
#include "storage.h"		// for StorageConfiguration
#include "track.h"		// for Sector

using Track::Sector;
using Track::byte;

namespace
{
constexpr unsigned int HEADER_SIZE_BYTES = 0x11u;
constexpr unsigned int TRACK_METADATA_SIZE_BYTES = 0x11u;


DFS::Geometry compute_geometry(unsigned int sides,
			       const std::vector<Sector>& sectors)
{
  std::set<unsigned char> cylinders, records;
  for (const Track::Sector s : sectors)
    {
      cylinders.insert(cylinders.end(), s.address.cylinder);
      records.insert(s.address.record);
    }
  return DFS::Geometry(cylinders.size(), sides, records.size(),
		       DFS::Encoding::MFM);

}


class InvalidHxcMfmFile : public std::exception
{
public:
  InvalidHxcMfmFile(const std::string& msg)
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

class UnsupportedHxcMfmFile : public std::exception
{
public:
  UnsupportedHxcMfmFile(const std::string& msg)
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

struct Header // an in-memory, not on-disk, representation
{
  char signature[7];
  unsigned int tracks;
  unsigned int sides;
  unsigned int rpm;
  unsigned int bitrate;
  unsigned int interface_type;
  unsigned long track_list_offset;
};


  std::ostream& operator<<(std::ostream& os, const Header& h)
{
  int col1 = 18;
  using std::setw;
  std::ostream::sentry s(os);
  if (s)
    {
      os << std::dec;
      os << setw(col1) << "signature" << ": " << h.signature << "\n";
      os << setw(col1) << "tracks" << ": " << h.tracks << "\n";
      os << setw(col1) << "sides" << ": " << h.sides << "\n";
      os << setw(col1) << "rpm" << ": " << h.rpm << "\n";
      os << setw(col1) << "bitrate" << ": " << h.bitrate << "\n";
      os << setw(col1) << "interface_type" << ": " << h.interface_type << "\n";
      os << setw(col1) << "track_list_offset" << ": " << h.track_list_offset << "\n";
    }
  return os;
}

struct TrackDataKey
{
  unsigned int track_number;
  unsigned int side_number;

  TrackDataKey(unsigned int track, unsigned int side)
    : track_number(track), side_number(side)
  {
  }

  bool operator<(const TrackDataKey& other) const
  {
    if (track_number < other.track_number)
      return true;
    if (other.track_number < track_number)
      return false;
    return side_number < other.side_number;
  }
};


std::ostream& operator<<(std::ostream& os, const TrackDataKey& k)
{
  std::ostream::sentry s(os);
  if (s)
    {
      std::ostringstream ss;
      ss << '(' << std::setw(2) << k.track_number << ',' << k.side_number << ')';
      os << ss.str();
    }
  return os;
}

struct TrackData		// an in-memory, not on-disk, representation
{
  TrackData(unsigned long size, unsigned offset)
    : mfmtracksize(size), mfmtrackoffset(offset)
  {
  }

  unsigned long mfmtracksize;
  unsigned long mfmtrackoffset;
};

std::ostream& operator<<(std::ostream& os, const TrackData& d)
{
  std::ostream::sentry s(os);
  if (s)
    {
      std::ostringstream ss;
      ss << "(offset=" << std::setw(6) << d.mfmtrackoffset
	 << ", size=" << std::setw(5) << d.mfmtracksize << ')';
      os << ss.str();
    }
  return os;
}

unsigned short le_word(const byte *d)
{
  return static_cast<unsigned short>(d[0] | (d[1] << 8u));
}

unsigned long le_quad(const byte *d)
{
  return static_cast<unsigned long>(d[0] | (d[1] << 8u) |
				    (d[2] << 16u) | (d[3] << 24u));
}

std::optional<Header> read_and_verify_header(DFS::FileAccess *f, std::string& error)
{
  std::vector<byte> header_data = f->read(0, 19);
  const byte* d = header_data.data();
  /* 0x00 - 0x06 is a magic string, including a terminating NUL. */
  const char expected_magic[7] = "HXCMFM";
  if (memcmp(expected_magic, header_data.data(), sizeof(expected_magic)))
    {
      std::ostringstream ss;
      ss << "header signature is invalid (should be " << expected_magic << "): ";
      DFS::hexdump_bytes(ss,
			 0,
			 sizeof(Header::signature),
			 static_cast<const unsigned char*>(header_data.data()),
			 static_cast<const unsigned char*>(header_data.data() + sizeof(Header::signature)));
      error = ss.str();
      return std::nullopt;
    }

  Header result;
  std::copy(header_data.begin(), header_data.begin() + sizeof(Header::signature),
	    result.signature);
  assert(sizeof(result.signature) == 0x07u); // signature is followed immediately by...
  /* 0x07, 0x08  */ result.tracks = le_word(d + 0x07);
  /* 0x09        */ result.sides = d[0x09];
  /* 0x0A - 0x0B */ result.rpm = le_word(d + 0x0A);
  /* 0x0C - 0x0D */ result.bitrate = le_word(d + 0x0C);
  /* 0x0E        */ result.interface_type = d[0x0E];
  /* 0x0F - 0x12 */ result.track_list_offset = le_quad(d + 0x0F);

  if (result.track_list_offset < 0x13)
    {
      std::ostringstream ss;
      ss << "header data is invalid; the track list begins at file position "
	 << std::dec << result.track_list_offset
	 << " which is within the header itself";
      error = ss.str();
      return std::nullopt;
    }

  if (DFS::verbose)
    {
      std::cerr << result;
    }
  return result;
}


class HxcMfmFile : public DFS::AbstractImageFile
{
public:
  explicit HxcMfmFile(const std::string& name, bool compressed, std::unique_ptr<DFS::FileAccess>&& file);
  std::string description() const;
  bool connect_drives(DFS::StorageConfiguration* storage, DFS::DriveAllocation how,
		      std::string& error) override;

private:
  std::map<TrackDataKey, TrackData> get_track_metadata();
  std::vector<Track::Sector> read_all_sectors(unsigned int side,
					      const std::map<TrackDataKey, TrackData>&);

  class DataAccessAdapter : public DFS::AbstractDrive
  {
  public:
    DataAccessAdapter(HxcMfmFile* f,
		      DFS::Geometry geom,
		      unsigned int side,
		      const std::vector<Track::Sector> sectors)
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
      const Sector& sect(sectors_[lba]);
      DFS::SectorBuffer buf;
      std::copy(sect.data.begin(), sect.data.end(), buf.begin());
      return buf;
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

    HxcMfmFile *f_;
    std::string name_;
    std::unique_ptr<DFS::FileAccess> file_;
    DFS::Geometry geom_;	// geom_ has just one side.
    unsigned int side_;
    std::vector<Track::Sector> sectors_;
  };

  Header header_;
  std::string name_;
  std::unique_ptr<DFS::FileAccess> file_;
  const bool compressed_;
  std::vector<DataAccessAdapter> acc_;
};

HxcMfmFile::HxcMfmFile(const std::string& name, bool compressed, std::unique_ptr<DFS::FileAccess>&& file)
  : name_(name),
    file_(std::move(file)),
    compressed_(compressed)
{
  std::string error;
  auto header = read_and_verify_header(file_.get(), error);
  if (!header)
    {
      throw InvalidHxcMfmFile(error);
    }
  /* header is valid but we may not support it; check this now. */
  if (header->sides > 2)
    {
      std::ostringstream ss;
      ss << "image file encodes more than 2 sides:  " << header->sides;
      throw UnsupportedHxcMfmFile(ss.str());
    }
  /* We can accept any number of tracks, don't care about the RPM or bit rate. */
  if (header->interface_type != 4)
    {
      std::ostringstream ss;
      ss << "image file has unsupported interface type " << header->interface_type;
      throw UnsupportedHxcMfmFile(ss.str());
    }
  /* header is valid and represents a configuration we support. */
  header_ = *header;

  const std::map<TrackDataKey, TrackData> track_metadata = get_track_metadata();
  for (unsigned int side = 0; side < header_.sides; ++side)
    {
      std::vector<Sector> sectors = read_all_sectors(side, track_metadata);
      DFS::Geometry g = compute_geometry(1, sectors);
      acc_.emplace_back(this, g, side, sectors);
    }
}

std::string HxcMfmFile::description() const
{
  std::ostringstream ss;
  if (compressed_)
    ss << "compressed ";
  ss << "HxC MFM file " << name_;
  return ss.str();
}



bool HxcMfmFile::connect_drives(DFS::StorageConfiguration* storage, DFS::DriveAllocation how,
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

std::map<TrackDataKey, TrackData> HxcMfmFile::get_track_metadata()
{
  std::map<TrackDataKey, TrackData> result;

  for (unsigned long pos = header_.track_list_offset;
       /* termination by break */;
       pos += 11)
    {
      std::vector<byte> raw_metadata = file_->read(pos, 11);
      const byte* raw = raw_metadata.data();
      const TrackDataKey key(le_word(raw), raw[2]);
      const TrackData td(le_quad(raw+3), le_quad(raw+7));
      if (DFS::verbose)
	{
	  std::cerr << "HxcMfmFile::get_track_metadata: data for "
		    << std::setw(6) << key << " is at " << td << "\n";
	}

      result.insert(result.end(), std::make_pair(key, td));
      if (key.track_number == header_.tracks-1 && key.side_number == header_.sides-1)
	break;
    }
  if (DFS::verbose)
    {
      std::cerr << "HxcMfmFile::get_track_metadata: collected data for "
		<< result.size() << " tracks\n";
    }
  return result;
}



std::vector<Sector>
HxcMfmFile::read_all_sectors(unsigned int side,
			     const std::map<TrackDataKey, TrackData>& track_metadata)
{
  std::vector<Sector> result;
  for (auto [key, td] : track_metadata)
    {
      if (key.side_number != side)
	continue;

      std::vector<byte> track = file_->read(td.mfmtrackoffset, td.mfmtracksize);
      if (track.size() != td.mfmtracksize)
	{
	  std::ostringstream ss;
	  ss << "image file contains metadata for track " << key.track_number
	     << " stating that the data for that track begins at file offset "
	     << td.mfmtrackoffset << " and that the data is "
	     << td.mfmtracksize << " bytes long, but this doesn't fit within the file";
	  throw InvalidHxcMfmFile(ss.str());
	}

      std::transform(track.begin(), track.end(),
		     track.begin(), // transform in-place.
		     Track::reverse_bit_order);

      Track::BitStream bits(track);
      std::vector<Sector> track_sectors = Track::IbmMfmDecoder(DFS::verbose).decode(bits);
      std::sort(track_sectors.begin(), track_sectors.end(),
		[](const Sector& a, const Sector& b)
		{
		  return a.address < b.address;
		});
      std::string error;
      if (!DFS::check_track_is_supported(track_sectors, key.track_number, key.side_number, DFS::SECTOR_BYTES, DFS::verbose, error))
	{
	  throw UnsupportedHxcMfmFile(error);
	}
      std::copy(track_sectors.begin(), track_sectors.end(),
		std::back_inserter(result));
    }
  return result;
}

}  // namespace

namespace DFS
{
  std::unique_ptr<DFS::AbstractImageFile>
  make_hxcmfm_file(const std::string& name, bool compressed, std::unique_ptr<FileAccess> file, std::string& error)
  {
    try
      {
	std::unique_ptr<HxcMfmFile> result = std::make_unique<HxcMfmFile>(name, compressed, std::move(file));
	return result;
      }
    catch (std::exception& e)
      {
	error = e.what();
	return 0;
      }
  }

}  // namespace DFS
