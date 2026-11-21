#include "phaseshift/scheduler_v2.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#define CHECK(e) do{if(!(e))throw std::runtime_error("scheduler_heap_v2: " #e);}while(false)
// Independent original O(n) scan oracle with the same input/output contract.
struct ScanOracle {
 struct Point{std::optional<double> value;double cpu;};
 std::size_t n;
 double frac;
 ps::SparsePermutation random;
 std::map<std::size_t,Point> seen;
 explicit ScanOracle(std::size_t n_,std::uint64_t seed,double f)
   :n(n_),frac(f),random(n_,seed){}
 void observe(std::size_t index,std::optional<double> m,double cpu){
  seen.emplace(index,Point{m,std::min(1.e6,std::max(1.,cpu))});
 }
 std::optional<std::size_t> next(){
  if(seen.size()==n)return {};
  if(seen.empty())return 0;
  if(seen.size()==1&&!seen.count(n-1))return n-1;
  const auto k=seen.size();
  auto quota=[&](std::size_t x){
   return static_cast<std::size_t>(std::floor(static_cast<double>(x)*frac+1e-9));
  };
  auto unseen=[&]()->std::optional<std::size_t>{
   while(random.has_next()){
    const auto i=static_cast<std::size_t>(random.next());
    if(!seen.count(i))return i;
   }return {};
  };
  if(k>=2&&quota(k-1)>quota(k-2))return unseen();
  double best=-1;
  std::size_t best_at=0;
  for(auto a=seen.begin(),b=std::next(a);b!=seen.end();++a,++b){
   if(b->first<=a->first+1)continue;
   const auto span=b->first-a->first-1;
   const bool paired=a->second.value&&b->second.value;
   const double contrast=paired?std::min(2.,std::abs(*a->second.value-*b->second.value)):0.;
   const double uncertainty=a->second.value.has_value()!=b->second.value.has_value()?1.:0.2;
   const double cost=1.+0.5*(a->second.cpu+b->second.cpu);
   const double score=(0.2+contrast+uncertainty)*static_cast<double>(span)/std::sqrt(cost);
   if(score>best){best=score;best_at=a->first+(b->first-a->first)/2;}
  }
  if(best>=0)return best_at;
  return unseen();
 }
};
int main(){
 for(const std::size_t n:{1U,2U,3U,11U,64U,127U,1000U}){
  for(const auto fraction:{0.,0.2,0.5,1.}){
   for(std::uint64_t seed:{0ULL,1ULL,2026ULL}){
    ps::HierarchicalPolicyV2 heap(n,seed,fraction);
    ScanOracle scan(n,seed,fraction);
    for(std::size_t i=0;i<n;++i){
     const auto proposed=heap.next();
     const auto expected=scan.next();
     CHECK(proposed==expected&&proposed.has_value());
     const auto index=*proposed;
     std::optional<double> value;
     if(index%7!=0)value=static_cast<double>((index*17+seed)%19)/3.;
     const double cost=static_cast<double>(1+index%13);
     heap.observe(index,value,cost);
     scan.observe(index,value,cost);
     CHECK(heap.rng_state()==scan.random.state());
    }
    CHECK(!heap.next()&&!scan.next());
   }
  }
 }
 return 0;
}
