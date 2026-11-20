#pragma once
#include "phaseshift/core.hpp"
#include <cstddef>
namespace ps {
struct Interval {double point=0,lower=0,upper=0;std::size_t cases=0;};
double sample_median(std::vector<double> values);
/* Resampling unit: independent seed-matched CASE pair, never batch iterations.
   Percentile bootstrap for prespecified one-dimensional local contrast. */
Interval paired_case_bootstrap(const std::vector<double>& right_minus_left,
                               std::uint64_t seed,double confidence=0.99,
                               std::size_t replicates=20000);
Interval case_bootstrap(const std::vector<double>& values,
                        std::uint64_t seed,double confidence=0.99,
                        std::size_t replicates=20000);
/* Multiple hypotheses from a single prespecified family; adjusted BH-FDR. */
std::vector<bool> bh_fdr(const std::vector<double>& p_values,double q);
/* Holm's step-down family-wise error control, valid with arbitrarily dependent
   valid marginal p-values for one predeclared, fixed-size family.
   This does not correct adaptive selection or repeated holdout reuse. */
std::vector<bool> holm_fwer(const std::vector<double>& p_values,double alpha);
/* Exact/simulation sign-flip randomization on matched differences for a
   location-shift null. This p-value does not correct discovery selection. */
double paired_sign_flip_p(const std::vector<double>& differences,std::uint64_t seed,
                          std::size_t draws=20000);
}
