#include "phaseshift/measurement_drift_v2.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("drift: " #x);}while(false)
int main(){
 const auto stable=ps::block_drift_v2({{3,100},{2,100},{1,100},{0,100},{4,100},{5,100}});
 CHECK(stable.eligible&&!stable.flagged&&stable.log_ratio==0);
 const auto drift=ps::block_drift_v2({{0,100},{1,100},{2,100},{3,150},{4,300},{5,300},{6,300}});
 CHECK(drift.eligible&&drift.flagged);
 CHECK(!ps::block_drift_v2({{0,100},{1,100}}).eligible);
 CHECK(!ps::block_drift_v2({{0,100},{0,100},{1,100},{2,100},{3,100},{4,100}}).eligible);
 CHECK(!ps::block_drift_v2({{0,100},{1,100},{2,100},{3,100},{4,100},
  {5,std::numeric_limits<double>::infinity()}}).eligible);
 return 0;
}
