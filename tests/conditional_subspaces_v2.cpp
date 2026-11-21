#include "phaseshift/lazy_search.hpp"
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#define CHECK(cond) do{if(!(cond))throw std::runtime_error("admissible search: " #cond);}while(false)
int main(){
 using ps::AdmissibleSearchSpace;using ps::SparsePermutation;
 using Axis=AdmissibleSearchSpace::Axis;using Scope=AdmissibleSearchSpace::Subspace;
 const std::vector<Axis> axes={{"flag",{"false","true"}},{"kind",{"a","b"}},{"x",{"0","1","2","3"}}};
 const std::vector<Scope> rules={
  {{"kind",{"a"}},{"x",{"0","1"}}},
  {{"flag",{"true"}},{"kind",{"b"}},{"x",{"2","3"}}}
 };
 const AdmissibleSearchSpace valid(axes,rules);
 CHECK(valid.cardinality()==6);
 CHECK(valid.subspace_count()==2);
 std::set<std::string> identities;
 for(std::uint64_t i=0;i<valid.cardinality();++i){
  const auto coords=valid.at(i);
  CHECK(coords.size()==3);
  const auto flag=coords[0].second,kind=coords[1].second,x=coords[2].second;
  CHECK((kind=="a"&&(x=="0"||x=="1"))||
        (kind=="b"&&flag=="true"&&(x=="2"||x=="3")));
  CHECK(identities.insert(flag+"|"+kind+"|"+x).second);
  CHECK(valid.index_of(coords).has_value());
  CHECK(*valid.index_of(coords)==i);
 }
 CHECK(identities.size()==6);
 SparsePermutation a(valid.cardinality(),2026),b(valid.cardinality(),2026);
 std::set<std::uint64_t> sampled;
 while(a.has_next()){const auto index=a.next();CHECK(index==b.next());CHECK(sampled.insert(index).second);}
 CHECK(!b.has_next());CHECK(sampled.size()==6);
 const AdmissibleSearchSpace unrestricted(axes);
 CHECK(unrestricted.cardinality()==16);CHECK(unrestricted.subspace_count()==1);
 for(std::uint64_t i=0;i<unrestricted.cardinality();++i){
  const auto coords=unrestricted.at(i);
  CHECK(unrestricted.index_of(coords)==i);
 }
 CHECK(!valid.index_of({{"flag","false"},{"kind","b"},{"x","2"}}));
 CHECK(!valid.index_of({{"flag","false"},{"kind","a"},{"x","9"}}));
 CHECK(!valid.index_of({{"kind","a"},{"flag","false"},{"x","0"}}));
 CHECK(!valid.index_of({{"flag","false"},{"kind","a"}}));
 CHECK(!valid.index_of({{"flag","false"},{"kind","a"},{"x","0"},{"extra","x"}}));
 std::vector<std::string> labels;
 for(int i=0;i<1000;++i)labels.push_back(std::to_string(i));
 const AdmissibleSearchSpace huge({{"a",labels},{"b",labels},{"c",labels}});
 CHECK(huge.cardinality()==1000000000ULL);
 CHECK(huge.at(999999999).size()==3);
 CHECK(huge.index_of(huge.at(999999999))==999999999ULL);
 const auto rejected=[&](const std::vector<Scope>& examples){
  try{(void)AdmissibleSearchSpace(axes,examples);}catch(const ps::Error&){return true;}return false;
 };
 CHECK(rejected({{{"kind",{"a"}}},{{"x",{"0"}}}}));
 CHECK(rejected({{{"missing",{"a"}}}}));
 CHECK(rejected({{{"x",{"0","0"}}}}));
 CHECK(rejected({{{"x",{"99"}}}}));
 CHECK(rejected({{{"x",{}}}}));
 CHECK(rejected({Scope{}}));
 CHECK(!rejected({{{"kind",{"a"}}},{{"kind",{"b"}}}}));
 bool out_of_range=false;try{(void)valid.at(6);}catch(const ps::Error&){out_of_range=true;}CHECK(out_of_range);
 return 0;
}
