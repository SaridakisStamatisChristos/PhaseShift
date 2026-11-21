#pragma once
#include "phaseshift/core.hpp"
#include <cstdint>
#include <limits>
namespace ps {
// A v2 hypothesis gets an exclusive 1e9-wide seed window. Validate the
// entire fixed holdout and negative-control horizon before TESTED/SQL writes.
inline void validate_v2_confirmation_seed_space(std::uint64_t base,
 std::uint64_t rank,int pairs){
 if(rank<1||rank>16||pairs<20||pairs>100)
  throw Error("invalid v2 confirmation seed schedule");
 constexpr auto max_seed=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
 constexpr std::uint64_t window=1000000000ULL;
 const std::uint64_t offset=(rank-1)*window;
 // Covers performance 1e6+i*97, failure 7e6+i*101 and independent
 // negative control 30e6. No two H1..H16 windows overlap.
 const std::uint64_t last=30000000ULL+static_cast<std::uint64_t>(pairs-1)*101ULL;
 if(offset>max_seed||last>max_seed-offset||base>max_seed-offset-last)
  throw Error("v2 confirmation seed groups overflow signed SQLite domain");
}
} // namespace ps
