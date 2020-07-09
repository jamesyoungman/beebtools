#ifndef INC_TRACK_H /* -*- Mode: C++ -*- */
#define INC_TRACK_H 1

#include <optional>
#include <functional>

struct SectorAddress
{
  unsigned char cylinder;
  unsigned char head;
  unsigned char record;		// note: 1-based

  bool operator<(const SectorAddress& a) const;
  bool operator==(const SectorAddress& a) const;
  bool operator!=(const SectorAddress& a) const;
};

namespace std
{
  ostream& operator<<(ostream&, const SectorAddress&);
}  // namespace std

struct Sector
{
  SectorAddress address;
  // We don't yield control sectors.
  std::vector<unsigned char> data;
  unsigned short crc;
};

// IbmFmDecoder decodes an FM track into sectors.
class IbmFmDecoder
{
public:
  IbmFmDecoder(bool verbose);

  // decode() decodes an FM data stream (as clock/data byte pairs)
  // into a sector. If no more sectors are available in source, nullopt
  // is returned.  source must be initialized such that the first byte
  // it returns is after the index mark and before the sync field.
  //
  // Only data sectors are returned.
  std::vector<Sector> decode(const std::vector<unsigned char>& track);

private:
  bool verbose_;
};

#endif
