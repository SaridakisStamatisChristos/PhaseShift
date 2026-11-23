#pragma once
// Experimental bounded V3-04 *conditional* matched-pair discovery portfolio.
// The only upstream evidence is committed discovery contrast feedback.
// This is a ranking policy, not a significance test or calibrated model.
#include "phaseshift/v3/types.hpp"
#include "phaseshift/v3/acquisition.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ps::v3 {
struct ConditionalPairChoice {
 std::vector<AxisValue> left,right;
 std::size_t context=0,edge=0;
 std::string arm,prior_evidence_digest;
 std::string acquisition_audit{};
 bool operator==(const ConditionalPairChoice&)const=default;
};
class ConditionalPairPortfolio {
 using Coords=std::vector<AxisValue>;
 struct Evidence {std::vector<double> valid;std::size_t censored=0;};
 struct Region {std::size_t lo=0,hi=0,depth=0;};
 struct Context {
  std::string key;
  std::vector<std::pair<std::size_t,std::uint64_t>> legal;
  std::unique_ptr<SparsePermutation> unseen;
  std::map<std::size_t,Evidence> observed;
  std::vector<Region> frontier; // disjoint unissued-edge intervals
  std::set<std::size_t> issued;  // bounded <= max_pairs
 };
 const AdmissibleSearchSpace& space_;
 std::vector<LazySearchSpace::Axis> axes_;
 std::size_t primary_=0,max_contexts_,max_pairs_,completed_=0;
 bool region_tree_=false,acquisition_=false,coverage_first_=false;
 EmpiricalCostModelV3 cost_model_;
 std::uint64_t seed_;
 double practical_log_ratio_;
 SparsePermutation global_;
 std::vector<Context> contexts_;
 std::map<std::string,std::size_t> by_context_;
 std::vector<Coords> strata_;
 std::size_t next_stratum_=0;
 std::optional<ConditionalPairChoice> pending_;
 static void append_u64(std::string& s,std::uint64_t value){
  for(unsigned shift=0;shift<64;shift+=8)s.push_back(static_cast<char>((value>>shift)&255));
 }
 static void append_str(std::string& s,const std::string& x) {
  append_u64(s,static_cast<std::uint64_t>(x.size()));s+=x;
 }
 static std::uint64_t stable_hash(const std::string& value){
  std::uint64_t h=14695981039346656037ULL;
  for(const unsigned char b:value){h^=b;h*=1099511628211ULL;}
  return h;
 }
 static double median(std::vector<double> values){
  if(values.empty())throw Error("v3 conditional empty evidence");
  std::sort(values.begin(),values.end());
  const auto n=values.size(),m=n/2;
  return n%2?values[m]:values[m-1]*0.5+values[m]*0.5;
 }
 std::string context_key(const Coords& coords)const{
  if(coords.size()!=axes_.size())throw Error("v3 conditional wrong coordinate count");
  std::string key;
  for(std::size_t i=0;i<axes_.size();++i) {
   if(coords[i].first!=axes_[i].first)throw Error("v3 conditional axis order differs");
   if(i==primary_)continue;
   append_str(key,coords[i].first);append_str(key,coords[i].second);
  }
  return key;
 }
 std::optional<std::size_t> enroll(const Coords& point){
  const auto key=context_key(point);
  if(const auto it=by_context_.find(key);it!=by_context_.end())
   return it->second;
  if(contexts_.size()>=max_contexts_)return std::nullopt;
  Context created;created.key=key;
  auto coords=point;
  for(std::size_t i=0;i<axes_[primary_].second.size();++i){
   coords[primary_].second=axes_[primary_].second[i];
   if(const auto found=space_.index_of(coords))
    created.legal.emplace_back(i,*found);
  }
  if(created.legal.size()<2)return std::nullopt; // singleton cannot form paired contrast
  const auto n=created.legal.size()-1;
  created.unseen=std::make_unique<SparsePermutation>(
      static_cast<std::uint64_t>(n),seed_^stable_hash(key)^0x2a50baf4ULL);
  if(region_tree_)created.frontier.push_back(Region{0,n-1,0});
  const auto id=contexts_.size();
  by_context_.emplace(key,id);
  contexts_.push_back(std::move(created));
  return id;
 }
 bool claim_region(std::size_t id,std::size_t edge){
  auto& c=contexts_.at(id);
  if(!c.issued.insert(edge).second)return false;
  for(std::size_t i=0;i<c.frontier.size();++i){
   const auto node=c.frontier[i];
   if(edge<node.lo||edge>node.hi)continue;
   c.frontier.erase(c.frontier.begin()+static_cast<std::ptrdiff_t>(i));
   if(node.lo<edge)c.frontier.push_back({node.lo,edge-1,node.depth+1});
   if(edge<node.hi)c.frontier.push_back({edge+1,node.hi,node.depth+1});
   return true;
  }
  throw Error("v3 region tree issued edge absent from legal frontier");
 }
 std::optional<ConditionalPairChoice> explore_at(const Coords& point){
  const auto id=enroll(point);
  if(!id)return std::nullopt;
  auto& c=contexts_[*id];
  // A region-tree proposal may have issued an edge without consuming its
  // sparse-permutation slot. Only unseen legal edges are admissible.
  while(c.unseen->has_next()){
   const auto edge=static_cast<std::size_t>(c.unseen->next());
   if(region_tree_&&!claim_region(*id,edge))continue;
   return choice(*id,edge,"explore");
  }
  return std::nullopt;
 }
 AcquisitionComponentsV3 components(std::size_t id,std::size_t edge,bool replicate)const{
  const auto& c=contexts_.at(id);
  AcquisitionComponentsV3 out;out.replication=replicate;
  const auto n=c.legal.size()-1;
  std::size_t nearest=n;
  double positive=0.,negative=0.;
  for(const auto& [other,obs]:c.observed){
   const auto distance=edge>other?edge-other:other-edge;
   nearest=std::min(nearest,distance);
   if(replicate&&other!=edge)continue;
   const double proximity=1./(1.+static_cast<double>(distance));
   if(!obs.valid.empty()){
    const double center=median(obs.valid);
    const double magnitude=std::min(1.,std::abs(center));
    out.contrast=std::max(out.contrast,magnitude*proximity);
    positive=std::max(positive,center>0.?magnitude:0.);
    negative=std::max(negative,center<0.?magnitude:0.);
    if(replicate){
     const double ambiguity=1./(1.+std::abs(std::abs(center)-practical_log_ratio_));
     out.uncertainty=ambiguity/std::sqrt(static_cast<double>(obs.valid.size()));
    }
   }
   if(obs.censored)out.status=std::max(out.status,proximity);
  }
  if(!replicate)out.uncertainty=1./(1.+static_cast<double>(c.observed.size()));
  out.diversity=replicate?0.:static_cast<double>(nearest)/static_cast<double>(n);
  // Rival sign evidence is a ranking proxy only, not an interaction estimate.
  out.interaction=std::min(positive,negative);
  return out;
 }
 double cost_rank(std::size_t id,std::size_t edge,bool replicate)const{
  return acquisition_score_v3(components(id,edge,replicate),cost_model_.estimate(id));
 }
 // Deterministic conditional interval frontier: select a legal, unseen
 // midpoint and bisect only that context. No Cartesian partition in memory.
 std::optional<ConditionalPairChoice> region_refinement(){
  struct Target {std::size_t id,edge;double score;};
  std::optional<Target> best;
  for(std::size_t id=0;id<contexts_.size();++id){
   const auto& c=contexts_[id];
   for(const auto& node:c.frontier){
    const auto width=node.hi-node.lo+1;
    const auto midpoint=node.lo+(node.hi-node.lo)/2;
    double maximum=0.;
    for(const auto& [edge,obs]:c.observed)
     if(edge>=node.lo&&edge<=node.hi&&!obs.valid.empty())
      maximum=std::max(maximum,std::min(4.,std::abs(median(obs.valid))));
    const double score=acquisition_?cost_rank(id,midpoint,false):
      static_cast<double>(width)*(1.+maximum)/(1.+static_cast<double>(node.depth));
    if(!best||score>best->score||
       (score==best->score&&std::pair{id,midpoint}<
                              std::pair{best->id,best->edge}))
     best=Target{id,midpoint,score};
   }
  }
  if(!best)return std::nullopt;
  if(!claim_region(best->id,best->edge))
   throw Error("v3 region tree repeated issued legal midpoint");
  return choice(best->id,best->edge,"explore");
 }
 ConditionalPairChoice choice(std::size_t id,std::size_t edge,
                              std::string arm)const {
  const auto& c=contexts_.at(id);
  if(edge+1>=c.legal.size())throw Error("v3 conditional edge out of range");
  return {space_.at(c.legal.at(edge).second),
          space_.at(c.legal.at(edge+1).second),id,edge,
          std::move(arm),""};
 }
 std::optional<ConditionalPairChoice> new_edge(){
  // Give each frozen disjoint legal rectangle one deterministic opportunity
  // before volume-weighted global exploration can obscure a rare stratum.
  while(next_stratum_<strata_.size()){
   const auto& origin=strata_.at(next_stratum_++);
   if(auto c=explore_at(origin))return c;
  }
  // Uniform-without-replacement global screening, bounded to prevent
  // unbounded rejection in a near-exhausted/degenerate conditional universe.
  for(std::size_t attempt=0;
      attempt<256&&contexts_.size()<max_contexts_&&global_.has_next();
      ++attempt) {
   if(auto c=explore_at(space_.at(global_.next())))return c;
  }
  for(std::size_t id=0;id<contexts_.size();++id)
   if(region_tree_&&!contexts_[id].frontier.empty()){
    const auto node=contexts_[id].frontier.front();
    const auto edge=node.lo+(node.hi-node.lo)/2;
    if(!claim_region(id,edge))throw Error("v3 region tree fallback repeated edge");
    return choice(id,edge,"explore");
   }
  for(std::size_t id=0;id<contexts_.size();++id)
   while(!region_tree_&&contexts_[id].unseen->has_next()){
    const auto edge=static_cast<std::size_t>(contexts_[id].unseen->next());
    return choice(id,edge,"explore");
   }
  return std::nullopt;
 }
 std::optional<std::pair<std::size_t,std::size_t>> replicate_target()const {
  double best=-1;
  std::optional<std::pair<std::size_t,std::size_t>> target;
  for(std::size_t id=0;id<contexts_.size();++id)
   for(const auto& [edge,obs]:contexts_[id].observed){
    if(obs.valid.empty())continue; // never promote censored/failed samples
    const double center=median(obs.valid);
    std::vector<double> errors;
    for(const double v:obs.valid)errors.push_back(std::abs(v-center));
    const double mad=obs.valid.size()>=3?1.4826*median(errors):0.;
    const double distance=std::abs(std::abs(center)-practical_log_ratio_);
    const double rank=acquisition_?cost_rank(id,edge,true):
      (1.+std::min(10.,mad))/((.10+distance)*std::sqrt(static_cast<double>(obs.valid.size())));
    if(!std::isfinite(rank))throw Error("v3 conditional nonfinite acquisition");
    const auto key=std::pair{id,edge};
    if(rank>best||(rank==best&&(!target||key<*target))){
     best=rank;target=key;
    }
   }
  return target;
 }
public:
 ConditionalPairPortfolio(const AdmissibleSearchSpace& space,
    std::vector<LazySearchSpace::Axis> axes,std::string primary,
    std::vector<AdmissibleSearchSpace::Subspace> declared_subspaces,
    std::uint64_t seed,std::size_t max_pairs,double min_ratio,
    std::size_t max_contexts=64,bool region_tree=false,bool acquisition=false,
    bool coverage_first=false)
 :space_(space),axes_(std::move(axes)),
  max_contexts_(max_contexts),max_pairs_(max_pairs),region_tree_(region_tree),acquisition_(acquisition),
  coverage_first_(coverage_first),seed_(seed),
  practical_log_ratio_(std::log(min_ratio)),
  global_(space.cardinality(),seed^0xacc705e4ULL){
  if(axes_.size()<2||axes_.size()>32||!max_contexts_||max_contexts_>128||
     !max_pairs_||max_pairs_>100 ||!std::isfinite(min_ratio) ||min_ratio<=1.)
   throw Error("v3 invalid bounded conditional portfolio contract");
  if(acquisition_&&!region_tree_)throw Error("v3 acquisition requires region tree");
  if(coverage_first_&&(!acquisition_||!region_tree_))
   throw Error("v3 coverage-first is an opt-in cost-aware region policy");
  if(declared_subspaces.size()>256)throw Error("v3 too many strata");
  const auto primary_it=std::find_if(axes_.begin(),axes_.end(),
   [&](const auto& axis){return axis.first==primary;});
  if(primary_it==axes_.end())throw Error("v3 ordered primary axis absent");
  primary_=static_cast<std::size_t>(std::distance(axes_.begin(),primary_it));
  if(primary_it->second.size()<2||primary_it->second.size()>4096)
   throw Error("v3 conditional primary axis cardinality outside 2..4096");
  std::int64_t prev=0;
  bool initial=true;
  for(const auto& label:primary_it->second){
   std::size_t n=0;std::int64_t v=0;
   try{v=std::stoll(label,&n);}
   catch(const std::exception&){throw Error("v3 conditional primary must be numeric");}
   if(n!=label.size()||(!initial&&v<=prev))
    throw Error("v3 conditional primary must be strictly ordered numeric");
   prev=v;initial=false;
  }
  for(const auto& scope:declared_subspaces){
   Coords origin;
   for(const auto& [axis,labels]:axes_){
    const auto it=scope.find(axis);
    if(labels.empty()||(it!=scope.end()&&it->second.empty()))
     throw Error("v3 conditional empty axis domain");
    origin.emplace_back(axis,it==scope.end()?labels.front():it->second.front());
   }
   if(!space_.index_of(origin))
    throw Error("v3 conditional declared stratum not in admissible universe");
   strata_.push_back(std::move(origin));
  }
 }
 std::string evidence_digest()const{
  std::string data=coverage_first_?"phaseshift.v3.coverage-cost-acquisition-pair.1":
                   acquisition_?"phaseshift.v3.cost-acquisition-pair.1":
                   region_tree_?"phaseshift.v3.region-tree-pair.1":
                                 "phaseshift.v3.conditional-pair.1";
  append_u64(data,completed_);append_u64(data,max_pairs_);
  append_u64(data,next_stratum_);append_u64(data,global_.state());
  append_u64(data,std::bit_cast<std::uint64_t>(practical_log_ratio_));
  append_u64(data,contexts_.size());
  for(const auto& c:contexts_){
   append_str(data,c.key);append_u64(data,c.unseen->state());
   append_u64(data,c.observed.size());
   for(const auto& [edge,obs]:c.observed){
    append_u64(data,edge);append_u64(data,obs.censored);
    append_u64(data,obs.valid.size());
    for(double d:obs.valid)append_u64(data,std::bit_cast<std::uint64_t>(d==0.?0.:d));
   }
   if(region_tree_){
    append_u64(data,c.issued.size());
    for(const auto edge:c.issued)append_u64(data,edge);
    append_u64(data,c.frontier.size());
    for(const auto& node:c.frontier){
     append_u64(data,node.lo);append_u64(data,node.hi);append_u64(data,node.depth);
    }
   }
  }
  if(acquisition_)append_str(data,cost_model_.canonical_state());
  return ps::sha256(data);
 }
 std::optional<ConditionalPairChoice> next(){
  if(pending_)throw Error("v3 conditional pending discovery lacks committed feedback");
  if(completed_>=max_pairs_)return std::nullopt;
  const auto prior=evidence_digest();
  const auto replicate=replicate_target();
  const bool explore=completed_<2||completed_%4==0||!replicate;
  std::optional<ConditionalPairChoice> candidate;
  if(region_tree_){
   // Every declared rare rectangle is screened *before* local refinement.
   // Thereafter the frozen 25% global reserve is never traded for a local fit.
   if(next_stratum_<strata_.size()||completed_<2||completed_%4==0)
    candidate=new_edge();
   else if(acquisition_&&!coverage_first_&&replicate&&completed_%5==3)
    candidate=choice(replicate->first,replicate->second,"replicate");
   else candidate=region_refinement();
   if(!candidate&&replicate)
    candidate=choice(replicate->first,replicate->second,"replicate");
   if(!candidate)candidate=new_edge();
  }else{
   candidate=explore?new_edge():std::optional<ConditionalPairChoice>{};
   if(!candidate&&replicate)
    candidate=choice(replicate->first,replicate->second,"replicate");
   if(!candidate&&!explore)
    candidate=new_edge();
  }
  if(!candidate)return std::nullopt;
  candidate->prior_evidence_digest=prior;
  if(acquisition_)candidate->acquisition_audit=acquisition_audit_v3(
     components(candidate->context,candidate->edge,candidate->arm=="replicate"),
     cost_model_.estimate(candidate->context));
  pending_=*candidate;
  return candidate;
 }
 void on_committed(const ConditionalPairChoice& c,Cohort cohort,
                   std::optional<double> contrast,std::optional<PairMeasuredCost> cost=std::nullopt){
  if(cohort!=Cohort::Discovery)
   throw Error("v3 sealed confirmation cannot update conditional portfolio");
  if(!pending_||*pending_!=c)
   throw Error("v3 conditional stale/forged pending decision feedback");
  if(contrast&&!std::isfinite(*contrast))
   throw Error("v3 nonfinite conditional matched contrast");
  if(acquisition_){
   if(!cost)throw Error("v3 acquisition requires actual committed cost");
   cost_model_.add(c.context,cohort,*cost,contrast.has_value());
  }
  auto& item=contexts_.at(c.context).observed[c.edge];
  if(contrast)item.valid.push_back(*contrast);
  else ++item.censored;
  ++completed_;pending_.reset();
 }
 std::size_t committed()const noexcept{return completed_;}
 std::size_t tracked_contexts()const noexcept{return contexts_.size();}
 std::size_t region_nodes()const noexcept{
  std::size_t total=0;for(const auto& c:contexts_)total+=c.frontier.size();
  return total;
 }
 std::size_t resident_global_entries()const noexcept{return global_.resident_entries();}
};
} // namespace ps::v3
