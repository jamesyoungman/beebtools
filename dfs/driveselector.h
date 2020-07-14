// We draw a distinction between the identifier of a single surface of
// a floppy disc and the identification of the immediate container of
// a root catalog.  While in Acorn DFS these are the same thing, this
// is not the case in Opus DDOS.
//
// In Opus DDOS, a single floppy disc can contain up to eight volumes.
// These are identified by a letter (A-H). In Opus DDOS, a decimal
// number identifies a single disc catalog containing up to eight
// volumes.  Each volume contains a DFS catalog (i.e. a root).
//
//
// The types corresponding to these concepts are:
//
// SurfaceSelector - identifies a disc surface
// VolumeSelector - identifies a specific volume.
//
// For Acorn DFS file systems, surface N has just one associated
// volume which is unnamed.  In Opus DDOS, surface N may have up to 8
// volumes, but if no volume is specidied, A is assumed.

#ifndef INC_DRIVESELECTOR_H
#define INC_DRIVESELECTOR_H 1

#include <exception>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

namespace DFS
{

  class SurfaceSelector
  {
  public:
    using repr_type = unsigned int;

    constexpr explicit SurfaceSelector(repr_type d)
      : d_(d)
      {
      }

    explicit SurfaceSelector(long int ld)
      : d_(coerce(ld))
      {
      }

    explicit SurfaceSelector(int i)
      : d_(coerce(i))
      {
      }

    static SurfaceSelector acorn_default_last_surface();
    static std::optional<SurfaceSelector> parse(const std::string&, size_t*end, std::string& error);
    bool operator<(const SurfaceSelector& other) const
    {
      return d_ < other.d_;
    }

    repr_type surface() const
    {
      return d_;
    }

    SurfaceSelector next() const;
    SurfaceSelector prev() const;

    bool operator==(const SurfaceSelector& other)
      {
       return d_ == other.d_;
      }

    SurfaceSelector postincrement()
      {
       SurfaceSelector clone(d_);
       ++d_;
       return clone;
      }

    // Returns the selector of the opposite surface on the same media
    // (whether it corresponds to readable media or not).
    SurfaceSelector opposite_surface() const;

    static SurfaceSelector corresponding_side_of_next_device(const SurfaceSelector&);
    void ostream_insert(std::ostream& os) const;
    std::string to_string() const;

  private:
    static unsigned int coerce(long int li);
    static unsigned int coerce(int i);

    repr_type d_;
  };

typedef SurfaceSelector drive_number;

 // A VolumeSelector identifies a specific volume on a drive.
 class VolumeSelector
 {
 public:
   explicit VolumeSelector(const DFS::SurfaceSelector& n); /* with default (unnamed) volume */
   explicit VolumeSelector(unsigned n); /* with default (unnamed) volume */
   explicit VolumeSelector(const SurfaceSelector& surface, char subvol);
   VolumeSelector(const VolumeSelector&);
   static std::optional<VolumeSelector> parse(const std::string&, size_t*end, std::string& error);
   std::string to_string() const;
   void ostream_insert(std::ostream&) const;
   VolumeSelector& operator=(const VolumeSelector&);
   bool operator==(const VolumeSelector&) const;
   bool operator<(const VolumeSelector&) const;
   SurfaceSelector surface() const
   {
     return surface_;
   }
   char effective_subvolume() const;
   std::optional<char> subvolume() const;

 private:
   SurfaceSelector surface_;
   std::optional<char> subvolume_;
 };

}  // namespace DFS


namespace std
{
  ostream& operator<<(ostream& os, const DFS::SurfaceSelector& sel);
  ostream& operator<<(ostream& os, const DFS::VolumeSelector& vol);
  string to_string(const DFS::SurfaceSelector&);

  template<> class numeric_limits<DFS::SurfaceSelector>
    {
    public:
      static constexpr bool is_specialized = true;
      static constexpr bool is_signed = numeric_limits<DFS::SurfaceSelector::repr_type>::is_signed;
      static constexpr bool is_integer = numeric_limits<DFS::SurfaceSelector::repr_type>::is_integer;
      static constexpr bool is_exact = numeric_limits<DFS::SurfaceSelector::repr_type>::is_exact;
      static constexpr bool has_infinity = numeric_limits<DFS::SurfaceSelector::repr_type>::has_infinity;
      static constexpr bool has_quiet_NaN = numeric_limits<DFS::SurfaceSelector::repr_type>::has_quiet_NaN;
      static constexpr bool has_denorm = numeric_limits<DFS::SurfaceSelector::repr_type>::has_denorm;
      static constexpr bool has_denorm_loss = numeric_limits<DFS::SurfaceSelector::repr_type>::has_denorm_loss;
      static constexpr auto round_style = numeric_limits<DFS::SurfaceSelector::repr_type>::round_style;
      static constexpr bool is_iec559 = numeric_limits<DFS::SurfaceSelector::repr_type>::is_iec559;
      static constexpr bool is_bounded = numeric_limits<DFS::SurfaceSelector::repr_type>::is_bounded;
      static constexpr bool is_modulo = numeric_limits<DFS::SurfaceSelector::repr_type>::is_modulo;
      static constexpr auto digits = numeric_limits<DFS::SurfaceSelector::repr_type>::digits;
      static constexpr auto digits10 = numeric_limits<DFS::SurfaceSelector::repr_type>::digits10;
      static constexpr auto max_digits10 = numeric_limits<DFS::SurfaceSelector::repr_type>::max_digits10;
      static constexpr int radix = numeric_limits<DFS::SurfaceSelector::repr_type>::radix;
      static constexpr int min_exponent = numeric_limits<DFS::SurfaceSelector::repr_type>::min_exponent;
      static constexpr int max_exponent = numeric_limits<DFS::SurfaceSelector::repr_type>::max_exponent;
      static constexpr int max_exponent10 = numeric_limits<DFS::SurfaceSelector::repr_type>::max_exponent10;
      static constexpr bool traps = numeric_limits<DFS::SurfaceSelector::repr_type>::traps;
      static constexpr bool tinyness_before = numeric_limits<DFS::SurfaceSelector::repr_type>::tinyness_before;

      static constexpr DFS::SurfaceSelector max()
      {
	return DFS::SurfaceSelector(numeric_limits<DFS::SurfaceSelector::repr_type>::max());
      }

      static constexpr DFS::SurfaceSelector min()
      {
	return DFS::SurfaceSelector(numeric_limits<DFS::SurfaceSelector::repr_type>::min());
      }

      static constexpr DFS::SurfaceSelector lowest()
      {
	return min();
      }

      static constexpr DFS::SurfaceSelector epsilon()
      {
	return DFS::SurfaceSelector(numeric_limits<DFS::SurfaceSelector::repr_type>::epsilon());
      }

      static constexpr DFS::SurfaceSelector round_error()
      {
	return DFS::SurfaceSelector(numeric_limits<DFS::SurfaceSelector::repr_type>::round_error());
      }

      static constexpr DFS::SurfaceSelector infinity()
      {
	return DFS::SurfaceSelector(numeric_limits<DFS::SurfaceSelector::repr_type>::infinity());
      }

      static constexpr DFS::SurfaceSelector quiet_NaN()
      {
	return DFS::SurfaceSelector(numeric_limits<DFS::SurfaceSelector::repr_type>::quiet_NaN());
      }

      static constexpr DFS::SurfaceSelector signaling_NaN()
      {
	return DFS::SurfaceSelector(numeric_limits<DFS::SurfaceSelector::repr_type>::signaling_NaN());
      }

      static constexpr DFS::SurfaceSelector denorm_min()
      {
	return DFS::SurfaceSelector(numeric_limits<DFS::SurfaceSelector::repr_type>::denorm_min());
      }

    };
}

#endif
