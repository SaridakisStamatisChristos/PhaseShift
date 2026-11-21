#pragma once
#include <cstddef>
#include <optional>
#include <span>
#include <vector>
namespace ps {
struct ConstantSegmentV2 {std::size_t begin=0,end=0;double mean=0;};
struct MultiChangeFitV2 {
 std::vector<std::size_t> changes;
 std::vector<ConstantSegmentV2> segments;
 double residual_sse=0,penalized_sse=0;
};
// Exact finite-range piecewise-constant optimal partition with hard K bound.
// O(K*N*N) operations, O(K*N) auxiliary memory; N<=4096, K<=32.
// Per-change penalty is specified, not inferred as a confidence interval.
MultiChangeFitV2 fit_multi_change_v2(std::span<const double> values,
 std::size_t min_segment_size,std::size_t max_changes,double penalty);
// PELT with delayed pruning for minimum segment size; exact K-bounded DP
// fallback when the unconstrained optimum exceeds max_changes.
MultiChangeFitV2 fit_multi_change_pelt_v2(std::span<const double> values,
 std::size_t min_segment_size,std::size_t max_changes,double penalty);
}

// Optional discovery-only screen for an abrupt and persistent level change.
// It is NOT a significance test or a confidence interval. Requires 3 measured
// bins on each side; rejects short impulses and broad smooth transitions.
// Log-metric units and a predeclared log(practical effect ratio) are expected.
namespace ps {
struct PersistentJumpEvidenceV2 {
 double absolute_persistent_shift=0;
 double absolute_edge_change=0;
 double upper_quartile_background_change=0;
};
std::optional<PersistentJumpEvidenceV2> screen_persistent_jump_v2(
 std::span<const double> values,std::size_t cut,double minimum_log_effect);
}
