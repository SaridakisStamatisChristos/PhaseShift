#include "phaseshift/algorithms.hpp"
#include "phaseshift/core.hpp"
#include <algorithm>
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
  ps::Case c;c.family="calibration";c.x=4;c.threshold=8;c.negative=false;
  auto low=ps::make_case(c);
  require(ps::execute(low,"calibration").work==14ULL,"legacy calibration low unchanged");
  c.x=8;auto high=ps::make_case(c);
  require(ps::execute(high,"calibration").work==10008ULL,"legacy calibration high unchanged");
  std::cout<<"PASS opt-in multistep and smooth workers, null context and unchanged legacy oracle\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
