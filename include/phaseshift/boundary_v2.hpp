#pragma once
#include <cstddef>
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
}
