#pragma once
#include "phaseshift/lazy_search.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <queue>
#include <iterator>
#include <string>
namespace ps {
// Ranked interval search for one ordered structural axis. Its uncertainty
// score is a heuristic, NOT a calibrated posterior or significance guarantee.
class HierarchicalPolicyV2 {
 struct Summary{std::optional<double> metric;double cpu_ms=1;};
 struct Gap{
  double score=0;
  std::size_t left=0,right=0;
  // Preserve the historical scan's first-left-wins tie rule exactly.
  bool operator<(const Gap& other)const noexcept{
   if(score!=other.score)return score<other.score;
   return left>other.left;
  }
 };
 std::size_t count_;double random_fraction_;SparsePermutation random_;
 bool prefer_contrast_=false;
 std::map<std::size_t,Summary> observed_;std::string origin_="endpoint";
 std::priority_queue<Gap> gaps_;
 void push_gap(std::map<std::size_t,Summary>::const_iterator left,
               std::map<std::size_t,Summary>::const_iterator right){
  if(right->first<=left->first+1)return;
  const auto span=right->first-left->first-1;
  const bool both=left->second.metric.has_value()&&right->second.metric.has_value();
  const double contrast=both?std::min(2.,std::abs(*left->second.metric-*right->second.metric)):0.;
  const double uncertainty=left->second.metric.has_value()!=right->second.metric.has_value()?1.:0.2;
  const double cost=1.+0.5*(left->second.cpu_ms+right->second.cpu_ms);
  // Existing hierarchical_v2 score is immutable for replay compatibility.
  // Opt-in cliff bracketing instead prefers measured opposite-side contrasts
  // and uses width only to break ambiguity or search yet-unproven regions.
  // This is an acquisition heuristic, not a calibrated jump probability.
  const double score=prefer_contrast_?
   contrast+static_cast<double>(span)/static_cast<double>(count_):
   (0.2+contrast+uncertainty)*static_cast<double>(span)/std::sqrt(cost);
  gaps_.push(Gap{score,left->first,right->first});
 }
public:
 HierarchicalPolicyV2(std::size_t n,std::uint64_t seed,double fraction,
                      bool prefer_cliff_contrast=false)
 :count_(n),random_fraction_(fraction),random_(n,seed),
  prefer_contrast_(prefer_cliff_contrast){
  if(!n||!std::isfinite(fraction)||fraction<0||fraction>1)
   throw Error("invalid hierarchical v2 policy configuration");
 }
 const std::string& origin() const noexcept{return origin_;}
 std::uint64_t rng_state() const noexcept{return random_.state();}
 std::size_t evidence_size() const noexcept{return observed_.size();}
 void observe(std::size_t i,std::optional<double> metric,double cpu_ms){
  if(i>=count_||observed_.count(i)||!std::isfinite(cpu_ms)||cpu_ms<0||
    (metric&&!std::isfinite(*metric)))throw Error("invalid/duplicate hierarchical v2 evidence");
  const auto inserted=observed_.emplace(i,Summary{metric,std::min(1.e6,std::max(1.,cpu_ms))});
  const auto right=std::next(inserted.first);
  if(inserted.first!=observed_.begin())
   push_gap(std::prev(inserted.first),inserted.first);
  if(right!=observed_.end())
   push_gap(inserted.first,right);
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
  // Lazy-invalidated interval heap: O(log n) insertion/update, no global
  // O(n) per-proposal scan. Stale gaps are discarded only after a commit.
  while(!gaps_.empty()){
   const auto gap=gaps_.top();
   const auto left=observed_.find(gap.left);
   const auto right=observed_.find(gap.right);
   if(left==observed_.end()||right==observed_.end()||
      std::next(left)!=right){
    gaps_.pop();continue;
   }
   origin_=prefer_contrast_?"cliff_bracket_interval":"hierarchical_interval";
   return gap.left+(gap.right-gap.left)/2;
  }
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
