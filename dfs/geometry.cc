#include "geometry.h"

#include "abstractio.h"

#include <cassert>
#include <cassert>
#include <exception>
#include <optional>
#include <sstream>

namespace
{
  std::optional<DFS::sector_count_type> guess_spt(unsigned long int total)
  {
    // We guess 18 sectors per track first, as 720 (e.g. chs=40,1,18)
    // is divisible by both 18 and 10.
    if (total % 18 == 0)
      return DFS::sector_count(18);
    if (total % 10 == 0)
      return DFS::sector_count(10);
    return std::nullopt;
  }

  std::optional<int> guess_heads(long total_sectors,
				 int sectors_per_track)
  {
    if (total_sectors <= (1 * 40L * sectors_per_track))
	return 1;
    // 2*40 == 1*80, so it is hard to tell the difference betwen a
    // single sided 80t image and a double sided 40t image based on
    // size alone.  To distinguish these, we rely on
    // guess_geometry_from_total_bytes() being called with a heads
    // parameter, and so in that case this function will not be
    // called.
    if (total_sectors > (1 * 80L * sectors_per_track))
      return 2;
    return std::nullopt;
  }

}  // namespace

namespace DFS
{
  DFS::sector_count_type Geometry::total_sectors() const
  {
    return DFS::sector_count(cylinders * heads * sectors);
  }

  std::optional<Geometry> guess_geometry_from_total_bytes
  (unsigned long total_bytes, std::optional<int> heads)
  {
    if (total_bytes % DFS::SECTOR_BYTES)
      return std::nullopt;
    const auto tot_sectors = total_bytes / DFS::SECTOR_BYTES;
    const std::optional<DFS::sector_count_type> sectors_per_track = guess_spt(tot_sectors);
    if (!sectors_per_track)
      return std::nullopt;
    Geometry g;
    g.sectors = *sectors_per_track;
    if (!heads)
      {
	heads = guess_heads(tot_sectors, g.sectors);
	if (!heads)
	  return std::nullopt;
      }
    g.heads = *heads;
    auto c = tot_sectors / (g.heads * g.sectors);
    if (c > std::numeric_limits<int>::max())
      throw std::out_of_range("infeasibly large device");
    g.cylinders = static_cast<int>(c);
    if (sectors_per_track == 10)
      g.encoding = Encoding::FM;
    else if (sectors_per_track == 18)
      g.encoding == Encoding::MFM;
    return g;
  }

  Geometry::Geometry(int c, int h, DFS::sector_count_type s, std::optional<Encoding> enc)
    : cylinders(c), heads(h), sectors(s), encoding(enc)
  {
  }

  Geometry::Geometry()
    : cylinders(0), heads(0), sectors(0)
  {
  }

  bool Geometry::operator==(const Geometry& g) const
  {
    if (cylinders != g.cylinders)
      return false;
    if (heads != g.heads)
      return false;
    if (sectors != g.sectors)
      return false;
    if (encoding && g.encoding)
      return *encoding == *g.encoding;
    else
      return true;
  }

  std::string encoding_to_str(const DFS::Encoding& e)
  {
    switch (e)
      {
      case DFS::Encoding::FM: return "FM";
      case DFS::Encoding::MFM: return "MFM";
      }
    assert(!"unhandled case in encoding_to_str");
  }

  std::string encoding_description(const DFS::Encoding& e)
  {
    if (DFS::Encoding::MFM == e)
      return "double density";
    else
      return "single density";
  }

  std::string Geometry::to_str() const
  {
    std::ostringstream os;
    os << "{chs=" << cylinders << "," << heads << "," << sectors;
    if (encoding)
      {
	os << ", encoding=" << encoding_to_str(*encoding);
      }
    else
      {
	os << ", encoding unknown";
      }
    os << "}";
    return os.str();
  }

  std::string Geometry::description() const
  {
    std::ostringstream os;
    if (encoding)
      {
	os << encoding_description(*encoding) << ", ";

      }
    os << heads << " " << (heads == 1 ? "side" : "sides") << ", "
       << cylinders << " " << (cylinders == 1 ? "track" : "tracks") << ", "
       << sectors << " " << (sectors == 1 ? "sector" : "sectors") << " per track";
    return os.str();
  }

}  // namespace DFS
