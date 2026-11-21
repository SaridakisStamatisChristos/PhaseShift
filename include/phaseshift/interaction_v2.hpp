#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace ps {
struct FactorialCellV2 {
 std::string first,second;
 std::uint64_t replicate_seed=0;
 std::optional<double> log_metric;
 std::string status="ok";
};
struct InteractionEvidenceV2 {
 std::string kind="INSUFFICIENT_EVIDENCE";
 double median_contrast=0;
 std::size_t complete_cohorts=0,incomplete_cohorts=0;
 std::string failure_status;
};
// Discovery-only 2x2 difference-in-differences or conditional status screen.
// Arbitrary labels remain nominal and retain their explicitly supplied order.
// Independent full seed cohorts are required; invalid statuses NEVER become
// "large runtime". This is NOT a calibrated p-value or causal intervention.
InteractionEvidenceV2 screen_factorial_v2(
 const std::array<std::string,2>& first,
 const std::array<std::string,2>& second,
 const std::vector<FactorialCellV2>& observations,
 std::size_t min_complete_cohorts,double practical_log_effect);
// A fixed-size, predeclared independent-seed 2x2 holdout. Computes a
// conservative one-sided exact sign test on seed-level DID contrasts under
// independent, sign-symmetric null cohorts. This is NOT a CONFIRMED verdict:
// the caller still needs a sealed campaign, multiplicity adjustment and
// linked raw workers/samples before any positive attestation.
struct FixedFactorialHoldoutV2 {
 std::string state="INCOMPLETE_HOLDOUT";
 double median_signed_effect=0.;
 double unadjusted_one_sided_p=1.;
 std::size_t tested_cohorts=0,consistent_sign_cohorts=0;
 bool supports_predeclared_effect=false;
};
FixedFactorialHoldoutV2 fixed_factorial_holdout_v2(
 const std::array<std::string,2>& first,
 const std::array<std::string,2>& second,
 const std::vector<FactorialCellV2>& independent_holdout,
 const std::vector<std::uint64_t>& discovery_seeds,
 std::size_t frozen_seed_cohorts,int predeclared_direction,
 double minimum_effect,double one_sided_alpha);
}
