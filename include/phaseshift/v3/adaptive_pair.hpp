#pragma once
// Experimental v3 one-axis discovery allocation. No worker, SQLite or holdout access.
// Ranking uncertainty is heuristic, never a posterior or a p-value.
#include "phaseshift/v3/types.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace ps::v3 {
struct AdaptivePairChoice {
 std::size_t edge=0;
 std::string left_label,right_label,arm,prior_evidence_digest;
 bool operator==(const AdaptivePairChoice&) const = default;
};
struct AdaptiveEdgeSummary {
 std::size_t valid_seeds=0,censored_seeds=0;
 std::optional<double> median_contrast,robust_mad;
};
class AdaptivePairPolicy {
 struct Edge {std::vector<double> contrasts;std::size_t censored=0;};
 std::vector<std::string> labels_;
 std::map<std::size_t,Edge> evidence_;
 SparsePermutation untested_;
 std::optional<AdaptivePairChoice> pending_;
 std::size_t committed_=0,max_pairs_=0;
 double practical_log_ratio_=0.0;
 static void append_int(std::string& data,std::uint64_t n) {
  for(unsigned shift=0;shift<64;shift+=8)
   data.push_back(static_cast<char>((n>>shift)&255U));
 }
 static void append_string(std::string& data,const std::string& s) {
  append_int(data,static_cast<std::uint64_t>(s.size()));data+=s;
 }
 static double median(std::vector<double> x) {
  if(x.empty())throw Error("v3 empty uncertainty statistics");
  std::sort(x.begin(),x.end());
  const std::size_t m=x.size()/2;
  return x.size()%2?x[m]:x[m-1]*0.5+x[m]*0.5;
 }
 std::optional<std::size_t> best_replicate() const {
  double best=-1;
  std::optional<std::size_t> target;
  for(const auto& [edge,observed]:evidence_) {
   if(observed.contrasts.empty())continue; // never promote failed status
   const auto center=median(observed.contrasts);
   std::vector<double> deviations;
   deviations.reserve(observed.contrasts.size());
   for(const double x:observed.contrasts)
    deviations.push_back(std::abs(x-center));
   // With fewer than three independent seeds MAD is not a noise estimate.
   // This is deliberately only a *ranking diagnostic*, not calibrated.
   const double dispersion=observed.contrasts.size()>=3?
     1.4826*median(deviations):0.0;
   const auto ambiguity=std::abs(std::abs(center)-practical_log_ratio_);
   const auto value=(1.+std::min(10.,dispersion))/
     ((.10+ambiguity)*std::sqrt(static_cast<double>(observed.contrasts.size())));
   if(!std::isfinite(value))throw Error("v3 invalid adaptive acquisition score");
   if(value>best || (value==best && (!target || edge<*target))) {
    best=value;target=edge;
   }
  }
  return target;
 }
public:
 AdaptivePairPolicy(std::vector<std::string> legal_ordered_labels,
                    std::uint64_t policy_seed,std::size_t max_pairs,
                    double min_practical_ratio)
  :labels_(std::move(legal_ordered_labels)),
   untested_(labels_.size()>1?labels_.size()-1:0,policy_seed^0x6144650aULL),
   max_pairs_(max_pairs),practical_log_ratio_(std::log(min_practical_ratio)){
  if(labels_.size()<3||labels_.size()>4096||
     std::set<std::string>(labels_.begin(),labels_.end()).size()!=labels_.size())
   throw Error("v3 adaptive policy requires 3..4096 distinct legal ordered labels");
  if(!max_pairs_||max_pairs_>100||
     !(min_practical_ratio>1.)||!std::isfinite(min_practical_ratio))
   throw Error("v3 invalid bounded adaptive discovery contract");
  for(const auto& label:labels_)
   if(label.empty()||label.size()>2048)throw Error("v3 invalid structural axis label");
 }
 std::string evidence_digest() const {
  std::string bytes="phaseshift.v3.adaptive-pair-policy.1";
  append_int(bytes,static_cast<std::uint64_t>(max_pairs_));
  append_int(bytes,static_cast<std::uint64_t>(committed_));
  append_int(bytes,untested_.state());
  append_int(bytes,std::bit_cast<std::uint64_t>(practical_log_ratio_));
  append_int(bytes,static_cast<std::uint64_t>(labels_.size()));
  for(const auto& label:labels_)append_string(bytes,label);
  append_int(bytes,static_cast<std::uint64_t>(evidence_.size()));
  for(const auto& [edge,data]:evidence_) {
   append_int(bytes,static_cast<std::uint64_t>(edge));
   append_int(bytes,static_cast<std::uint64_t>(data.censored));
   append_int(bytes,static_cast<std::uint64_t>(data.contrasts.size()));
   for(double v:data.contrasts)
    append_int(bytes,std::bit_cast<std::uint64_t>(v==0.0?0.0:v));
  }
  return ps::sha256(bytes);
 }
 std::optional<AdaptivePairChoice> next() {
  if(pending_)throw Error("v3 adaptive decision lacks committed feedback");
  if(committed_>=max_pairs_)return std::nullopt;
  const auto prior=evidence_digest();
  const auto replicated=best_replicate();
  const bool explore=untested_.has_next()&&
    (committed_<2 || committed_%4==0 || !replicated);
  std::size_t edge=0;
  std::string arm;
  if(explore){edge=static_cast<std::size_t>(untested_.next());arm="explore";}
  else if(replicated){edge=*replicated;arm="replicate";}
  else if(untested_.has_next()){edge=static_cast<std::size_t>(untested_.next());arm="explore";}
  else return std::nullopt;
  pending_=AdaptivePairChoice{edge,labels_.at(edge),labels_.at(edge+1),
                              std::move(arm),prior};
  return pending_;
 }
 void on_committed(const AdaptivePairChoice& choice,Cohort cohort,
                   std::optional<double> paired_log_contrast) {
  if(cohort!=Cohort::Discovery)
   throw Error("v3 sealed confirmation cannot update adaptive discovery policy");
  if(!pending_||choice!=*pending_)
   throw Error("v3 adaptive feedback absent or does not match proposed decision");
  if(paired_log_contrast && !std::isfinite(*paired_log_contrast))
   throw Error("v3 nonfinite adaptive discovery contrast");
  auto& edge=evidence_[choice.edge];
  if(paired_log_contrast)edge.contrasts.push_back(*paired_log_contrast);
  else ++edge.censored;
  ++committed_;
  pending_.reset();
 }
 AdaptiveEdgeSummary summary(std::size_t edge) const {
  if(edge+1>=labels_.size())throw Error("v3 invalid adaptive edge");
  AdaptiveEdgeSummary result;
  const auto found=evidence_.find(edge);
  if(found==evidence_.end())return result;
  result.valid_seeds=found->second.contrasts.size();
  result.censored_seeds=found->second.censored;
  if(result.valid_seeds){
   const auto center=median(found->second.contrasts);
   result.median_contrast=center;
   if(result.valid_seeds>=3){
    std::vector<double> errors;
    for(double x:found->second.contrasts)errors.push_back(std::abs(x-center));
    result.robust_mad=1.4826*median(std::move(errors));
   }
  }
  return result;
 }
 std::size_t committed_pairs()const noexcept{return committed_;}
 std::size_t edges_total()const noexcept{return labels_.size()-1;}
};
} // namespace ps::v3
