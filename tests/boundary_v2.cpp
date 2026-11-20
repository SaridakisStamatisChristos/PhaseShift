#include "phaseshift/boundary_v2.hpp"
#include "phaseshift/core.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>
static double brute(const std::vector<double>& y,std::size_t minimum,std::size_t k,double penalty){
 double optimum=std::numeric_limits<double>::infinity();
 std::vector<std::size_t> boundaries;
 std::function<void(std::size_t)> search=[&](std::size_t from){
  if(y.size()-from>=minimum){
   auto all=boundaries;all.push_back(y.size());
   std::size_t begin=0;double cost=penalty*static_cast<double>(boundaries.size());
   for(const auto end:all){
    double mean=0;
    for(std::size_t i=begin;i<end;i++)mean+=y[i];
    mean/=static_cast<double>(end-begin);
    for(std::size_t i=begin;i<end;i++)cost+=(y[i]-mean)*(y[i]-mean);
    begin=end;
   }
   optimum=std::min(optimum,cost);
  }
  if(boundaries.size()>=k)return;
  for(std::size_t point=from+minimum;point+minimum<=y.size();++point){
   boundaries.push_back(point);search(point);boundaries.pop_back();
  }
 };
 search(0);return optimum;
}
int main(){
 using ps::fit_multi_change_v2;
 {
  const std::vector<double> y={0,0,10,10,0,0};
  auto f=fit_multi_change_v2(y,2,2,0.1);
  assert((f.changes==std::vector<std::size_t>{2,4}));
  assert(f.segments.size()==3&&f.residual_sse==0);
 }
 const std::vector<double> smooth={0,1,2,3,4,5};
 assert(fit_multi_change_v2(smooth,2,3,100).changes.empty());
 for(int mask=0;mask<256;mask++){
  std::vector<double> y;
  for(int i=0;i<8;i++)y.push_back((mask&(1<<i))?4:0);
  for(std::size_t minimum=1;minimum<=3;minimum++)
   for(std::size_t k=0;k<=3;k++)
    for(double penalty:{0.,0.5,10.,100.}){
     const auto f=fit_multi_change_v2(y,minimum,k,penalty);
     assert(std::abs(f.penalized_sse-brute(y,minimum,k,penalty))<1e-7);
     assert(f.changes.size()<=k);
     for(const auto& s:f.segments)assert(s.end-s.begin>=minimum);
    }
 }
 bool bad=false;
 try{(void)fit_multi_change_v2(std::vector<double>{},1,1,1);}catch(const ps::Error&){bad=true;}assert(bad);
 bad=false;
 try{(void)fit_multi_change_v2(std::vector<double>{1,2},0,1,1);}catch(const ps::Error&){bad=true;}assert(bad);
 bad=false;
 try{(void)fit_multi_change_v2(std::vector<double>{1,2},1,1,-1);}catch(const ps::Error&){bad=true;}assert(bad);
 bad=false;
 try{(void)fit_multi_change_v2(std::vector<double>{1,std::numeric_limits<double>::quiet_NaN()},1,1,1);}catch(const ps::Error&){bad=true;}assert(bad);
 return 0;
}
