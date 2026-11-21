#include "phaseshift/hybrid_v2.hpp"
#include <limits>
#include <set>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("hybrid: " #x);}while(false)
int main(){
 using ps::AdmissibleSearchSpace;using ps::HybridMixedPolicyV2;
 using Axis=AdmissibleSearchSpace::Axis;
 const std::vector<Axis> axes={{"mode",{"a","b"}},{"x",{"0","1","2","3","4","5","6","7"}}};
 const AdmissibleSearchSpace space(axes);
 const auto idx=[&](const char* mode,const char* x){
  return *space.index_of({{"mode",mode},{"x",x}});
 };
 HybridMixedPolicyV2 targeted(axes,{},"x",1,42,0.,true);
 for(const auto i:{idx("a","0"),idx("a","7"),idx("b","0"),idx("b","7")})
  targeted.observe(i,i==idx("a","0")?1.:10.,2.);
 const auto selected=targeted.next();
 CHECK(selected);
 CHECK(targeted.origin()=="hybrid_matched_interval");
 CHECK(*selected==idx("a","3")||*selected==idx("b","3"));
 // An observed matched structural contrast must request two NEW
 // observations with the same seed, not independent unrelated seeds.
 HybridMixedPolicyV2 paired(axes,{},"x",3,2026,0.,true);
 for(const auto p:std::vector<std::pair<std::uint64_t,double>>{
     {idx("a","0"),1.},{idx("a","7"),10.},
     {idx("b","0"),1.},{idx("b","7"),1.}})
  paired.observe(p.first*3,p.second,2.);
 const auto left_replicate=paired.next();
 CHECK(left_replicate&&*left_replicate==idx("a","0")*3+1);
 CHECK(paired.origin()=="hybrid_paired_replication");
 paired.observe(*left_replicate,1.,2.);
 const auto right_replicate=paired.next();
 CHECK(right_replicate&&*right_replicate==idx("a","7")*3+1);
 CHECK(paired.origin()=="hybrid_paired_replication");
 CHECK(*left_replicate%3==*right_replicate%3);
 CHECK(*left_replicate/3!=*right_replicate/3);
 paired.observe(*right_replicate,10.,2.);
 HybridMixedPolicyV2 null_pair(axes,{},"x",3,2026,0.,true);
 for(const auto i:{idx("a","0"),idx("a","7"),idx("b","0"),idx("b","7")})
  null_pair.observe(i*3,1.,2.);
 CHECK(null_pair.next());
 CHECK(null_pair.origin()!="hybrid_paired_replication");
 HybridMixedPolicyV2 a(axes,{},"x",2,12,0.25,true),b(axes,{},"x",2,12,0.25,true);
 std::set<std::uint64_t> seen;
 for(int k=0;k<32;++k){
  const auto next=a.next(),repeat=b.next();
  CHECK(next&&repeat&&*next==*repeat);
  CHECK(a.rng_state()==b.rng_state());
  CHECK(seen.insert(*next).second);
  const double metric=space.at(*next/2)[0].second=="a"?2.:6.;
  a.observe(*next,metric,2.);
  b.observe(*repeat,metric,2.);
 }
 CHECK(seen.size()==32&&!a.next()&&!b.next());
 const std::vector<AdmissibleSearchSpace::Subspace> scopes={
  {{"mode",{"a"}},{"x",{"0","1","2","3"}}},
  {{"mode",{"b"}},{"x",{"4","5","6","7"}}}};
 const AdmissibleSearchSpace valid(axes,scopes);
 HybridMixedPolicyV2 restricted(axes,scopes,"x",1,4,0.3,true);
 std::set<std::uint64_t> unique;
 while(const auto candidate=restricted.next()){
  CHECK(*candidate<valid.cardinality());
  CHECK(unique.insert(*candidate).second);
  restricted.observe(*candidate,1.,3.);
 }
 CHECK(unique.size()==valid.cardinality());
 const std::vector<Axis> nominal={{"flag",{"false","true"}},{"mode",{"a","b","c"}}};
 const AdmissibleSearchSpace cat(nominal);
 HybridMixedPolicyV2 categories(nominal,{},"mode",1,9,0.,false);
 for(const auto& coords:std::vector<std::vector<std::pair<std::string,std::string>>>{
  {{"flag","false"},{"mode","a"}},{{"flag","true"},{"mode","a"}},
  {{"flag","false"},{"mode","b"}},{{"flag","true"},{"mode","b"}}})
  categories.observe(*cat.index_of(coords),1.,3.);
 const auto categorical=categories.next();
 CHECK(categorical&&categories.origin()=="hybrid_matched_category");
 CHECK(cat.at(*categorical)[1].second=="c");
 bool threw=false;
 try{categories.observe(*categorical,std::numeric_limits<double>::infinity(),1.);}
 catch(const ps::Error&){threw=true;}
 CHECK(threw);
 return 0;
}
