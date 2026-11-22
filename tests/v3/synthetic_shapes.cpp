#include "phaseshift/algorithms.hpp"
#include "phaseshift/core.hpp"
#include <algorithm>
#include <array>
#include <vector>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

static void require(bool ok,const std::string& message){
 if(!ok)throw std::runtime_error("v3 analytic calibration shapes: "+message);
}
int main(){
 try{
  const auto registered=ps::algorithms_for("calibration");
  require(std::find(registered.begin(),registered.end(),"calibration_steps_v3")!=registered.end(),"steps registration");
  require(std::find(registered.begin(),registered.end(),"calibration_smooth_v3")!=registered.end(),"smooth registration");
  for(std::int64_t threshold:{1,4,12,19}){
   for(int negative=0;negative<2;++negative){
    for(std::int64_t x=0;x<=24;++x){
     ps::Case input;input.family="calibration";input.x=x;input.threshold=threshold;
     input.negative=negative!=0;input.seed=137;input.generator_version=2;
     const auto c=ps::make_case(input);
     const auto expected=ps::oracle(c).values;
     const auto steps=ps::execute(c,"calibration_steps_v3");
     const auto smooth=ps::execute(c,"calibration_smooth_v3");
     require(steps.values==expected&&smooth.values==expected,"independent correctness oracle");
     const std::uint64_t v=static_cast<std::uint64_t>(x),t=static_cast<std::uint64_t>(threshold);
     const auto want_step=negative?1024ULL:1024ULL+4ULL*v+(v>=t?8192ULL:0ULL)+(v>=t+4ULL?16384ULL:0ULL);
     const auto want_smooth=negative?1024ULL:1024ULL+40ULL*v+2ULL*v*v;
     require(steps.work==want_step,"independent two-step truth");
     require(smooth.work==want_smooth,"independent smooth truth");
    }
   }
  }
  for(auto name:{"calibration_steps_v3","calibration_smooth_v3"}){
   for(auto invalid:std::array<std::vector<std::int64_t>,2>{
       std::vector<std::int64_t>{1025,8,0},
       std::vector<std::int64_t>{4,1025,0}}){
    ps::Case malformed;malformed.family="calibration";malformed.bytes=ps::encode_values(invalid);
    bool rejected=false;
    try{(void)ps::execute(malformed,name);}catch(const ps::Error&){rejected=true;}
    require(rejected,"reject unbounded synthetic workload");
   }
  }
  require(std::find(registered.begin(),registered.end(),"calibration_seed_shared_null_v3")!=registered.end(),"seed shared null registration");
  for(const std::uint64_t seed:{1ULL,137ULL,(1ULL<<62)+127ULL}){
   for(std::int64_t threshold:{3,8,19}){
    std::uint64_t expected=0;
    for(std::int64_t x=0;x<=24;++x){
     ps::Case c;c.family="calibration";c.x=x;c.threshold=threshold;c.negative=true;c.seed=seed;
     auto actual=ps::make_case(c);
     const auto output=ps::execute(actual,"calibration_seed_shared_null_v3");
     require(output.values==ps::oracle(actual).values,"shared null correctness oracle");
     if(x==0)expected=output.work;
     require(output.work==expected,"same matched seed must share exact work across coordinates");
     const auto formula=1024ULL<<((ps::splitmix64(seed^0x5d6c9e3a712bf084ULL)>>7)&3ULL);
     require(output.work==formula,"seed-only null independent operation count");
    }
   }
  }
  for(const auto negative: {false,true}){
   ps::Case c;c.family="calibration";c.x=2;c.threshold=8;c.negative=negative;c.seed=137;
   if(!negative){
    bool rejected=false;
    try{(void)ps::execute(ps::make_case(c),"calibration_seed_shared_null_v3");}
    catch(const ps::Error&){rejected=true;}
    require(rejected,"shared null refuses non-null context");
   }
  }
  ps::Case c;c.family="calibration";c.x=4;c.threshold=8;c.negative=false;
  auto low=ps::make_case(c);
  require(ps::execute(low,"calibration").work==14ULL,"legacy calibration low unchanged");
  c.x=8;auto high=ps::make_case(c);
  require(ps::execute(high,"calibration").work==10008ULL,"legacy calibration high unchanged");
  std::cout<<"PASS opt-in multistep and smooth workers, null context and unchanged legacy oracle\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
