#pragma once
#include "phaseshift/lazy_search.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ps {

// A restricted logical implication: WHEN axis belongs to one of these labels,
// REQUIRE another (possibly the same) axis to belong to the specified labels.
// The compiler transforms AND-ed implications into a bounded, disjoint union
// of rectangles. This keeps uniform/stratified/weighted sampling exact over
// eligible candidates, with no enumeration of their Cartesian product.
struct ConditionalRuleV2 {
 std::string when_axis;
 std::vector<std::string> when_labels;
 std::string require_axis;
 std::vector<std::string> require_labels;
};

inline std::vector<AdmissibleSearchSpace::Subspace>
compile_conditional_rules_v2(
 const std::vector<AdmissibleSearchSpace::Axis>& axes,
 const std::vector<AdmissibleSearchSpace::Subspace>& initial,
 const std::vector<ConditionalRuleV2>& rules){
 using Scope=AdmissibleSearchSpace::Subspace;
 if(rules.empty())return initial; // Existing v2 transcripts remain identical.
 if(rules.size()>32)throw Error("v2 conditional rule cap exceeded (32)");
 // Independently validate user-supplied rectangles before normalization.
 (void)AdmissibleSearchSpace(axes,initial);
 std::map<std::string,std::vector<std::string>> domains;
 for(const auto& [name,labels]:axes){
  if(!domains.emplace(name,labels).second)
   throw Error("v2 conditional duplicate axis: "+name);
 }
 for(const auto& rule:rules){
  const auto wa=domains.find(rule.when_axis);
  const auto ra=domains.find(rule.require_axis);
  if(wa==domains.end()||ra==domains.end())
   throw Error("v2 conditional rule uses unknown axis");
  const auto check=[&](const std::vector<std::string>& labels,
                        const std::vector<std::string>& domain){
   if(labels.empty()||std::set<std::string>(labels.begin(),labels.end()).size()!=labels.size())
    throw Error("v2 conditional labels must be nonempty and distinct");
   for(const auto& label:labels)
    if(std::find(domain.begin(),domain.end(),label)==domain.end())
     throw Error("v2 conditional label outside declared axis");
  };
  check(rule.when_labels,wa->second);
  check(rule.require_labels,ra->second);
 }
 std::vector<Scope> current=initial.empty()?std::vector<Scope>{Scope{}}:initial;
 const auto canonical=[&](Scope& scope,const std::string& axis,
                          const std::vector<std::string>& subset){
  const auto& full=domains.at(axis);
  if(subset==full)scope.erase(axis);
  else scope[axis]=subset;
 };
 const auto effective=[&](const Scope& scope,const std::string& axis){
  const auto found=scope.find(axis);
  return found==scope.end()?domains.at(axis):found->second;
 };
 for(const auto& rule:rules){
  std::vector<Scope> updated;
  for(const auto& rect:current){
   const auto allowed=effective(rect,rule.when_axis);
   const std::set<std::string> selected(rule.when_labels.begin(),rule.when_labels.end());
   std::vector<std::string> yes,no;
   for(const auto& v:allowed)
    (selected.count(v)?yes:no).push_back(v);
   if(!no.empty()){
    Scope outside=rect;
    canonical(outside,rule.when_axis,no);
    updated.push_back(std::move(outside));
   }
   if(!yes.empty()){
    Scope inside=rect;
    canonical(inside,rule.when_axis,yes);
    const auto require=effective(inside,rule.require_axis);
    const std::set<std::string> constrained(rule.require_labels.begin(),rule.require_labels.end());
    std::vector<std::string> valid;
    for(const auto& v:require)if(constrained.count(v))valid.push_back(v);
    if(!valid.empty()){
     canonical(inside,rule.require_axis,valid);
     updated.push_back(std::move(inside));
    }
   }
   if(updated.size()>256)throw Error("v2 conditional decomposition exceeds 256 rectangles");
  }
  if(updated.empty())throw Error("v2 conditional rules exclude all candidates");
  current=std::move(updated);
 }
 if(current.size()==1&&current.front().empty())return {};
 // Recheck geometric disjointness, index uniqueness and checked cardinality.
 (void)AdmissibleSearchSpace(axes,current);
 return current;
}
inline bool pair_in_admissible_space_v2(
 const std::vector<AdmissibleSearchSpace::Axis>& axes,
 const std::vector<AdmissibleSearchSpace::Subspace>& subspaces,
 const std::vector<std::pair<std::string,std::string>>& left,
 const std::vector<std::pair<std::string,std::string>>& right){
 const AdmissibleSearchSpace space(axes,subspaces);
 return space.index_of(left).has_value()&&space.index_of(right).has_value();
}
} // namespace ps
