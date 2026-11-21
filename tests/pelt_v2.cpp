#include "phaseshift/boundary_v2.hpp"
#include "phaseshift/core.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>
#define CHECK(expr) do{if(!(expr))throw std::runtime_error("PELT differential: " #expr);}while(false)
int main(){
 for(int mask=0;mask<256;++mask){
  std::vector<double> y;
  for(int i=0;i<8;++i)y.push_back(mask&(1<<i)?4.:0.);
  for(std::size_t min=1;min<=3;++min)
   for(std::size_t k=0;k<=3;++k)
    for(double penalty:{0.,0.1,0.5,10.,100.}){
     const auto ref=ps::fit_multi_change_v2(y,min,k,penalty);
     const auto opt=ps::fit_multi_change_pelt_v2(y,min,k,penalty);
     CHECK(opt.changes.size()<=k);
     CHECK(opt.segments.size()==opt.changes.size()+1);
     CHECK(std::abs(opt.penalized_sse-ref.penalized_sse)<1e-7);
     CHECK(std::abs(opt.residual_sse-ref.residual_sse)<1e-7);
     std::size_t last=0;
     for(const auto& segment:opt.segments){
      CHECK(segment.begin==last);
      CHECK(segment.end<=y.size()&&segment.end-segment.begin>=min);
      last=segment.end;
     }
     CHECK(last==y.size());
    }
 }
 const auto f=ps::fit_multi_change_pelt_v2(std::vector<double>{1,1,1,20,20,20,2,2,2},2,3,0.01);
 CHECK((f.changes==std::vector<std::size_t>{3,6}));
 CHECK(ps::fit_multi_change_pelt_v2(std::vector<double>{1,2,3,4,5,6},2,3,1000).changes.empty());
 bool failed=false;try{(void)ps::fit_multi_change_pelt_v2(std::vector<double>{},1,1,1);}catch(const ps::Error&){failed=true;}CHECK(failed);
 failed=false;try{(void)ps::fit_multi_change_pelt_v2(std::vector<double>{1,2},1,1,-1);}catch(const ps::Error&){failed=true;}CHECK(failed);
 failed=false;try{(void)ps::fit_multi_change_pelt_v2(std::vector<double>{1,std::numeric_limits<double>::infinity()},1,1,1);}catch(const ps::Error&){failed=true;}CHECK(failed);
 return 0;
}
