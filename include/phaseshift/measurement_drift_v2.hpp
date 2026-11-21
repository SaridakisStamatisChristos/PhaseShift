#pragma once
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
namespace ps {
struct DriftV2 {bool eligible=false, flagged=false;double log_ratio=0;};
// Descriptive early/late drift only; no p-values, correction or causal claims.
inline DriftV2 block_drift_v2(std::vector<std::pair<int,double>> observations){
 DriftV2 result;
 if(observations.size()<6)return result;
 std::sort(observations.begin(),observations.end());
 for(std::size_t i=0;i<observations.size();++i)
  if(!std::isfinite(observations[i].second)||observations[i].second<=0||
    (i&&observations[i].first==observations[i-1].first))return result;
 const std::size_t n=observations.size()/3;
 const auto median=[&](std::size_t begin,std::size_t end){
  std::vector<double> v;
  for(auto i=begin;i<end;++i)v.push_back(observations[i].second);
  std::sort(v.begin(),v.end());
  const auto middle=v.size()/2;
  return v.size()%2?v[middle]:(v[middle-1]+v[middle])/2.;
 };
 const double first=median(0,n);
 const double last=median(observations.size()-n,observations.size());
 result.log_ratio=std::log(last/first);
 result.eligible=std::isfinite(result.log_ratio);
 result.flagged=result.eligible&&std::abs(result.log_ratio)>=std::log(1.2);
 return result;
}
}
