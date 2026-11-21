#pragma once
#include "phaseshift/stratified_v2.hpp"
#include <cmath>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <vector>

namespace ps {
// Matched-context policy. Geometry is used only for ordered axes; nominal
// levels receive conditional coverage without assuming a numeric ordering.
class HybridMixedPolicyV2 {
 using Axis=AdmissibleSearchSpace::Axis;
 using Scope=AdmissibleSearchSpace::Subspace;
 struct Sample{std::uint64_t index;std::optional<double> signal;double cpu_ms;};
 struct Gap{
  double score;std::string context;std::size_t left,right;
  bool operator<(const Gap& other)const{
   if(score!=other.score)return score<other.score;
   if(context!=other.context)return context>other.context;
   if(left!=other.left)return left>other.left;
   return right>other.right;
  }
 };
 AdmissibleSearchSpace space_;
 StratifiedIndexSamplerV2 stratified_;
 std::string primary_,origin_="hybrid_stratified";
 std::vector<std::string> labels_;
 std::uint64_t replicas_;
 double fraction_;
 bool ordered_;
 std::map<std::uint64_t,Sample> observed_;
 std::set<std::uint64_t> rejected_;
 std::map<std::string,std::map<std::size_t,Sample>> groups_;
 std::priority_queue<Gap> gaps_;
 std::optional<std::uint64_t> pending_replication_;
 static std::string encoded(const std::string& x){return std::to_string(x.size())+":"+x;}
 std::string context(const std::vector<std::pair<std::string,std::string>>& coords,
  std::uint64_t seed)const{
  std::string key=std::to_string(seed)+"/";
  for(const auto& [name,value]:coords)if(name!=primary_)key+=encoded(name)+encoded(value);
  return key;
 }
 void add_gap(const std::string& key,std::size_t a,const Sample& left,
  std::size_t b,const Sample& right){
  if(!ordered_||b<=a+1)return;
  const bool paired=left.signal.has_value()&&right.signal.has_value();
  const double contrast=paired?std::min(8.,std::abs(*left.signal-*right.signal)):0.;
  const double uncertainty=paired?0.2:1.0;
  const double cost=1.+0.5*(left.cpu_ms+right.cpu_ms);
  gaps_.push(Gap{(0.3+contrast+uncertainty)*std::log2(static_cast<double>(b-a)+1.)/
   std::sqrt(cost),key,a,b});
 }
 std::optional<std::uint64_t> ordered_target(){
  while(!gaps_.empty()){
   const auto gap=gaps_.top();gaps_.pop();
   const auto found=groups_.find(gap.context);
   if(found==groups_.end())continue;
   const auto& group=found->second;
   const auto a=group.find(gap.left),b=group.find(gap.right);
   if(a==group.end()||b==group.end()||std::next(a)!=b)continue;
   auto coords=space_.at(a->second.index/replicas_);
   auto axis=std::find_if(coords.begin(),coords.end(),
    [&](const auto& x){return x.first==primary_;});
   if(axis==coords.end())throw Error("hybrid v2 lost primary coordinate");
   const auto seed=a->second.index%replicas_;
   const auto mid=gap.left+(gap.right-gap.left)/2;
   const auto width=gap.right-gap.left;
   for(std::size_t delta=0;delta<width;++delta){
    const auto candidate_ordinal=(delta%2==0)?mid+delta/2:mid-(delta+1)/2;
    if(candidate_ordinal<=gap.left||candidate_ordinal>=gap.right)continue;
    axis->second=labels_[candidate_ordinal];
    const auto rank=space_.index_of(coords);
    if(!rank)continue;
    const auto candidate=*rank*replicas_+seed;
    if(!observed_.count(candidate)&&!rejected_.count(candidate))return candidate;
   }
  }
  return std::nullopt;
 }
 std::optional<std::uint64_t> categorical_target(){
  for(const auto& [key,group]:groups_){
   (void)key;
   if(group.empty())continue;
   auto coords=space_.at(group.begin()->second.index/replicas_);
   auto axis=std::find_if(coords.begin(),coords.end(),
    [&](const auto& x){return x.first==primary_;});
   if(axis==coords.end())throw Error("hybrid v2 lost nominal coordinate");
   const auto seed=group.begin()->second.index%replicas_;
   for(const auto& level:labels_){
    axis->second=level;
    const auto rank=space_.index_of(coords);
    if(!rank)continue;
    const auto candidate=*rank*replicas_+seed;
    if(!observed_.count(candidate)&&!rejected_.count(candidate))return candidate;
   }
  }
  return std::nullopt;
 }
 // Discovery-only adaptive replication: when a strong seed-matched
 // contrast appears, spend two measurements at the SAME endpoints on
 // a fresh shared seed. This is a heuristic, not calibrated uncertainty.
 std::optional<std::uint64_t> replication_target(){
  if(replicas_<2)return std::nullopt;
  double best_contrast=0.75;
  std::optional<std::pair<std::uint64_t,std::uint64_t>> best_pair;
  for(const auto& [context_key,group]:groups_){
   (void)context_key;
   if(group.size()<2)continue;
   for(auto left=group.begin(),right=std::next(left);
       right!=group.end();++left,++right){
    if(!left->second.signal||!right->second.signal)continue;
    const double contrast=std::abs(*left->second.signal-*right->second.signal);
    if(!(contrast>best_contrast))continue;
    const auto l_coordinate=left->second.index/replicas_;
    const auto r_coordinate=right->second.index/replicas_;
    for(std::uint64_t seed=0;seed<replicas_;++seed){
     const auto l=l_coordinate*replicas_+seed;
     const auto r=r_coordinate*replicas_+seed;
     if(observed_.count(l)||observed_.count(r)||
        rejected_.count(l)||rejected_.count(r))continue;
     best_contrast=contrast;
     best_pair=std::pair<std::uint64_t,std::uint64_t>{l,r};
     break;
    }
   }
  }
  if(!best_pair)return std::nullopt;
  pending_replication_=best_pair->second;
  return best_pair->first;
 }
 std::optional<std::uint64_t> baseline(){
  while(stratified_.has_next()){
   const auto index=stratified_.next();
   if(!observed_.count(index)&&!rejected_.count(index))return index;
  }
  return std::nullopt;
 }
public:
 HybridMixedPolicyV2(std::vector<Axis> axes,std::vector<Scope> scopes,
  const std::string& primary,std::uint64_t replicas,std::uint64_t seed,
  double exploration_fraction,bool ordered)
  :space_(axes,scopes),stratified_(axes,scopes,primary,replicas,seed),
   primary_(primary),replicas_(replicas),fraction_(exploration_fraction),ordered_(ordered){
  if(!std::isfinite(fraction_)||fraction_<0||fraction_>1||!replicas_||
    space_.cardinality()>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())/replicas_)
   throw Error("invalid hybrid v2 search configuration");
  for(const auto& [axis,values]:axes)if(axis==primary_)labels_=values;
  if(labels_.empty()||labels_.size()>4096)
   throw Error("hybrid v2 requires 1..4096 primary labels");
 }
 const std::string& origin()const noexcept{return origin_;}
 std::uint64_t rng_state()const noexcept{return stratified_.state();}
 void reject(std::uint64_t i){
  if(i>=space_.cardinality()*replicas_||observed_.count(i)||
     !rejected_.insert(i).second)throw Error("invalid hybrid rejected proposal");
  pending_replication_.reset(); // an invalid half cannot force a pairing
 }
 void observe(std::uint64_t i,std::optional<double> value,double cpu_ms){
  if(i>=space_.cardinality()*replicas_||rejected_.count(i)||
     !std::isfinite(cpu_ms)||cpu_ms<0||(value&&!std::isfinite(*value)))
   throw Error("invalid hybrid v2 observation");
  const Sample sample{i,value,std::min(1.e6,std::max(1.,cpu_ms))};
  if(!observed_.emplace(i,sample).second)throw Error("duplicate hybrid v2 observation");
  const auto coords=space_.at(i/replicas_);
  const auto axis=std::find_if(coords.begin(),coords.end(),
   [&](const auto& x){return x.first==primary_;});
  if(axis==coords.end())throw Error("hybrid v2 missing primary axis");
  const auto pos=std::find(labels_.begin(),labels_.end(),axis->second);
  if(pos==labels_.end())throw Error("hybrid v2 unexpected primary label");
  const auto ordinal=static_cast<std::size_t>(std::distance(labels_.begin(),pos));
  const auto key=context(coords,i%replicas_);
  auto& group=groups_[key];
  const auto inserted=group.emplace(ordinal,sample);
  if(!inserted.second)throw Error("duplicate hybrid v2 matched context");
  const auto right=std::next(inserted.first);
  if(inserted.first!=group.begin()){
   const auto left=std::prev(inserted.first);
   add_gap(key,left->first,left->second,ordinal,inserted.first->second);
  }
  if(right!=group.end())add_gap(key,ordinal,inserted.first->second,right->first,right->second);
 }
 std::optional<std::uint64_t> next(){
  if(observed_.size()+rejected_.size()==space_.cardinality()*replicas_)return std::nullopt;
  if(pending_replication_){
   const auto paired=*pending_replication_;
   pending_replication_.reset();
   if(!observed_.count(paired)&&!rejected_.count(paired)){
    origin_="hybrid_paired_replication";
    return paired;
   }
  }
  const auto n=observed_.size();
  // Bounded discovery allocation; this never replaces independent holdout.
  if(n>=4&&replicas_>1&&n%4==0){
   if(const auto candidate=replication_target()){
    origin_="hybrid_paired_replication";
    return candidate;
   }
  }
  const auto quota=[&](std::size_t count){
   return static_cast<std::size_t>(std::floor(static_cast<double>(count)*fraction_+1.e-9));
  };
  if(n>=4&&!(n>0&&quota(n)>quota(n-1))){
   const auto target=ordered_?ordered_target():categorical_target();
   if(target){
    origin_=ordered_?"hybrid_matched_interval":"hybrid_matched_category";
    return target;
   }
  }
  origin_="hybrid_stratified";return baseline();
 }
};
} // namespace ps
