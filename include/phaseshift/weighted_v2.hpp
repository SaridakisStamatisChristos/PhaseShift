#pragma once
#include "phaseshift/lazy_search.hpp"
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace ps {
// Product-of-axis-priors weighted draws WITHOUT replacement across disjoint
// admissible rectangles. An indexed sparse prefix ledger removes only drawn
// leaf mass: O(draws * dimensions) retained prefix entries, never O(product).
// Long-double precision is checked: tiny unrepresentable residual mass fails
// closed rather than silently turning weighted sampling into uniform sampling.
class WeightedIndexSamplerV2 {
public:
 using Axis=AdmissibleSearchSpace::Axis;
 using Scope=AdmissibleSearchSpace::Subspace;
 using Priors=std::map<std::string,std::map<std::string,double>>;
private:
 struct Block {
  LazySearchSpace space;
  std::vector<std::vector<long double>> probabilities;
  long double mass=1;
  std::uint64_t base=0;
 };
 struct Prefix {
  std::size_t block=0,depth=0;
  std::uint64_t rank=0;
  bool operator<(const Prefix& o)const noexcept {
   return std::tie(block,depth,rank)<std::tie(o.block,o.depth,o.rank);
  }
 };
 std::vector<Block> blocks_;
 std::map<Prefix,long double> removed_;
 std::set<std::uint64_t> used_;
 std::uint64_t replicas_=1,total_=0,drawn_=0,state_=0;

 long double consumed(const Prefix& key)const {
  const auto it=removed_.find(key);
  return it==removed_.end()?0.L:it->second;
 }
 static long double available(long double whole,long double used) {
  if(!std::isfinite(whole)||!std::isfinite(used)||whole<0||used<0)
   throw Error("v2 weighted sampler invalid mass");
  return std::max(0.L,whole-used);
 }
 long double uniform_unit(){
  constexpr long double denom=18446744073709551616.L;
  return static_cast<long double>(splitmix64(state_))/denom;
 }
 // Use one stable draw for the entire level, then retain the last positive
 // fallback for rounding at the upper endpoint. No artificial category order.
 std::size_t choose(const std::vector<long double>& weights){
  long double total=0;
  for(const auto v:weights)if(v>0)total+=v;
  if(!(total>0)||!std::isfinite(total))
   throw Error("v2 weighted prior mass exhausted or below numeric resolution");
  long double target=uniform_unit()*total;
  std::size_t last=weights.size();
  for(std::size_t i=0;i<weights.size();++i){
   if(!(weights[i]>0))continue;
   last=i;
   if(target<weights[i])return i;
   target-=weights[i];
  }
  if(last==weights.size())throw Error("v2 weighted selection has no positive branch");
  return last;
 }
public:
 WeightedIndexSamplerV2(std::vector<Axis> axes,std::vector<Scope> scopes,
   const Priors& priors,std::uint64_t replicas,std::uint64_t seed)
   :replicas_(replicas),state_(seed^0x9824a34b4edbed73ULL){
  const AdmissibleSearchSpace admissible(axes,scopes);
  if(replicas==0||admissible.cardinality()>
     static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())/replicas)
   throw Error("CARDINALITY_OVERFLOW: weighted v2 replicate space");
  total_=admissible.cardinality()*replicas;
  if(priors.empty())throw Error("weighted_v2 requires explicit domain_priors");
  for(const auto& [name,labels]:priors){
   const auto it=std::find_if(axes.begin(),axes.end(),[&](const Axis& a){
    return a.first==name;
   });
   if(it==axes.end())throw Error("unknown weighted v2 prior axis: "+name);
   for(const auto& [label,value]:labels){
    if(std::find(it->second.begin(),it->second.end(),label)==it->second.end())
     throw Error("unknown weighted v2 prior label: "+name+"="+label);
    if(!std::isfinite(value)||value<=0||value>1.e6||value<1.e-6)
     throw Error("v2 prior weight must be finite and in [1e-6,1e6]");
   }
  }
  if(scopes.empty())scopes.emplace_back();
  // Normalize over the full axis labels first. Subspace masses then represent
  // genuine probability mass, even when admissible rectangles differ in size.
  std::map<std::string,std::map<std::string,long double>> full;
  for(const auto& [name,labels]:axes){
   long double sum=0;
   for(const auto& label:labels){
    const auto a=priors.find(name);
    const auto v=a==priors.end()?1.:
       (a->second.count(label)?a->second.at(label):1.);
    full[name][label]=static_cast<long double>(v);
    sum+=static_cast<long double>(v);
   }
   if(!(sum>0)||!std::isfinite(sum))throw Error("v2 prior axis sum invalid");
   for(auto& [label,p]:full[name]){(void)label;p/=sum;}
  }
  std::uint64_t base=0;
  for(const auto& scope:scopes){
   auto restricted=axes;
   for(auto& [name,labels]:restricted){
    const auto it=scope.find(name);
    if(it!=scope.end())labels=it->second;
   }
   Block block{LazySearchSpace(restricted),{},1.L,base};
   for(const auto& [name,labels]:restricted){
    std::vector<long double> q;
    long double sum=0;
    for(const auto& label:labels){
     const auto p=full.at(name).at(label);
     q.push_back(p);sum+=p;
    }
    if(!(sum>0))throw Error("v2 admissible prior mass underflow");
    block.mass*=sum;
    for(auto& p:q)p/=sum;
    block.probabilities.push_back(std::move(q));
   }
   if(!(block.mass>0)||!std::isfinite(block.mass))
    throw Error("v2 weighted rectangle prior mass underflow");
   base+=block.space.cardinality();
   blocks_.push_back(std::move(block));
  }
  if(base!=admissible.cardinality())throw Error("v2 weighted union index mismatch");
 }
 bool has_next()const noexcept{return drawn_<total_;}
 std::uint64_t state()const noexcept{return state_;}
 std::size_t resident_entries()const noexcept{return removed_.size()+used_.size();}
 std::uint64_t next(){
  if(!has_next())throw Error("v2 weighted candidate pool exhausted");
  std::vector<long double> block_weights;
  block_weights.reserve(blocks_.size());
  for(std::size_t b=0;b<blocks_.size();++b)
   block_weights.push_back(available(blocks_[b].mass,consumed({b,0,0})));
  const auto selected_block=choose(block_weights);
  const auto& block=blocks_[selected_block];
  long double mass=block.mass;
  std::uint64_t rank=0;
  std::vector<Prefix> path{{selected_block,0,0}};
  for(std::size_t i=0;i<block.probabilities.size();++i){
   const auto& prior=block.probabilities[i];
   std::vector<long double> weights;
   weights.reserve(prior.size());
   for(std::size_t j=0;j<prior.size();++j){
    const auto next_rank=rank*prior.size()+j;
    weights.push_back(available(mass*prior[j],
      consumed({selected_block,i+1,next_rank})));
   }
   const auto choice=choose(weights);
   mass*=prior[choice];
   rank=rank*prior.size()+choice;
   path.push_back({selected_block,i+1,rank});
  }
  std::vector<long double> seeds;
  seeds.reserve(static_cast<std::size_t>(replicas_));
  for(std::uint64_t i=0;i<replicas_;++i)
   seeds.push_back(available(mass/static_cast<long double>(replicas_),
     consumed({selected_block,block.probabilities.size()+1,
       rank*replicas_+i})));
  const auto chosen_seed=choose(seeds);
  const auto full_rank=rank*replicas_+chosen_seed;
  path.push_back({selected_block,block.probabilities.size()+1,full_rank});
  const auto virtual_index=(block.base+rank)*replicas_+chosen_seed;
  if(!used_.insert(virtual_index).second)
   throw Error("v2 weighted sampler duplicated a candidate");
  const auto leaf_mass=mass/static_cast<long double>(replicas_);
  for(const auto& prefix:path)removed_[prefix]+=leaf_mass;
  ++drawn_;
  return virtual_index;
 }
};
} // namespace ps
