#pragma once
#include "phaseshift/lazy_search.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>
namespace ps {
// Stratifies on one primary-axis label. Samples within each stratum exactly
// without replacement, round-robins across strata, and never forms the product.
// This is NOT uniform across all pairs: small strata are deliberately favored.
class StratifiedIndexSamplerV2 {
 struct Slice {
  std::uint64_t block_start=0,end=0,radix=0,suffix=0,label_position=0;
 };
 struct Bucket {
  std::vector<Slice> slices;
  std::uint64_t count=0;
  std::optional<SparsePermutation> sampler;
 };
 std::vector<Bucket> buckets_;
 std::vector<std::size_t> order_;
 std::uint64_t replicas_=1,total_=0,drawn_=0,state_=0;
 std::size_t turn_=0;
public:
 StratifiedIndexSamplerV2(std::vector<AdmissibleSearchSpace::Axis> axes,
   std::vector<AdmissibleSearchSpace::Subspace> scopes,
   const std::string& axis,std::uint64_t replicas,std::uint64_t seed)
   :replicas_(replicas),state_(seed^0x70d2c93b24aeULL){
  const AdmissibleSearchSpace valid(axes,scopes);
  if(replicas==0||valid.cardinality()>
    static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())/replicas)
   throw Error("CARDINALITY_OVERFLOW: v2 stratified replicate universe");
  total_=valid.cardinality()*replicas;
  const auto found=std::find_if(axes.begin(),axes.end(),[&](const auto& p){return p.first==axis;});
  if(found==axes.end())throw Error("stratified v2 primary axis not found");
  const auto p=static_cast<std::size_t>(std::distance(axes.begin(),found));
  if(found->second.size()>100000)throw Error("stratified v2 primary axis cardinality limit");
  std::map<std::string,std::size_t> labels;
  buckets_.resize(found->second.size());
  for(std::size_t i=0;i<found->second.size();++i)labels.emplace(found->second[i],i);
  if(scopes.empty())scopes.emplace_back();
  std::uint64_t block_start=0;
  for(const auto& scope:scopes){
   auto restricted=axes;
   for(auto& [name,values]:restricted){
    const auto restriction=scope.find(name);
    if(restriction!=scope.end())values=restriction->second;
   }
   const LazySearchSpace block(restricted);
   const auto cardinality=block.cardinality();
   const auto radix=static_cast<std::uint64_t>(restricted[p].second.size());
   std::uint64_t suffix=1;
   for(std::size_t j=p+1;j<restricted.size();++j)
    suffix*=static_cast<std::uint64_t>(restricted[j].second.size());
   const auto slice_count=(cardinality/radix)*replicas_;
   for(std::size_t label_position=0;label_position<restricted[p].second.size();++label_position){
    auto& bucket=buckets_[labels.at(restricted[p].second[label_position])];
    bucket.count+=slice_count;
    bucket.slices.push_back(Slice{block_start,bucket.count,radix,suffix,label_position});
   }
   block_start+=cardinality;
  }
  if(block_start!=valid.cardinality())throw Error("v2 stratified union mapping mismatch");
  for(std::size_t i=0;i<buckets_.size();++i){
   auto& b=buckets_[i];
   if(b.count==0)continue;
   b.sampler.emplace(b.count,seed^(0x9e3779b97f4a7c15ULL*(i+1)));
   order_.push_back(i);
  }
  if(order_.empty())throw Error("empty v2 stratified search");
  for(std::size_t i=order_.size();i>1;--i){
   const auto j=static_cast<std::size_t>(randbelow(state_,i));
   std::swap(order_[i-1],order_[j]);
  }
 }
 bool has_next() const noexcept{return drawn_<total_;}
 std::uint64_t state() const noexcept{return state_;}
 std::uint64_t next(){
  if(!has_next())throw Error("v2 stratified candidate pool exhausted");
  for(std::size_t tries=0;tries<order_.size();++tries){
   auto& bucket=buckets_[order_[turn_]];
   turn_=(turn_+1)%order_.size();
   if(!bucket.sampler->has_next())continue;
   const auto rank=bucket.sampler->next();
   const auto it=std::lower_bound(bucket.slices.begin(),bucket.slices.end(),rank,
      [](const Slice& s,std::uint64_t r){return s.end<=r;});
   if(it==bucket.slices.end())throw Error("v2 stratum rank exceeded slice");
   const auto previous=it==bucket.slices.begin()?0:std::prev(it)->end;
   const auto offset=rank-previous;
   const auto coordinate_rank=offset/replicas_,replicate=offset%replicas_;
   const auto before=coordinate_rank/it->suffix,after=coordinate_rank%it->suffix;
   const auto block_rank=(before*it->radix+it->label_position)*it->suffix+after;
   const auto selected=(it->block_start+block_rank)*replicas_+replicate;
   ++drawn_;
   state_^=selected+0x3c79ac492ba7b653ULL;
   (void)splitmix64(state_);
   return selected;
  }
  throw Error("v2 stratified scheduler state exhausted prematurely");
 }
};
} // namespace ps
