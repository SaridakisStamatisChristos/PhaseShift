#pragma once
#include "phaseshift/core.hpp"
#include <cstdint>
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
