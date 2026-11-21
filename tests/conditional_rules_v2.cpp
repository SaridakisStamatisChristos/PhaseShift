#include "phaseshift/conditional_space_v2.hpp"
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#define CHECK(e) do { if(!(e))throw std::runtime_error("conditional_v2: " #e); }while(false)
int main(){
 using namespace ps;
 using Axis=AdmissibleSearchSpace::Axis;
 using Scope=AdmissibleSearchSpace::Subspace;
 const std::vector<Axis> axes={
  {"flag",{"false","true"}},{"kind",{"a","b"}},{"x",{"0","1","2","3"}}};
 const std::vector<ConditionalRuleV2> rules={
  {"kind",{"a"},"x",{"0","1"}},
  {"kind",{"b"},"flag",{"true"}},
  {"kind",{"b"},"x",{"2","3"}}
 };
 const auto compiled=compile_conditional_rules_v2(axes,{},rules);
 const AdmissibleSearchSpace legal(axes,compiled);
 CHECK(legal.cardinality()==6);
 std::set<std::uint64_t> ranks;
 const AdmissibleSearchSpace full(axes);
 for(std::uint64_t i=0;i<full.cardinality();++i){
  const auto coords=full.at(i);
  const bool valid=(coords[1].second=="a"&&
      (coords[2].second=="0"||coords[2].second=="1"))||
     (coords[1].second=="b"&&coords[0].second=="true"&&
      (coords[2].second=="2"||coords[2].second=="3"));
  const auto rank=legal.index_of(coords);
  CHECK(rank.has_value()==valid);
  if(rank){CHECK(ranks.insert(*rank).second);CHECK(legal.at(*rank)==coords);}
 }
 CHECK(ranks.size()==6);
 CHECK(compile_conditional_rules_v2(axes,{},rules)==compiled);
 // Rule compiler must be stable when applied to an already restricted union:
 // this happens when a stored experiment config contains both compiled scopes
 // and the original rules on resume.
 const auto again=compile_conditional_rules_v2(axes,compiled,rules);
 const AdmissibleSearchSpace resumed(axes,again);
 CHECK(resumed.cardinality()==legal.cardinality());
 for(std::uint64_t i=0;i<full.cardinality();++i)
  CHECK(resumed.index_of(full.at(i)).has_value()==
        legal.index_of(full.at(i)).has_value());

 // Same-axis implication is a valid exact intersection, not a special bypass.
 const auto self=compile_conditional_rules_v2(
  axes,{},{{"x",{"2","3"},"x",{"2"}}});
 const AdmissibleSearchSpace narrowed(axes,self);
 CHECK(narrowed.cardinality()==12); // x=0,1,2 only
 const auto impossible=[&](const std::vector<ConditionalRuleV2>& rs){
  try{(void)compile_conditional_rules_v2(axes,{},rs);}
  catch(const Error&){return true;}
  return false;
 };
 CHECK(impossible({{"kind",{"z"},"x",{"1"}}}));
 CHECK(impossible({{"bad",{"1"},"x",{"1"}}}));
 CHECK(impossible({{"kind",{"a"},"bad",{"1"}}}));
 CHECK(impossible({{"kind",{},"x",{"1"}}}));
 CHECK(impossible({{"kind",{"a","a"},"x",{"1"}}}));
 CHECK(impossible({{"kind",{"a"},"x",{}}}));
 CHECK(impossible({{"kind",{"a"},"x",{"0"}},{"kind",{"a"},"x",{"1"}},
                   {"kind",{"b"},"x",{"2"}},{"kind",{"b"},"x",{"3"}}}));
 std::vector<ConditionalRuleV2> excessive(33,{"kind",{"a"},"x",{"0"}});
 CHECK(impossible(excessive));
 // Large Cartesian space: 1 billion potential coordinates but only a few
 // declared axis vectors plus a bounded compiled rectangle set in memory.
 std::vector<std::string> many;
 for(int i=0;i<1000;++i)many.push_back(std::to_string(i));
 const std::vector<Axis> huge={{"a",many},{"b",many},{"c",many}};
 const auto sparse=compile_conditional_rules_v2(
  huge,{},{{"a",{"0"},"b",{"0","1"}}});
 const AdmissibleSearchSpace huge_legal(huge,sparse);
 CHECK(huge_legal.cardinality()==999002000ULL);
 CHECK(huge_legal.index_of(huge_legal.at(huge_legal.cardinality()-1)).has_value());
 return 0;
}
