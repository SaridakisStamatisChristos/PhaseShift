#include "phaseshift/scheduler_v2.hpp"
#include "phaseshift/core.hpp"
#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>
#define CHECK(x) do{if(!(x))throw std::runtime_error("cliff_bracket_v2 oracle: " #x);}while(false)
int main(){
 // Independent exhaustive reference: a monotone one-step function with an
 // unknown cut at any of the 63 possible positions must be fully bracketed
 // by exactly eight endpoint + bisection measurements, with zero exploration.
 for(std::size_t threshold=1;threshold<64;++threshold){
  ps::HierarchicalPolicyV2 a(64,42,0.,true),b(64,42,0.,true);
  std::vector<std::optional<double>> seen(64);
  for(std::size_t step=0;step<8;++step){
   const auto x=a.next(),y=b.next();
   CHECK(x.has_value()&&y.has_value()&&x==y);
   CHECK(!seen[*x].has_value());
   const double value=*x<threshold?0.:2.;
   seen[*x]=value;
   a.observe(*x,value,1.);b.observe(*y,value,1.);
   CHECK(a.rng_state()==b.rng_state());
   CHECK(a.origin()==b.origin());
  }
  std::size_t left=0,right=64;
  for(std::size_t i=0;i<64;++i)if(seen[i]){
   if(i<threshold&&i>left)left=i;
   if(i>=threshold&&i<right)right=i;
  }
  CHECK(right==threshold&&left+1==right);
 }
 // The original 3-argument hierarchical policy and a false contrast policy
 // must replay the same decisions after every committed observation.
 ps::HierarchicalPolicyV2 legacy(64,17,0.2),explicit_legacy(64,17,0.2,false);
 for(std::size_t step=0;step<64;++step){
  const auto a=legacy.next(),b=explicit_legacy.next();
  CHECK(a&&b&&a==b);
  const auto signal=*a<24?1.:2.;
  legacy.observe(*a,signal,3.);
  explicit_legacy.observe(*b,signal,3.);
  CHECK(legacy.rng_state()==explicit_legacy.rng_state());
 }
 CHECK(!legacy.next()&&!explicit_legacy.next());
 bool thrown=false;
 try{ps::HierarchicalPolicyV2 bad(0,9,0.,true);}catch(const ps::Error&){thrown=true;}
 CHECK(thrown);
 thrown=false;
 try{ps::HierarchicalPolicyV2 bad(64,9,NAN,true);}catch(const ps::Error&){thrown=true;}
 CHECK(thrown);
 return 0;
}
