#include "phaseshift/interaction_v2.hpp"
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>
#define CHECK(x) do{if(!(x))throw std::runtime_error("factorial_holdout_v2: " #x);}while(false)
int main(){
 using namespace ps;
 const std::array<std::string,2> a={"a0","a1"},b={"b0","b1"};
 std::vector<FactorialCellV2> holdout;
 for(std::uint64_t seed=100;seed<120;++seed)
  for(const auto& x:a)for(const auto& y:b){
   double effect=x=="a1"&&y=="b1"?2.:0.;
   holdout.push_back({x,y,seed,effect,"ok"});
  }
 const std::vector<std::uint64_t> discovery={0,1,2,3,4};
 const auto result=fixed_factorial_holdout_v2(
  a,b,holdout,discovery,20,1,0.5,0.05);
 CHECK(result.tested_cohorts==20&&result.consistent_sign_cohorts==20);
 CHECK(result.supports_predeclared_effect);
 CHECK(result.state=="FROZEN_HOLDOUT_SUPPORT_REQUIRES_CAMPAIGN_SEAL");
 CHECK(std::abs(result.unadjusted_one_sided_p-1./1048576.)<1e-12);
 CHECK(!fixed_factorial_holdout_v2(
  a,b,holdout,discovery,20,-1,0.5,0.05).supports_predeclared_effect);
 std::vector<FactorialCellV2> null=holdout;
 for(auto& cell:null)if(cell.first=="a1"&&cell.second=="b1"&&cell.replicate_seed%2==0)
  cell.log_metric=-2.;
 const auto neutral=fixed_factorial_holdout_v2(a,b,null,discovery,20,1,0.5,0.05);
 CHECK(!neutral.supports_predeclared_effect);
 auto missing=holdout;missing.pop_back();
 CHECK(fixed_factorial_holdout_v2(
  a,b,missing,discovery,20,1,0.5,0.05).state=="INCOMPLETE_HOLDOUT");
 auto censored=holdout;censored[0].log_metric.reset();censored[0].status="wrong_answer";
 CHECK(fixed_factorial_holdout_v2(
  a,b,censored,discovery,20,1,0.5,0.05).state=="INVALID_OR_CENSORED_HOLDOUT");
 const auto rejects=[&](auto&& fn){
  try{fn();}catch(const Error&){return true;}return false;
 };
 CHECK(rejects([&]{(void)fixed_factorial_holdout_v2(
  a,b,holdout,{100},20,1,0.5,0.05);}));
 CHECK(rejects([&]{(void)fixed_factorial_holdout_v2(
  a,b,holdout,{3,3},20,1,0.5,0.05);}));
 CHECK(rejects([&]{(void)fixed_factorial_holdout_v2(
  a,b,holdout,discovery,20,0,0.5,0.05);}));
 CHECK(rejects([&]{(void)fixed_factorial_holdout_v2(
  a,b,holdout,discovery,20,1,0.5,std::numeric_limits<double>::quiet_NaN());}));
 auto duplicated=holdout;duplicated[0]=duplicated[1];
 CHECK(rejects([&]{(void)fixed_factorial_holdout_v2(
  a,b,duplicated,discovery,20,1,0.5,0.05);}));
 return 0;
}
