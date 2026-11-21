#pragma once
#include "phaseshift/lazy_search.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
namespace ps {
// Ranked interval search for one ordered structural axis. Its uncertainty
// score is a heuristic, NOT a calibrated posterior or significance guarantee.
class HierarchicalPolicyV2 {
 struct Summary{std::optional<double> metric;double cpu_ms=1;};
 std::size_t count_;double random_fraction_;SparsePermutation random_;
 std::map<std::size_t,Summary> observed_;std::string origin_="endpoint";
public:
 HierarchicalPolicyV2(std::size_t n,std::uint64_t seed,double fraction)
 :count_(n),random_fraction_(fraction),random_(n,seed){
  if(!n||!std::isfinite(fraction)||fraction<0||fraction>1)
   throw Error("invalid hierarchical v2 policy configuration");
 }
 const std::string& origin() const noexcept{return origin_;}
 std::uint64_t rng_state() const noexcept{return random_.state();}
 std::size_t evidence_size() const noexcept{return observed_.size();}
 void observe(std::size_t i,std::optional<double> metric,double cpu_ms){
  if(i>=count_||observed_.count(i)||!std::isfinite(cpu_ms)||cpu_ms<0||
    (metric&&!std::isfinite(*metric)))throw Error("invalid/duplicate hierarchical v2 evidence");
  observed_.emplace(i,Summary{metric,std::min(1.e6,std::max(1.,cpu_ms))});
 }
 std::optional<std::size_t> next(){
  if(observed_.size()==count_)return std::nullopt;
  if(observed_.empty()){origin_="left_endpoint";return 0;}
  if(observed_.size()==1&&!observed_.count(count_-1)){
   origin_="right_endpoint";return count_-1;
  }
  const auto samples=observed_.size();
  const auto quota=[&](std::size_t n){return static_cast<std::size_t>(std::floor(static_cast<double>(n)*random_fraction_+1e-9));};
  if(samples>=2&&quota(samples-1)>quota(samples-2)){
   origin_="random_exploration";return unseen_random();
  }
  double best=-1;std::size_t best_index=0;
  for(auto left=observed_.begin(),right=std::next(left);right!=observed_.end();++left,++right){
   if(right->first<=left->first+1)continue;
   const auto span=right->first-left->first-1;
   const bool both=left->second.metric.has_value()&&right->second.metric.has_value();
   const double contrast=both?std::min(2.,std::abs(*left->second.metric-*right->second.metric)):0.;
   const double uncertainty=left->second.metric.has_value()!=right->second.metric.has_value()?1.:0.2;
   const double cost=1.+0.5*(left->second.cpu_ms+right->second.cpu_ms);
   const double score=(0.2+contrast+uncertainty)*static_cast<double>(span)/std::sqrt(cost);
   if(score>best){best=score;best_index=left->first+(right->first-left->first)/2;}
  }
  if(best>=0){origin_="hierarchical_interval";return best_index;}
  origin_="random_fallback";return unseen_random();
 }
private:
 std::optional<std::size_t> unseen_random(){
  while(random_.has_next()){
   const auto index=static_cast<std::size_t>(random_.next());
   if(!observed_.count(index))return index;
  }
  return std::nullopt;
 }
};
}
