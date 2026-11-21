#pragma once
// V3 opt-in legal ordered-axis slices. This module cannot read confirmation
// evidence, allocate an eager Cartesian grid or return illegal coordinates.
// It is not a probabilistic/confirmed boundary inference engine.
#include "phaseshift/v3/types.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ps::v3 {
struct AxisSliceProposal {
 std::uint64_t virtual_index=0;
 std::string origin;
 // Heuristic scheduling diagnostic, NOT a p-value/posterior.
 double ranking_score=0;
};
class AxisSlicePolicyV3 {
 using Coordinates=std::vector<std::pair<std::string,std::string>>;
 struct Sample {std::optional<double> metric;double cost;};
 struct Slice {
  std::string context;
  std::vector<std::pair<std::size_t,std::uint64_t>> legal;
  std::map<std::size_t,Sample> observed;
 };
 struct Pending {std::size_t slice=0,primary_position=0;bool indexed=false;};
 const AdmissibleSearchSpace& space_;
 std::vector<LazySearchSpace::Axis> axes_;
 std::size_t primary_=0,max_contexts_=0,max_proposals_=0;
 std::uint64_t proposals_=0;
 SparsePermutation exploration_;
 std::vector<Slice> slices_;
 std::map<std::string,std::size_t> slice_by_context_;
 std::set<std::uint64_t> issued_;
 std::map<std::uint64_t,Pending> pending_;
 static std::string encode_context(const Coordinates& coords,std::size_t primary) {
  std::string key;
  for(std::size_t i=0;i<coords.size();++i)if(i!=primary) {
   const auto& [axis,value]=coords[i];
   key+=std::to_string(axis.size())+":"+axis;
   key+=std::to_string(value.size())+":"+value;
  }
  return key;
 }
 std::optional<std::pair<std::size_t,std::size_t>> enroll(std::uint64_t index) {
  auto coordinates=space_.at(index);
  const auto label=std::find(axes_[primary_].second.begin(),
                             axes_[primary_].second.end(),coordinates[primary_].second);
  if(label==axes_[primary_].second.end())
   throw Error("v3 selected primary coordinate not in declared labels");
  const auto position=static_cast<std::size_t>(std::distance(axes_[primary_].second.begin(),label));
  const auto key=encode_context(coordinates,primary_);
  if(auto found=slice_by_context_.find(key);found!=slice_by_context_.end())
   return std::pair<std::size_t,std::size_t>{found->second,position};
  if(slices_.size()>=max_contexts_)return std::nullopt; // explicit global fallback
  Slice slice;slice.context=key;
  for(std::size_t pos=0;pos<axes_[primary_].second.size();++pos) {
   coordinates[primary_].second=axes_[primary_].second[pos];
   if(const auto legal=space_.index_of(coordinates))
    slice.legal.emplace_back(pos,*legal);
  }
  if(slice.legal.empty())
   throw Error("v3 context enrollment lacked original legal coordinate");
  const auto slice_id=slices_.size();
  slice_by_context_.emplace(key,slice_id);
  slices_.push_back(std::move(slice));
  return std::pair<std::size_t,std::size_t>{slice_id,position};
 }
 AxisSliceProposal issue(std::uint64_t index,std::string origin,double score) {
  if(index>=space_.cardinality() || issued_.count(index))
   throw Error("v3 invalid or duplicate selected legal index");
  const auto location=enroll(index);
  issued_.insert(index);
  if(location)
   pending_.emplace(index,Pending{location->first,location->second,true});
  else
   pending_.emplace(index,Pending{}); // bounded global screening only
  ++proposals_;
  return {index,std::move(origin),score};
 }
 std::optional<AxisSliceProposal> global() {
  // Bounded deterministic rejection, not unbounded virtual enumeration.
  for(std::size_t tries=0;tries<10000 && exploration_.has_next();++tries) {
   const auto index=exploration_.next();
   if(!issued_.count(index))return issue(index,"legal_global_exploration",0.);
  }
  if(exploration_.has_next())
   throw Error("v3 global fallback reached bounded-proposal complexity cap");
  return std::nullopt;
 }
 struct Target {std::uint64_t index;double score;std::size_t slice;std::size_t position;};
 std::optional<Target> best_refinement() const {
  std::optional<Target> best;
  const auto nominate=[&](std::size_t slice,std::size_t pos,std::uint64_t index,double score){
   if(issued_.count(index))return;
   if(!std::isfinite(score)||score<0)throw Error("v3 invalid acquisition score");
   if(!best||score>best->score||
      (score==best->score && index<best->index))
    best=Target{index,score,slice,pos};
  };
  for(std::size_t s=0;s<slices_.size();++s) {
   const auto& slice=slices_[s];
   if(slice.observed.empty())continue;
   if(slice.legal.size()<=1)continue;
   // Endpoints first; this never orders *categorical* dimensions.
   const auto& front=slice.legal.front();
   const auto& back=slice.legal.back();
   if(!slice.observed.count(front.first))
    nominate(s,front.first,front.second,2.);
   if(!slice.observed.count(back.first))
    nominate(s,back.first,back.second,2.);
   // Adjacent discovered ordered positions delimit legal candidate gaps.
   for(auto left=slice.observed.begin();left!=slice.observed.end();) {
    auto right=std::next(left);
    if(right==slice.observed.end())break;
    if(right->first>left->first+1) {
     const std::size_t mid=left->first+(right->first-left->first)/2;
     auto nearest=std::lower_bound(slice.legal.begin(),slice.legal.end(),mid,
       [](const auto& candidate,std::size_t value){return candidate.first<value;});
     // Examine both adjacent legal values and skip issued points.
     const auto inspect=[&](decltype(nearest) cand){
      if(cand==slice.legal.end()||cand->first<=left->first||cand->first>=right->first)return;
      const auto contrast=(left->second.metric&&right->second.metric)?
       std::min(4.0,std::abs(*left->second.metric-*right->second.metric)):0.;
      const auto span=static_cast<double>(right->first-left->first);
      const auto distance=std::abs(static_cast<double>(cand->first)-static_cast<double>(mid));
      const auto benefit=(1.+contrast)*span/
       (1.+distance)/std::sqrt(1.+left->second.cost+right->second.cost);
      nominate(s,cand->first,cand->second,benefit);
     };
     inspect(nearest);
     if(nearest!=slice.legal.begin())inspect(std::prev(nearest));
    }
    left=right;
   }
  }
  return best;
 }
public:
 AxisSlicePolicyV3(const AdmissibleSearchSpace& space,
    std::vector<LazySearchSpace::Axis> axes,const std::string& ordered_primary,
    std::uint64_t seed,std::size_t max_contexts=64,
    std::size_t max_proposals=20000)
  :space_(space),axes_(std::move(axes)),max_contexts_(max_contexts),
   max_proposals_(max_proposals),exploration_(space.cardinality(),seed^0xd51d8420ULL) {
  if(!max_contexts_||max_contexts_>512||!max_proposals_||max_proposals_>20000)
   throw Error("v3 invalid finite model caps");
  const auto found=std::find_if(axes_.begin(),axes_.end(),
    [&](const auto& axis){return axis.first==ordered_primary;});
  if(found==axes_.end()||found->second.empty()||found->second.size()>4096||
     space_.cardinality()==0)
   throw Error("v3 ordered axis missing or beyond bounded split cap");
  primary_=static_cast<std::size_t>(std::distance(axes_.begin(),found));
  if(axes_.size()>64)throw Error("v3 axis-count cap");
  // Caller guarantees ordered numeric primary labels; categorical labels
  // are *never* compared/ranked by this policy.
  for(const auto& [name,labels]:axes_)
   if(name.empty()||labels.empty())
    throw Error("v3 invalid axis label domain");
 }
 std::optional<AxisSliceProposal> next() {
  if(proposals_>=max_proposals_)return std::nullopt;
  // Every fourth trial reserves a global legal candidate for sparse/rare
  // categories even when a strong existing ordered interval exists.
  if(proposals_%4==0||slices_.empty())return global();
  if(auto target=best_refinement())
   return issue(target->index,"ordered_slice_refinement",target->score);
  return global();
 }
 void on_committed(std::uint64_t index,Cohort cohort,std::optional<double> metric,
                   double observed_cost_ms) {
  if(cohort!=Cohort::Discovery)
   throw Error("v3 sealed holdout cannot feed acquisition model");
  if((metric&&!std::isfinite(*metric))||
     !std::isfinite(observed_cost_ms)||observed_cost_ms<0)
   throw Error("v3 invalid committed-only policy observation");
  const auto found=pending_.find(index);
  if(found==pending_.end())throw Error("v3 unissued/duplicate committed observation");
  if(found->second.indexed) {
   auto& slice=slices_.at(found->second.slice);
   if(!slice.observed.emplace(found->second.primary_position,
       Sample{metric,observed_cost_ms}).second)
    throw Error("v3 duplicate primary coordinate in distinct proposed structure");
  }
  pending_.erase(found);
 }
 std::size_t tracked_contexts()const noexcept{return slices_.size();}
 std::size_t issued_count()const noexcept{return issued_.size();}
 std::size_t pending_count()const noexcept{return pending_.size();}
 std::uint64_t global_rng_state()const noexcept{return exploration_.state();}
};
} // namespace ps::v3
