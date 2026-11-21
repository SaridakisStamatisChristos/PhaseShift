#pragma once
// Opt-in matched discovery pair planning; does not run workers or see holdout.
#include "phaseshift/v3/types.hpp"
#include <limits>
#include <set>

namespace ps::v3 {
struct MatchedDiscoveryPair {
 ProposedTrial first,second;
 bool first_runs_first=true;
};
class ReplicatePlanner {
 std::uint64_t rng_;
 std::uint64_t next_number_=0;
 std::uint32_t max_pairs_;
 std::set<std::uint64_t> reserved_;
public:
 ReplicatePlanner(std::uint64_t seed,std::uint32_t max_pairs,
                  std::set<std::uint64_t> disallowed_seeds={})
  :rng_(seed),max_pairs_(max_pairs),reserved_(std::move(disallowed_seeds)) {
  if(!max_pairs || max_pairs>100000)
   throw Error("v3 invalid bounded matched replicate count");
 }
 MatchedDiscoveryPair propose(const AdmissibleSearchSpace& legal,
                               StructuralKey first,StructuralKey second) {
  first.validate();second.validate();
  if(first==second || first.generator_version!=second.generator_version ||
     !legal.index_of(first.coordinates) || !legal.index_of(second.coordinates))
   throw Error("v3 illegal/identical structural pair");
  if(next_number_>=max_pairs_)
   throw Error("v3 discovery replication cap reached");
  // Bound seed collision handling; never use hidden confirmation data.
  std::uint64_t seed=0;
  bool unique=false;
  for(int tries=0;tries<1024;++tries) {
   seed=ps::splitmix64(rng_) & 0x7fffffffULL; // SQLite signed-safe seed.
   if(!reserved_.count(seed)){unique=true;break;}
  }
  if(!unique)throw Error("v3 seed reservation could not find unique seed");
  reserved_.insert(seed);
  const auto index=static_cast<std::uint32_t>(next_number_);
  const auto number=next_number_++;
  ProposedTrial a{{std::move(first),{seed,index}},
                  ProposalIntent::Replicate,number*2,"","replicate_planner_v3",
                  "fresh matched discovery block"};
  ProposedTrial b{{std::move(second),{seed,index}},
                  ProposalIntent::Replicate,number*2+1,"","replicate_planner_v3",
                  "fresh matched discovery block"};
  return {std::move(a),std::move(b),((seed^number)&1ULL)==0};
 }
 std::uint64_t proposed_pairs()const noexcept{return next_number_;}
};
} // namespace ps::v3
