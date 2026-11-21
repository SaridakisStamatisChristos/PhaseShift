#include "phaseshift/evidence_view_v2.hpp"
#include <stdexcept>
#include <cmath>
#define CHECK(e) do{if(!(e))throw std::runtime_error("evidence_view_v2: " #e);}while(false)
int main(){
 using namespace ps;
 DiscoveryEvidenceViewV2 view(2);
 Observation valid;valid.algorithm="calibration";valid.status="ok";valid.cpu_ns=2000000;valid.peak_rss_bytes=1000;
 Observation failed;failed.algorithm="calibration";failed.status="wrong_answer";failed.cpu_ns=-1;
 view.append_committed("case-one",{valid,failed},std::nullopt);
 const auto& a=view.lookup("case-one");
 CHECK(!a.metric&&a.valid_samples==1&&a.failed_samples==1&&a.missing_cpu==1);
 CHECK(a.observed_cpu_ms==2.&&a.peak_rss_bytes==1000);
 Observation again=valid;again.cpu_ns=3000000;
 view.append_committed("case-two",{again},2.);
 CHECK(view.size()==2&&view.lookup("case-two").metric==2.);
 CHECK(view.lookup("case-two").observed_cpu_ms==3.);
 const auto reject=[&](auto&& fn){
  try{fn();}catch(const Error&){return true;}return false;
 };
 CHECK(reject([&]{view.lookup("missing");}));
 CHECK(reject([&]{view.append_committed("case-one",{valid},1.);}));
 CHECK(reject([&]{view.append_committed("case-three",{valid},1.);}));
 CHECK(reject([&]{DiscoveryEvidenceViewV2 bad(0);}));
 CHECK(reject([&]{DiscoveryEvidenceViewV2 bad(20001);}));
 DiscoveryEvidenceViewV2 fresh(2);
 CHECK(reject([&]{fresh.append_committed("empty",{},1.);}));
 CHECK(reject([&]{fresh.append_committed("bad",{valid},std::numeric_limits<double>::quiet_NaN());}));
 CHECK(reject([&]{fresh.append_committed("",{valid},1.);}));
 Observation overflow=valid;overflow.cpu_ns=std::numeric_limits<std::int64_t>::max();
 CHECK(reject([&]{fresh.append_committed("too-much",{overflow,overflow},1.);}));
 return 0;
}
