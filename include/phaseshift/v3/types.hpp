#pragma once
// PhaseShift v3 opt-in contracts. No changes to v1/v2 Case identity or ABIs.
#include "phaseshift/lazy_search.hpp"
#include <cmath>
#include <compare>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ps::v3 {
using AxisValue = std::pair<std::string,std::string>;

struct StructuralKey {
 std::vector<AxisValue> coordinates;
 std::uint32_t generator_version=0;
 auto operator<=>(const StructuralKey&) const = default;
 void validate() const {
  if(!generator_version || coordinates.empty() || coordinates.size()>64)
   throw Error("v3 invalid structure cardinality or generator version");
  std::string prev;
  // Coordinate order is the *declared axis order*, not alphabetical.
  for(const auto& [axis,label]:coordinates) {
   if(axis.empty()||label.empty()||axis.size()>256||label.size()>2048)
    throw Error("v3 empty or oversized structural coordinate");
   if(!prev.empty() && axis==prev)
    throw Error("v3 consecutive duplicate axis");
   prev=axis;
  }
  for(std::size_t i=0;i<coordinates.size();++i)
   for(std::size_t j=i+1;j<coordinates.size();++j)
    if(coordinates[i].first==coordinates[j].first)
     throw Error("v3 duplicate structural axis");
 }
};

struct ReplicateKey {
 std::uint64_t seed=0;
 std::uint32_t replicate_index=0;
 auto operator<=>(const ReplicateKey&) const = default;
};

struct CandidateKey {
 StructuralKey structure;
 ReplicateKey replicate;
 auto operator<=>(const CandidateKey&) const = default;
};

enum class Cohort { Discovery, Confirmation };
enum class ProposalIntent {
 Explore, RefineBoundary, ValidateInteraction, ResolveNoise,
 DiagnoseFailure, Replicate, RandomFallback
};
struct ProposedTrial {
 CandidateKey key;
 ProposalIntent intent=ProposalIntent::Explore;
 std::uint64_t proposal_number=0;
 std::string model_hash;
 std::string policy_version;
 std::string provenance_reason;
};
struct ObservationSummary {
 CandidateKey key;
 std::string status;
 std::optional<double> paired_log_ratio;
 std::optional<double> robust_dispersion;
 std::uint64_t measured_child_cpu_ns=0;
 std::uint64_t parent_cpu_ns=0;
 std::uint64_t total_wall_ns=0;
 std::string provenance_digest;
};
struct BudgetSnapshot {
 std::uint64_t child_cpu_ns=0,parent_cpu_ns=0,host_wall_ns=0;
 std::uint64_t evaluated_cases=0,attempted_cases=0,invalid_cases=0;
 std::uint64_t hard_worker_mem_bytes=0;
};
struct DecisionContext {
 const AdmissibleSearchSpace& space;
 const class DiscoveryEvidenceSnapshot& discovery;
 BudgetSnapshot budget;
 std::uint64_t deterministic_epoch=0;
};
struct SearchPolicyV3 {
 virtual ~SearchPolicyV3()=default;
 virtual std::optional<ProposedTrial> propose(const DecisionContext&)=0;
 virtual void on_committed(const ObservationSummary&)=0;
 virtual std::string canonical_state_digest()const=0;
};
inline void validate_observation(const ObservationSummary& observation) {
 observation.key.structure.validate();
 if(observation.status.empty()||observation.status.size()>128)
  throw Error("v3 empty/oversized status");
 if(observation.status!="ok" && (observation.paired_log_ratio ||
                                 observation.robust_dispersion))
  throw Error("v3 failed worker has forbidden finite speed metric");
 if(observation.paired_log_ratio && !std::isfinite(*observation.paired_log_ratio))
  throw Error("v3 nonfinite paired metric");
 if(observation.robust_dispersion &&
    (!std::isfinite(*observation.robust_dispersion) ||
      *observation.robust_dispersion<0))
  throw Error("v3 invalid dispersion");
 if(!observation.total_wall_ns || observation.provenance_digest.empty())
  throw Error("v3 missing elapsed wall or provenance digest");
}
} // namespace ps::v3
