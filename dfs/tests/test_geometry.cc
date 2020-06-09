#include "geometry.h"

#include <iostream>
#include <set>
#include <vector>

using DFS::Geometry;
using std::cerr;

namespace
{

bool test_geom_guess(std::string& label, unsigned long tot_bytes, std::optional<int>heads,
		     const Geometry& expected)
{
  std::optional<Geometry> guessed = DFS::guess_geometry_from_total_bytes(tot_bytes,
									 heads);

  auto fail = [&label, &tot_bytes, &heads](const std::string& failure_reason) -> bool
	      {
		cerr << "test_geom_guess: " << label << ": failed on tot_bytes="
		     << tot_bytes << ", ";
		if (heads)
		  cerr << "heads=" << *heads;
		else
		  cerr << "heads unknown";
		cerr << ": " << failure_reason << "\n";
		return false;
	      };
  if (!guessed)
    {
     return fail("guess failed");
    }
  if (*guessed == expected)
    return true;
  return fail("expected " + expected.to_str() + " got " + guessed->to_str());
}


bool test_dfs_geoms()  // Tests for DFS floppy disk geometries.
{
  /* There are ADFS image files also, some of which have other numbers
     of bytes per sector (e.g. chs=80,2,5 at 10244 bytes/sector) but
     the geometry guesser doesn't support such formats (since the
     program of which it is a part currently only understands DFS
     formats, not ADFS formats). */
  struct testcase
  {
    const char* label;
    unsigned long tot_bytes;
    std::optional<int> heads;
    Geometry expected;
  };
  using DFS::Encoding;
  const std::optional<int> U = std::nullopt; // unknown number of heads
  std::set<std::string> labels_seen;
  const std::vector<testcase> test_cases =
    {
     /* Single density formats. */
      {"40t.ss.sd_1", 40*1*10*256, 1, Geometry(40, 1, 10, Encoding::FM) },
      {"40t.ss.sd_U", 40*1*10*256, U, Geometry(40, 1, 10, Encoding::FM) },
      {"80t.ss.sd_1", 80*1*10*256, 1, Geometry(80, 1, 10, Encoding::FM) },
      /* The guesser won't guess 40,2,10 or 80,1,10 with unknown
	 heads, since both have the same total number of sectors.  To
	 get either you have to hint the number of heads. */
      {"40t.ds.sd_2", 40*2*10*256, 2, Geometry(40, 2, 10, Encoding::FM) },
      {"80t.ds.sd_2", 80*2*10*256, 2, Geometry(80, 2, 10, Encoding::FM) },
      {"80t.ds.sd_U", 80*2*10*256, U, Geometry(80, 2, 10, Encoding::FM) },
      /* The guesser should be able to cope with some esoteric formats
	 (for example 35 track which I have seen mentioned in
	 alternative DFS implementation user documentation, but never
	 seen an example of).  However, since there seem to be no
	 examples of image files having these formats I don't think
	 it's reasonable to introduce a test case here (since the
	 implication would be that the rest of the code would support
	 such a format).

	 There are image files which don't record in the file the data
	 for every sector of the device, but the convention for these
	 appears to be that the encoded device has a "normal" number
	 of tracks, and the data beyond the end-of-file all has some
	 conventional (e.g. zero) value.
      */

      /* Double density formats. */
      {"40t.ss.dd_1", 40*1*18*256, 1, Geometry(40, 1, 18, Encoding::MFM) },
      {"40t.ss.dd_U", 40*1*18*256, U, Geometry(40, 1, 18, Encoding::MFM) },
      {"80t.ss.dd_1", 80*1*18*256, 1, Geometry(80, 1, 18, Encoding::MFM) },
      /* The guesser won't guess 40,2,18 or 80,1,18 with unknown
	 heads, since both have the same total number of sectors.  To
	 get either you have to hint the number of heads.  However, in
	 practice if we know the disk is double density, a 40 track
	 format may be unlikely.  Perhaps in the future we should
	 allow the guesser to prefer 80t if it knows the device is 18
	 sectors per track. */
      {"40t.ds.dd_2", 40*2*18*256, 2, Geometry(40, 2, 18, Encoding::MFM) },
      {"80t.ds.dd_2", 80*2*18*256, 2, Geometry(80, 2, 18, Encoding::MFM) },
      {"80t.ds.dd_U", 80*2*18*256, U, Geometry(80, 2, 18, Encoding::MFM) },
    };
  for (const auto& tc : test_cases)
    {
      std::string label(tc.label);
      if (labels_seen.find(label) != labels_seen.end())
	{
	  cerr << "duplicate label " << label << "\n";
	  return false;
	}
      labels_seen.insert(label);
      if (!test_geom_guess(label, tc.tot_bytes, tc.heads, tc.expected))
	return false;
    }
  return true;
}

} // namespace

int main()
{
  return test_dfs_geoms() ? 0 : 1;
}
