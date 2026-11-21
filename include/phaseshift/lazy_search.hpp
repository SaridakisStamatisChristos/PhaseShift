#pragma once
#include "phaseshift/core.hpp"
#include <algorithm>
#include <cstdint>
#include <map>
#include <limits>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
namespace ps {
// Mixed-radix lazy universe: resident storage scales with axis lists, not product.
class LazySearchSpace {
public:
 using Axis=std::pair<std::string,std::vector<std::string>>;
 explicit LazySearchSpace(std::vector<Axis> axes):axes_(std::move(axes)){
  for(const auto& [name,values]:axes_){
   if(name.empty()||values.empty())throw Error("empty v2 search axis");
   if(std::set<std::string>(values.begin(),values.end()).size()!=values.size())
    throw Error("duplicate v2 search axis label: "+name);
   const auto radix=static_cast<std::uint64_t>(values.size());
   if(cardinality_>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())/radix)
    throw Error("CARDINALITY_OVERFLOW: v2 search exceeds signed SQLite index domain");
   cardinality_*=radix;
  }
 }
 std::uint64_t cardinality() const noexcept{return cardinality_;}
 std::vector<std::pair<std::string,std::string>> at(std::uint64_t index) const {
  if(index>=cardinality_)throw Error("v2 candidate index outside search space");
  std::vector<std::pair<std::string,std::string>> out(axes_.size());
  for(std::size_t i=axes_.size();i>0;--i){
   const auto& [name,values]=axes_[i-1];
   const auto radix=static_cast<std::uint64_t>(values.size());
   out[i-1]={name,values[static_cast<std::size_t>(index%radix)]};
   index/=radix;
  }
  return out;
 }
private:
 std::vector<Axis> axes_;std::uint64_t cardinality_=1;
};

/* Indexed union of pairwise-disjoint admissible axis-label rectangles.
   Exact uniform virtual-index sampling has no rejection bias inside the union.
   Resident memory scales with the declared rectangles, never their product. */
class AdmissibleSearchSpace {
public:
 using Axis=LazySearchSpace::Axis;
 using Subspace=std::map<std::string,std::vector<std::string>>;
 explicit AdmissibleSearchSpace(std::vector<Axis> axes,std::vector<Subspace> scopes={}){
  if(axes.empty())throw Error("v2 search requires at least one axis");
  std::map<std::string,std::set<std::string>> domains;
  for(const auto& [key,labels]:axes){
   if(!domains.emplace(key,std::set<std::string>(labels.begin(),labels.end())).second)
    throw Error("duplicate v2 axis name: "+key);
  }
  if(scopes.empty()){blocks_.emplace_back(std::move(axes));ends_.push_back(blocks_.back().cardinality());return;}
  if(scopes.size()>256)throw Error("admissible v2 subspace cap exceeded (256)");
  for(const auto& scope:scopes){
   if(scope.empty())throw Error("empty v2 admissible subspace");
   for(const auto& [key,allowed]:scope){
    const auto d=domains.find(key);
    if(d==domains.end())throw Error("unknown v2 admissible axis: "+key);
    if(allowed.empty()||std::set<std::string>(allowed.begin(),allowed.end()).size()!=allowed.size())
     throw Error("duplicate/empty v2 admissible labels: "+key);
    for(const auto& label:allowed)if(!d->second.count(label))
     throw Error("out-of-domain v2 admissible label: "+key+"="+label);
   }
  }
  // Rectangles must be disjoint; otherwise the shared overlap is oversampled.
  for(std::size_t i=0;i<scopes.size();++i)for(std::size_t j=i+1;j<scopes.size();++j){
   bool overlap=true;
   for(const auto& [key,domain]:domains){
    (void)domain;
    const auto a=scopes[i].find(key),b=scopes[j].find(key);
    if(a==scopes[i].end()||b==scopes[j].end())continue;
    bool shared=false;
    for(const auto& value:a->second)
     if(std::find(b->second.begin(),b->second.end(),value)!=b->second.end()){shared=true;break;}
    if(!shared){overlap=false;break;}
   }
   if(overlap)throw Error("overlapping v2 admissible subspaces");
  }
  for(const auto& scope:scopes){
   auto restricted=axes;
   for(auto& [key,labels]:restricted){
    auto found=scope.find(key);
    if(found!=scope.end())labels=found->second;
   }
   blocks_.emplace_back(std::move(restricted));
   const auto n=blocks_.back().cardinality();
   if(total_>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())-n)
    throw Error("CARDINALITY_OVERFLOW: v2 admissible union exceeds SQLite index domain");
   total_+=n;ends_.push_back(total_);
  }
 }
 std::uint64_t cardinality() const noexcept{return ends_.size()==1?ends_.front():total_;}
 std::size_t subspace_count() const noexcept{return blocks_.size();}
 std::vector<std::pair<std::string,std::string>> at(std::uint64_t index) const {
  if(index>=cardinality())throw Error("v2 admissible coordinate index out of range");
  const auto found=std::upper_bound(ends_.begin(),ends_.end(),index);
  const auto block=static_cast<std::size_t>(std::distance(ends_.begin(),found));
  return blocks_[block].at(index-(block?ends_[block-1]:0));
 }
private:
 std::vector<LazySearchSpace> blocks_;
 std::vector<std::uint64_t> ends_;
 std::uint64_t total_=0;
};

// Exact random k-without-replacement from N virtual indices.
// Sparse Fisher-Yates: O(k) resident entries, no N-sized vector.
class SparsePermutation {
public:
 SparsePermutation(std::uint64_t n,std::uint64_t seed):remaining_(n),state_(seed){
  if(n==0||n>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
   throw Error("CARDINALITY_OVERFLOW: invalid v2 permutation size");
 }
 bool has_next() const noexcept{return remaining_>0;}
 std::uint64_t state() const noexcept{return state_;}
 std::size_t resident_entries() const noexcept{return mapped_.size();}
 std::uint64_t next(){
  if(!has_next())throw Error("v2 candidate pool exhausted");
  const auto slot=randbelow(state_,remaining_),last=remaining_-1;
  const auto value=lookup(slot),replacement=lookup(last);
  mapped_.erase(slot);
  if(slot!=last)mapped_[slot]=replacement;
  mapped_.erase(last);--remaining_;
  return value;
 }
private:
 std::uint64_t lookup(std::uint64_t i) const {
  const auto it=mapped_.find(i);return it==mapped_.end()?i:it->second;
 }
 std::uint64_t remaining_,state_;
 std::unordered_map<std::uint64_t,std::uint64_t> mapped_;
};
}
