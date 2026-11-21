#include "phaseshift/slope_v2.hpp"
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>
#define CHECK(x) do{if(!(x))throw std::runtime_error("slope oracle: " #x);}while(false)
static double loss(const std::vector<std::int64_t>& x,
 const std::vector<double>& y,std::size_t a,std::size_t b){
 const double m=static_cast<double>(b-a);
 double sx=0,sy=0,sxx=0,sxy=0,syy=0;
 for(auto i=a;i<b;++i){
  const auto dx=static_cast<double>(x[i]-x[a]);
  const auto dy=y[i];
  sx+=dx;sy+=dy;sxx+=dx*dx;sxy+=dx*dy;syy+=dy*dy;
 }
 const auto xx=sxx-sx*sx/m,xy=sxy-sx*sy/m,yy=syy-sy*sy/m;
 return std::max(0.,yy-xy*xy/xx);
}
static double brute(const std::vector<std::int64_t>& x,
 const std::vector<double>& y,std::size_t min,std::size_t max,double penalty){
 double best=std::numeric_limits<double>::infinity();
 std::vector<std::size_t> boundaries;
 std::function<void(std::size_t)> recurse=[&](std::size_t start){
  if(y.size()-start>=min){
   auto last=std::size_t{0},total=penalty*boundaries.size();
   for(auto end:boundaries){total+=loss(x,y,last,end);last=end;}
   total+=loss(x,y,last,y.size());
   best=std::min(best,total);
  }
  if(boundaries.size()>=max)return;
  for(std::size_t next=start+min;next+min<=y.size();++next){
   boundaries.push_back(next);recurse(next);boundaries.pop_back();
  }
 };
 recurse(0);
 return best;
}
int main(){
 const std::vector<std::int64_t> x={0,1,2,3,4,5,6,7,8};
 const std::vector<double> y={0,1,2,3,4,5,20,23,26};
 const auto fit=ps::fit_multi_slope_v2(x,y,3,2,0.01);
 CHECK((fit.changes==std::vector<std::size_t>{6}));
 CHECK(std::abs(fit.segments[0].slope-1.)<1e-10);
 CHECK(std::abs(fit.segments[1].slope-3.)<1e-10);
 for(int mask=0;mask<256;++mask){
  std::vector<double> values;
  for(int i=0;i<8;++i)values.push_back(mask&(1<<i)?3.:0.);
  for(const auto min:{3U,4U})
   for(const auto k:{0U,1U,2U})
    for(const auto p:{0.,0.5,6.}){
     const auto fit2=ps::fit_multi_slope_v2(
      std::span<const std::int64_t>(x.data(),8),values,min,k,p);
     CHECK(fit2.changes.size()<=k);
     CHECK(std::abs(fit2.penalized_sse-brute(
       std::vector<std::int64_t>(x.begin(),x.begin()+8),values,min,k,p))<1e-7);
    }
 }
 const std::vector<std::int64_t> large={
  std::numeric_limits<std::int64_t>::max()-5,
  std::numeric_limits<std::int64_t>::max()-4,
  std::numeric_limits<std::int64_t>::max()-3,
  std::numeric_limits<std::int64_t>::max()-2,
  std::numeric_limits<std::int64_t>::max()-1,
  std::numeric_limits<std::int64_t>::max()};
 const auto huge=ps::fit_multi_slope_v2(large,
  std::vector<double>{1,2,3,4,5,6},3,1,100.);
 CHECK(huge.changes.empty()&&std::abs(huge.segments[0].slope-1.)<1e-8);
 bool failed=false;
 try{(void)ps::fit_multi_slope_v2(x,std::vector<double>{1,2},3,1,1.);}
 catch(const ps::Error&){failed=true;}CHECK(failed);
 failed=false;
 try{(void)ps::fit_multi_slope_v2(
  std::vector<std::int64_t>{1,1,2},std::vector<double>{1,2,3},3,1,1.);}
 catch(const ps::Error&){failed=true;}CHECK(failed);
 failed=false;
 try{(void)ps::fit_multi_slope_v2(
  std::vector<std::int64_t>{1,2,3},
  std::vector<double>{1,std::numeric_limits<double>::quiet_NaN(),3},3,1,1.);}
 catch(const ps::Error&){failed=true;}CHECK(failed);
 return 0;
}
