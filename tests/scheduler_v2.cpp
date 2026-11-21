#include "phaseshift/scheduler_v2.hpp"
#include <cassert>
#include <cstddef>
#include <optional>
#include <set>
#include <vector>
int main(){
 ps::HierarchicalPolicyV2 a(64,1234,0.2),b(64,1234,0.2);
 std::set<std::size_t> seen;std::vector<std::size_t> order;
 while(const auto i=a.next()){
  const auto j=b.next();assert(j&&*i==*j);
  assert(seen.insert(*i).second);order.push_back(*i);
  const std::optional<double> signal=(*i<31)?1.:20.;
  a.observe(*i,signal,2.);b.observe(*j,signal,2.);
  assert(a.rng_state()==b.rng_state());
 }
 assert(order.size()==64&&order[0]==0&&order[1]==63);
 assert(!a.next().has_value());
 bool bad=false;try{a.observe(0,1.,1.);}catch(const ps::Error&){bad=true;}assert(bad);
 bad=false;try{ps::HierarchicalPolicyV2 c(0,0,0.2);}catch(const ps::Error&){bad=true;}assert(bad);
 bad=false;try{ps::HierarchicalPolicyV2 c(3,0,1.1);}catch(const ps::Error&){bad=true;}assert(bad);
 ps::HierarchicalPolicyV2 one(1,7,0.);
 assert(one.next()==std::optional<std::size_t>(0));
 one.observe(0,2.,1.);assert(!one.next());
 return 0;
}
