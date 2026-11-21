#include "phaseshift/measurement_drift_v2.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("drift: " #x);}while(false)
int main(){
 const auto stable=ps::block_drift_v2({{3,100},{2,100},{1,100},{0,100},{4,100},{5,100}});
 CHECK(stable.eligible&&!stable.flagged&&stable.log_ratio==0);
 CHECK(stable.global_log_mad==0&&stable.drift_to_noise_ratio==0);
 const auto drift=ps::block_drift_v2({{0,100},{1,100},{2,100},{3,150},{4,300},{5,300},{6,300}});
 CHECK(drift.eligible&&drift.flagged);
 CHECK(drift.log_ratio>0&&drift.drift_to_noise_ratio>1);
 const auto extreme=ps::block_drift_v2(
  {{0,1e-300},{1,1e-300},{2,1e-300},
   {3,1e300},{4,1e300},{5,1e300}});
 CHECK(extreme.eligible&&extreme.flagged&&std::isfinite(extreme.log_ratio));
 CHECK(!ps::block_drift_v2({{0,100},{1,100}}).eligible);
 CHECK(!ps::block_drift_v2({{0,100},{0,100},{1,100},{2,100},{3,100},{4,100}}).eligible);
 CHECK(!ps::block_drift_v2({{0,100},{1,100},{2,100},{3,100},{4,100},
  {5,std::numeric_limits<double>::infinity()}}).eligible);
 return 0;
}
