#pragma once
// The v3 extraction of the v2 selector: no change to v2 seed, case, or RNG semantics.
// Allows the experiment coordinator to own worker/SQLite transactions without
// owning policy factory and per-policy acquisition bookkeeping.
#include "phaseshift/experiment.hpp"
#include "phaseshift/stratified_v2.hpp"
#include "phaseshift/weighted_v2.hpp"
#include "phaseshift/scheduler_v2.hpp"
#include "phaseshift/hybrid_v2.hpp"
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ps::v3 {
class LegacyV2PolicyAdapter {
 SparsePermutation uniform_;
 std::optional<StratifiedIndexSamplerV2> stratified_;
 std::optional<WeightedIndexSamplerV2> weighted_;
 std::optional<HybridMixedPolicyV2> hybrid_;
 std::optional<HierarchicalPolicyV2> hierarchy_;
public:
 LegacyV2PolicyAdapter(const Config& cfg,const AdmissibleSearchSpace& space,
                       std::uint64_t replicas,bool primary_numeric)
 :uniform_(checked_virtual_count(space,replicas),cfg.seed^0xb7d2f937c4abULL) {
  const std::vector<LazySearchSpace::Axis> axes(cfg.axes.begin(),cfg.axes.end());
  if(cfg.method=="weighted_v2")
   weighted_.emplace(axes,cfg.admissible_subspaces,cfg.domain_priors,
                     replicas,cfg.seed^0x47c11bf25eb49a0dULL);
  if(cfg.method=="stratified_v2")
   stratified_.emplace(axes,cfg.admissible_subspaces,cfg.axis,replicas,
                      cfg.seed^0xb4ad19f14acULL);
  if(cfg.method=="hybrid_mixed_v2")
   hybrid_.emplace(axes,cfg.admissible_subspaces,cfg.axis,replicas,
                   cfg.seed^0x827d391bULL,cfg.exploration_fraction,primary_numeric);
  if(cfg.method=="hierarchical_boundary_v2"||cfg.method=="cliff_bracket_v2")
   hierarchy_.emplace(static_cast<std::size_t>(space.cardinality()),
                      cfg.seed^0xd9af2e13ULL,cfg.exploration_fraction,
                      cfg.method=="cliff_bracket_v2");
 }
 static std::uint64_t checked_virtual_count(const AdmissibleSearchSpace& space,
                                              std::uint64_t replicas) {
  if(!replicas || space.cardinality() >
     static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())/replicas)
   throw Error("CARDINALITY_OVERFLOW: v2 coordinate/replicate universe");
  return space.cardinality()*replicas;
 }
 std::uint64_t rng_state() const noexcept {
  return hybrid_?hybrid_->rng_state():
    (hierarchy_?hierarchy_->rng_state():
      (weighted_?weighted_->state():(stratified_?stratified_->state():uniform_.state())));
 }
 void observe(std::uint64_t index,std::optional<double> metric,double cpu_ms) {
  if(hierarchy_)hierarchy_->observe(static_cast<std::size_t>(index),metric,cpu_ms);
  if(hybrid_)hybrid_->observe(index,metric,cpu_ms);
 }
 // Returns an immutable legacy-format suffix or no policy event.
 std::optional<std::string> acquisition_origin() const {
  if(hierarchy_)return std::string("origin=")+hierarchy_->origin();
  if(hybrid_)return std::string("origin=")+hybrid_->origin();
  if(stratified_)return std::string("origin=stratified_primary_axis");
  if(weighted_)return std::string("origin=weighted_domain_prior");
  return std::nullopt;
 }
 // Validator rejects structurally illegal candidates without modifying source
 // evidence; it is invoked by the coordinator, which owns case construction.
 // Hierarchical v2 historically fails on illegal coordinates rather than
 // trying alternatives. Preserve that exact behavior.
 template<typename IsLegal>
 std::optional<std::uint64_t> next_index(IsLegal&& legal) {
  if(hybrid_) {
   for(std::size_t attempts=0;attempts<10000;++attempts) {
    auto selected=hybrid_->next();
    if(!selected)return std::nullopt;
    if(!legal(*selected)){hybrid_->reject(*selected);continue;}
    return selected;
   }
   throw Error("v2 hybrid exceeded deterministic invalid-proposal cap");
  }
  if(hierarchy_) {
   const auto selected=hierarchy_->next();
   if(!selected)return std::nullopt;
   return static_cast<std::uint64_t>(*selected);
  }
  for(std::size_t attempts=0;attempts<10000 &&
       (weighted_?weighted_->has_next():(stratified_?stratified_->has_next():uniform_.has_next()));
       ++attempts) {
   const auto selected=weighted_?weighted_->next():
     (stratified_?stratified_->next():uniform_.next());
   if(legal(selected))return selected;
  }
  if(weighted_?weighted_->has_next():(stratified_?stratified_->has_next():uniform_.has_next()))
   throw Error("v2 exceeded deterministic invalid-proposal cap");
  return std::nullopt;
 }
};
} // namespace ps::v3
