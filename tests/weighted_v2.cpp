#include "phaseshift/weighted_v2.hpp"
#include <cmath>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#define CHECK(x) do { if(!(x))throw std::runtime_error("weighted_v2: " #x); } while(false)

int main(){
 using Sampler=ps::WeightedIndexSamplerV2;
 using Axis=Sampler::Axis;
 using Scope=Sampler::Scope;
 const std::vector<Axis> axes={{"mode",{"a","b"}},{"x",{"0","1","2"}}};
 const std::vector<Scope> scopes={
  {{"mode",{"a"}},{"x",{"0","1"}}},
  {{"mode",{"b"}},{"x",{"1","2"}}}
 };
 const Sampler::Priors p={{"mode",{{"a",9.},{"b",1.}}},
                         {"x",{{"0",3.},{"1",1.},{"2",1.}}}};
 const ps::AdmissibleSearchSpace valid(axes,scopes);
 CHECK(valid.cardinality()==4);
 Sampler a(axes,scopes,p,3,2026),b(axes,scopes,p,3,2026);
 std::set<std::uint64_t> seen;
 while(a.has_next()){
  CHECK(b.has_next());
  const auto i=a.next(),j=b.next();
  CHECK(i==j && a.state()==b.state());
  CHECK(i<valid.cardinality()*3);
  const auto coords=valid.at(i/3);
  CHECK(coords[0].second=="a" || coords[0].second=="b");
  CHECK(seen.insert(i).second);
 }
 CHECK(!b.has_next() && seen.size()==12);
 CHECK(a.resident_entries()<150);
 bool threw=false;
 try{(void)a.next();}catch(const ps::Error&){threw=true;}CHECK(threw);

 // The priors must affect proposal mass, without inventing categorical order.
 std::size_t favored=0;
 for(std::uint64_t seed=0;seed<2048;++seed){
  Sampler once(axes,scopes,p,1,seed);
  const auto coords=valid.at(once.next());
  if(coords[0].second=="a")++favored;
 }
 CHECK(favored>1750 && favored<2048);

 // Sparse search must not materialize an enormous virtual Cartesian product.
 std::vector<std::string> labels;
 for(int i=0;i<1000;++i)labels.push_back(std::to_string(i));
 const std::vector<Axis> huge={{"a",labels},{"b",labels},{"c",labels}};
 const Sampler::Priors heavy={{"a",{{"0",10.}}}};
 Sampler sparse(huge,{},heavy,1,77);
 std::set<std::uint64_t> large;
 for(int i=0;i<64;++i)CHECK(large.insert(sparse.next()).second);
 CHECK(large.size()==64 && sparse.resident_entries()<600);

 const auto rejects=[&](const Sampler::Priors& bad,
                        const std::vector<Scope>& allowed=scopes){
  try{(void)Sampler(axes,allowed,bad,1,9);}
  catch(const ps::Error&){return true;}
  return false;
 };
 CHECK(rejects({}));
 CHECK(rejects({{"missing",{{"x",1.}}}}));
 CHECK(rejects({{"mode",{{"missing",1.}}}}));
 CHECK(rejects({{"mode",{{"a",0.}}}}));
 CHECK(rejects({{"mode",{{"a",1.e-12}}}}));
 CHECK(rejects({{"mode",{{"a",1.e12}}}}));
 CHECK(rejects({{"mode",{{"a",std::numeric_limits<double>::infinity()}}}}));
 CHECK(rejects(p,{{{"mode",{"a"}}},{{"x",{"0"}}}}));
 threw=false;
 try{(void)Sampler(axes,scopes,p,0,9);}catch(const ps::Error&){threw=true;}
 CHECK(threw);
 return 0;
}
