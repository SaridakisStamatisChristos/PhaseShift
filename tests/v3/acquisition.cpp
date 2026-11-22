#include "phaseshift/v3/acquisition.hpp"
#include "phaseshift/v3/conditional_pair.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("v3 cost falsifier: " #x);}while(false)
using namespace ps;
using namespace ps::v3;
template<class F>void rejects(F&& f){bool rejected=false;try{f();}catch(const Error&){rejected=true;}CHECK(rejected);}
int main(){
 EmpiricalCostModelV3 m;
 CHECK(!m.estimate(0).enabled&&m.estimate(0).wall==0&&m.estimate(0).penalty()==1.);
 rejects([&]{m.add(0,Cohort::Confirmation,{100,50},true);});
 rejects([&]{m.add(0,Cohort::Discovery,{0,50},true);});
 rejects([&]{m.add(0,Cohort::Discovery,{100,-1},true);});
 CHECK(m.size()==0);
 // Frozen prequential split: indices 3 and 7 must be held out.
 const int ctx[]={0,1,0,1,1,0,1,1};
 for(int i=0;i<8;++i){
  const std::int64_t w=ctx[i]?1000:100;
  m.add(static_cast<std::size_t>(ctx[i]),Cohort::Discovery,{w,w/2},true);
 }
 const auto cheap=m.estimate(0),expensive=m.estimate(1),cold=m.estimate(42);
 CHECK(cheap.training==6&&cheap.heldout==2&&cheap.covered==2);
 CHECK(cheap.enabled&&expensive.enabled);
 CHECK(cheap.wall==100&&expensive.wall==1000&&cold.wall==100);
 CHECK(cheap.cpu_known&&cheap.cpu==50&&expensive.cpu==500);
 CHECK(cheap.wall_upper==1000&&cheap.cpu_upper==500);
 CHECK(expensive.penalty()==2.&&cheap.penalty()==1.);
 const AcquisitionComponentsV3 c{0.5,0.5,0.5,0.5,0.5,false};
 CHECK(std::abs(acquisition_score_v3(c,cheap)-0.5)<1e-12);
 CHECK(std::abs(acquisition_score_v3(c,expensive)-0.25)<1e-12);
 CHECK(std::abs(acquisition_score_v3(c,expensive,AcquisitionAblationV3::NoCost)-0.5)<1e-12);
 CHECK(acquisition_score_v3(c,cheap,AcquisitionAblationV3::NoUncertainty)<0.5);
 CHECK(acquisition_score_v3(c,cheap,AcquisitionAblationV3::NoDiversity)<0.5);
 CHECK(acquisition_score_v3(c,cheap,AcquisitionAblationV3::NoInteraction)<0.5);
 auto replicate=c;replicate.replication=true;
 CHECK(acquisition_score_v3(replicate,cheap,AcquisitionAblationV3::NoReplicate)==0.);
 auto bad=c;bad.contrast=std::numeric_limits<double>::quiet_NaN();
 rejects([&]{(void)acquisition_score_v3(bad,cheap);});
 bad.contrast=-1.;rejects([&]{(void)acquisition_score_v3(bad,cheap);});
 for(int j=0;j<3;++j)m.add(0,Cohort::Discovery,{100,std::nullopt},true);
 m.add(0,Cohort::Discovery,{999999999,std::nullopt},true);
 const auto drift=m.estimate(0);
 CHECK(drift.heldout==3&&drift.covered==2&&drift.wall_upper==1000);
 CHECK(!drift.enabled&&drift.penalty()==1.); // undercoverage + heldout not fitted
 m.add(0,Cohort::Discovery,{5000,std::nullopt},false);
 CHECK(m.estimate(0).failed==1&&m.estimate(0).wall_upper==1000);
 // Replay through actual policy interface; altered consumed cost changes model digest.
 const std::vector<LazySearchSpace::Axis> axes{{"group",{"rare","common"}},{"x",{"0","1","2","3","4","5","6","7"}}};
 const std::vector<AdmissibleSearchSpace::Subspace> strata{{{"group",{"rare"}}},{{"group",{"common"}}}};
 AdmissibleSearchSpace legal(axes,strata);
 ConditionalPairPortfolio p(legal,axes,"x",strata,71,20,1.2,4,true,true);
 ConditionalPairPortfolio q(legal,axes,"x",strata,71,20,1.2,4,true,true);
 CHECK(p.evidence_digest()==q.evidence_digest());
 for(std::size_t i=0;i<20;++i){
  const auto a=p.next(),b=q.next();CHECK(a&&b&&*a==*b);
  CHECK(!a->acquisition_audit.empty());
  CHECK(legal.index_of(a->left)&&legal.index_of(a->right));
  if(i==0)CHECK(a->left[0].second=="rare");
  if(i==1)CHECK(a->left[0].second=="common");
  if(i==0)rejects([&]{p.on_committed(*a,Cohort::Discovery,0.3);});
  const PairMeasuredCost cost{a->left[0].second=="rare"?1000:100,50};
  p.on_committed(*a,Cohort::Discovery,0.3,cost);
  q.on_committed(*b,Cohort::Discovery,0.3,cost);
  CHECK(p.evidence_digest()==q.evidence_digest());
 }
 CHECK(!p.next());
 std::cout<<"PASS prequential cost model, drift/failure fallback, score ablations, exact legal cost-aware replay\n";
}
