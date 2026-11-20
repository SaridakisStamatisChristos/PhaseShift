#pragma once
#include "phaseshift/experiment.hpp"
namespace ps {
struct GrowthFit {std::string model;double log_error=0;double factor=0;};
std::vector<GrowthFit> fit_growth_models(const std::vector<std::pair<double,double>>& size_runtime);
struct ScalingBoundary {
 bool candidate=false;
 std::size_t split=0; // first point in high-n regime
 double slope_left=0.0,slope_right=0.0,bic_gain=0.0;
 double left_n=0.0,right_n=0.0;
};
struct ScalingHoldoutVerdict {
 bool confirmed=false;
 double slope_difference=0,ci_lower=0,ci_upper=0,p_value=1;
 std::size_t independent_seed_pairs=0;
};
ScalingHoldoutVerdict evaluate_scaling_holdout(const std::vector<double>& differences,
 double discovery_slope_difference,std::uint64_t seed,double confidence=0.99,double q=0.05);
// Exploratory segmented log-log model against a single-slope null.
// The result never constitutes a theorem or independently confirmed transition.
ScalingBoundary detect_scaling_boundary(const std::vector<std::pair<double,double>>& samples,double min_slope_shift=0.6);
void scaling_study(const std::string& source,const std::string& out,int seeds=3);
}
