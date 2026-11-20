#pragma once
#include "phaseshift/experiment.hpp"
namespace ps {
struct GrowthFit {std::string model;double log_error=0;double factor=0;};
std::vector<GrowthFit> fit_growth_models(const std::vector<std::pair<double,double>>& size_runtime);
void scaling_study(const std::string& source,const std::string& out,int seeds=3);
}
