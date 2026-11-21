#include "phaseshift/conditional_space_v2.hpp"
#include "phaseshift/weighted_v2.hpp"
#include <cstdint>
#include <iostream>
#include <set>
#include <stdexcept>
#ifdef __linux__
#include <sys/resource.h>
#endif
#define CHECK(e) do{if(!(e))throw std::runtime_error("rss_v2: " #e);}while(false)
int main(){
 using Axis=ps::AdmissibleSearchSpace::Axis;
 std::vector<std::string> labels;
 for(int i=0;i<1000;++i)labels.push_back(std::to_string(i));
 const std::vector<Axis> axes={{"a",labels},{"b",labels},{"c",labels}};
 const auto disjoint=ps::compile_conditional_rules_v2(
  axes,{},{{"a",{"0"},"b",{"0","1"}}});
 const ps::AdmissibleSearchSpace space(axes,disjoint);
 CHECK(space.cardinality()==999002000ULL);
 const ps::WeightedIndexSamplerV2::Priors prior={{"a",{{"0",10.}}}};
 ps::WeightedIndexSamplerV2 sampler(axes,disjoint,prior,1,2026);
 std::set<std::uint64_t> selected;
 for(int i=0;i<256;++i){
  const auto index=sampler.next();
  CHECK(index<space.cardinality());
  CHECK(space.index_of(space.at(index))==index);
  CHECK(selected.insert(index).second);
 }
 CHECK(sampler.resident_entries()<3000);
#ifdef __linux__
 rusage usage{};
 CHECK(getrusage(RUSAGE_SELF,&usage)==0);
 // ru_maxrss is KiB on Linux. The upper bound is intentionally much larger
 // for sanitizer instrumentation and runtime loader overhead; this verifies
 // nonmaterialization, not precise allocations or an external memory win.
#ifdef PHASESHIFT_ASAN_BUILD
 constexpr long rss_cap_kib=256L*1024L;
#else
 constexpr long rss_cap_kib=128L*1024L;
#endif
 std::cout<<"v2 billion-coordinate weighted+conditional peak RSS KiB="
          <<usage.ru_maxrss<<" cap="<<rss_cap_kib<<"\n";
 CHECK(usage.ru_maxrss>0&&usage.ru_maxrss<rss_cap_kib);
#endif
 return 0;
}
