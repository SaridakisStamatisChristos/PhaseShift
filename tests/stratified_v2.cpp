#include "phaseshift/stratified_v2.hpp"
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#define CHECK(condition) do{if(!(condition))throw std::runtime_error("stratified_v2: " #condition);}while(false)
int main(){
 using ps::StratifiedIndexSamplerV2;
 using Axis=ps::AdmissibleSearchSpace::Axis;
 using Scope=ps::AdmissibleSearchSpace::Subspace;
 const std::vector<Axis> axes={{"flag",{"false","true"}},{"kind",{"a","b"}},{"x",{"0","1","2","3"}}};
 const std::vector<Scope> rules={
  {{"kind",{"a"}},{"x",{"0","1"}}},
  {{"flag",{"true"}},{"kind",{"b"}},{"x",{"2","3"}}}
 };
 const ps::AdmissibleSearchSpace universe(axes,rules);
 StratifiedIndexSamplerV2 a(axes,rules,"x",2,2026),b(axes,rules,"x",2,2026);
 std::set<std::string> warmup;
 std::set<std::uint64_t> observed;
 for(std::uint64_t i=0;i<universe.cardinality()*2;++i){
  CHECK(a.has_next()&&b.has_next());
  const auto x=a.next(),y=b.next();
  CHECK(x==y);CHECK(a.state()==b.state());
  CHECK(observed.insert(x).second);
  const auto key=universe.at(x/2);
  if(i<4)warmup.insert(key[2].second);
 }
 CHECK(warmup.size()==4);
 CHECK(observed.size()==universe.cardinality()*2);
 CHECK(!a.has_next()&&!b.has_next());
 bool exhausted=false;try{(void)a.next();}catch(const ps::Error&){exhausted=true;}CHECK(exhausted);
 // Uniform membership and valid strides even when primary axis is not last.
 const std::vector<Axis> reversed={{"x",{"0","1","2"}},{"z",{"a","b"}}};
 StratifiedIndexSamplerV2 first(reversed,{},"x",3,12);
 std::set<std::uint64_t> positions;
 while(first.has_next())CHECK(positions.insert(first.next()).second);
 CHECK(positions.size()==18);
 bool invalid=false;try{(void)StratifiedIndexSamplerV2(axes,rules,"ghost",2,22);}catch(const ps::Error&){invalid=true;}CHECK(invalid);
 invalid=false;try{(void)StratifiedIndexSamplerV2(axes,rules,"x",0,22);}catch(const ps::Error&){invalid=true;}CHECK(invalid);
 return 0;
}
