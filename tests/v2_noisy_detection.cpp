#include "phaseshift/boundary_v2.hpp"
#include "phaseshift/slope_v2.hpp"
#include "phaseshift/core.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#define CHECK(e) do{if(!(e))throw std::runtime_error("noisy_v2: " #e);}while(false)
int main(int argc,char** argv){
 CHECK(argc==2);
 namespace fs=std::filesystem;
 const fs::path root(argv[1]);
 fs::remove_all(root);
 fs::create_directories(root);
 std::ofstream rows(root/"observations.csv");
 CHECK(rows.good());
 std::ofstream raw(root/"datasets.csv");
 CHECK(raw.good());
 raw<<"scenario,seed,index,value\n";
 rows<<"scenario,seed,true_cut_1,true_cut_2,dp_changes,pelt_changes,dp_sse,pelt_sse,localization_error,model_version\n";
 struct Aggregate {int campaigns=0,with_any=0,near_truth=0,screened=0,screened_near_truth=0;};
 std::map<std::string,Aggregate> summary;
 constexpr int N=48;
 for(int seed=0;seed<20;++seed){
  for(int scenario=0;scenario<5;++scenario){
   std::uint64_t state=0x5174a31f00dULL+static_cast<std::uint64_t>(seed*233+scenario*911);
   std::vector<double> y(N);
   int cut1=-1,cut2=-1;
   for(int i=0;i<N;++i){
    const double noise=ps::randbelow(state,2)?0.15:-0.15;
    double level=0.;
    if(scenario==1){cut1=24;if(i>=cut1)level=2.;}
    if(scenario==2){cut1=16;cut2=32;if(i>=cut1&&i<cut2)level=2.;}
    if(scenario==3)level=0.04*i; // smoothly varying, no discrete true jump
    if(scenario==4&&i==24)level=4.; // isolated outlier
    y[static_cast<std::size_t>(i)]=level+noise;
   }
   for(std::size_t i=0;i<y.size();++i)
    raw<<scenario<<","<<seed<<","<<i<<","<<y[i]<<"\n";
   const auto fit=ps::fit_multi_change_v2(y,3,3,2.5);
   const auto pruned=ps::fit_multi_change_pelt_v2(y,3,3,2.5);
   const auto error=std::abs(fit.penalized_sse-pruned.penalized_sse);
   CHECK(std::isfinite(error)&&error<1e-7*std::max(1.,std::abs(fit.penalized_sse)));
   int nearest=-1;
   if(cut1>=0){
    nearest=999;
    for(const auto at:fit.changes)nearest=std::min(nearest,std::abs(static_cast<int>(at)-cut1));
   }
   auto& agg=summary[std::to_string(scenario)];
   ++agg.campaigns;
   if(!fit.changes.empty())++agg.with_any;
   if(cut1>=0&&nearest<=2)++agg.near_truth;
   bool screened_any=false,screened_near=false;
   for(const auto at:fit.changes){
    const auto accepted=ps::screen_persistent_jump_v2(y,at,std::log(1.2));
    if(accepted){
     screened_any=true;
     if(cut1>=0&&std::abs(static_cast<int>(at)-cut1)<=2)screened_near=true;
    }
   }
   if(screened_any)++agg.screened;
   if(screened_near)++agg.screened_near_truth;
   rows<<scenario<<","<<seed<<","<<cut1<<","<<cut2<<","
       <<fit.changes.size()<<","<<pruned.changes.size()<<","
       <<fit.penalized_sse<<","<<pruned.penalized_sse<<","
       <<nearest<<",piecewise_constant_v2\n";
  }
  // Irregular x spacing and a true slope change; not a jump.
  std::uint64_t state=0x9da43d76bULL+static_cast<std::uint64_t>(seed);
  std::vector<std::int64_t> x(N);
  std::vector<double> y(N);
  for(int i=0;i<N;++i){
   x[i]=static_cast<std::int64_t>(3*i+(i%2));
   const double noise=ps::randbelow(state,2)?0.14:-0.14;
   y[i]=(i<24?0.25*static_cast<double>(x[i]):
     0.25*static_cast<double>(x[24])+1.2*static_cast<double>(x[i]-x[24]))+noise;
  }
  const auto slope=ps::fit_multi_slope_v2(x,y,3,3,2.5);
  CHECK(std::isfinite(slope.penalized_sse));
  int nearest=999;
  for(const auto index:slope.changes)
   nearest=std::min(nearest,std::abs(static_cast<int>(index)-24));
  auto& a=summary["slope"];
  ++a.campaigns;
  if(!slope.changes.empty())++a.with_any;
  if(nearest<=2)++a.near_truth;
  rows<<"slope,"<<seed<<",24,-1,"<<slope.changes.size()<<",-1,"
      <<slope.penalized_sse<<",nan,"<<nearest<<",piecewise_linear_v2\n";
 }
 rows.flush();
 raw.flush();
 CHECK(rows.good()&&raw.good());
 std::ofstream report(root/"scorecard.json");
 report<<"{\n  \"schema\": 1,\n  \"claim\": \"noisy synthetic diagnostic, not statistical calibration or SOTA\",\n"
       <<"  \"seeds_per_scenario\": 20,\n  \"results\": {\n";
 bool first=true;
 for(const auto& [label,a]:summary){
  if(!first)report<<",\n";
  first=false;
  report<<"    \""<<label<<"\": {\"campaigns\": "<<a.campaigns
        <<", \"nonempty_detection\": "<<a.with_any
        <<", \"near_planted_boundary\": "<<a.near_truth
        <<", \"persistent_jump_screened\": "<<a.screened
        <<", \"persistent_jump_near_truth\": "<<a.screened_near_truth<<"}";
 }
 report<<"\n  }\n}\n";
 CHECK(report.good());
 CHECK(summary.size()==6);
 for(const auto& [label,a]:summary){(void)label;CHECK(a.campaigns==20);}
 // A shaped candidate is not a confirmed discovery, but the frozen 20-seed
 // screen must reject these controls without tuning on their outcomes.
 CHECK(summary["0"].screened==0); // constant null
 CHECK(summary["3"].screened==0); // smooth trend, no level discontinuity
 CHECK(summary["4"].screened==0); // single-bin impulse, not a persistent jump
 CHECK(summary["1"].screened_near_truth>=18); // planted single cliff
 CHECK(summary["2"].screened_near_truth>=18); // two independent cliffs
 // Adversarial inputs: strict validation, short flank, nonfinite observation.
 CHECK(!ps::screen_persistent_jump_v2(std::vector<double>{0.,0.,1.,1.},2,0.1));
 bool rejected=false;
 try{(void)ps::screen_persistent_jump_v2(std::vector<double>{0.,0.,0.,1.,1.,1.},3,0.);}
 catch(const ps::Error&){rejected=true;}
 CHECK(rejected);
 rejected=false;
 try{(void)ps::screen_persistent_jump_v2(std::vector<double>{0.,0.,0.,INFINITY,1.,1.},3,0.1);}
 catch(const ps::Error&){rejected=true;}
 CHECK(rejected);
 return 0;
}
