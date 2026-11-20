#include "phaseshift/scaling.hpp"
#include "phaseshift/statistics.hpp"
#include "phaseshift/core.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
static void require(bool x,const char* why){if(!x)throw std::runtime_error(why);}
static double discontinuous_cost(double n){
 // Two continuous but different log-log slopes with a misleading near-linear
 // initial region. This is a fixture, not a measured physical algorithm.
 const auto base=n/16.0;
 return n<=128?base:8.0*std::pow(n/128.0,3.0);
}
static double smooth_cost(double n){return n*std::log(n);}
static std::vector<std::pair<double,double>> discovery(bool cliff){
 std::vector<std::pair<double,double>> points;
 for(int n=8;n<=4096;n*=2)points.emplace_back(n,cliff?discontinuous_cost(n):smooth_cost(n));
 return points;
}
int main(int argc,char** argv){try{
 require(argc==2,"campaign evidence output path required");
 const auto out=std::filesystem::path(argv[1]);std::filesystem::create_directories(out);
 // B10: train only on sparse sizes; freeze a single selected candidate and
 // evaluate independently on previously unmeasured sizes and fresh seeds.
 const auto initial=discovery(true),negative=discovery(false);
 const auto found=ps::detect_scaling_boundary(initial);
 require(found.candidate,"B10 piecewise fixture not detected");
 require(!ps::detect_scaling_boundary(negative).candidate,"B10 smooth n log n false positive");
 const std::vector<int> fresh={24,48,512+127,1024+127};
 for(int n:fresh)for(const auto& original:initial)require(n!=static_cast<int>(original.first),"B10 holdout size reused");
 std::vector<double> shifts;std::ofstream csv(out/"b10_holdout.csv");
 csv<<"case_seed,left_n0,left_n1,right_n0,right_n1,slope_difference,fixture\n";
 for(int seed=0;seed<20;seed++){
  auto observe=[seed](int n){
   std::uint64_t state=static_cast<std::uint64_t>(n)*0x9e3779b97f4a7c15ULL+static_cast<std::uint64_t>(seed)*131ULL+0x53a7b190ULL;
   const double noise=1.0+(static_cast<double>(ps::randbelow(state,101))-50.0)/10000.0;
   return discontinuous_cost(static_cast<double>(n))*noise;
  };
  const auto ls=std::log(observe(fresh[1])/observe(fresh[0]))/std::log(static_cast<double>(fresh[1])/fresh[0]);
  const auto rs=std::log(observe(fresh[3])/observe(fresh[2]))/std::log(static_cast<double>(fresh[3])/fresh[2]);
  shifts.push_back(rs-ls);
  csv<<seed<<','<<fresh[0]<<','<<fresh[1]<<','<<fresh[2]<<','<<fresh[3]<<','<<rs-ls<<",deterministic_synthetic\n";
 }
 const auto validated=ps::evaluate_scaling_holdout(shifts,found.slope_right-found.slope_left,713);
 require(validated.confirmed,"B10 fresh-size holdout did not confirm piecewise fixture");
 const auto wrong_direction=ps::evaluate_scaling_holdout(shifts,-2.0,713);
 require(!wrong_direction.confirmed,"B10 incorrect frozen direction passed holdout");
 const std::vector<double> no_shift(20,0.0);
 require(!ps::evaluate_scaling_holdout(no_shift,2.0,713).confirmed,"B10 null holdout was confirmed");
 ps::Json b10;b10.put("campaign","B10");b10.put("fixture","deterministic two-regime injected synthetic cost");
 b10.put("selected_candidate",found.candidate);b10.put("smooth_null_abstained",true);
 b10.put("fresh_sizes_used",static_cast<std::int64_t>(fresh.size()));
 b10.put("fresh_seed_groups",static_cast<std::int64_t>(shifts.size()));
 b10.put("fresh_size_holdout_confirmed",validated.confirmed);
 b10.put("wrong_direction_rejected",!wrong_direction.confirmed);
 b10.put("null_holdout_rejected",true);
 ps::write_file((out/"b10.json").string(),b10.str());

 // B11: emulate systematic worker-order drift. ABBA / BAAB pairs have the
 // same mean execution position for A and B. Under zero true effect, later
 // temporary drift + large case-level variability should not be CONFIRMED.
 std::ofstream noise(out/"b11_case_nulls.csv");noise<<"campaign,seed,simulated_case_log_ratio\n";
 int false_positive=0;
 for(int trial=0;trial<40;trial++){
  std::vector<double> cases;
  for(int seed=0;seed<20;seed++){
   const bool abba=((seed+trial)%2)==0;
   const std::string order=abba?"ABBA":"BAAB";
   double sum_a=0,sum_b=0;
   for(int pos=0;pos<4;pos++){
    const double measured=100.0*(1.0+static_cast<double>(trial%7)*0.0002+0.07*static_cast<double>(pos));
    if(order[static_cast<std::size_t>(pos)]=='A')sum_a+=measured;
    else sum_b+=measured;
   }
   const double base=std::log(sum_a/sum_b);
   // Inject a deliberately unstable, nonstationary case-level contrast.
   // A symmetric seed order prevents false discoveries by construction; the
   // test is an adversarial null, not a statistical power certification.
   const double nonstationary=(seed%2==0?-1.0:1.0)*(0.16+0.01*(trial%5));
   cases.push_back(base+nonstationary);
   noise<<trial<<','<<seed<<','<<base+nonstationary<<'\n';
  }
  const auto interval=ps::paired_case_bootstrap(cases,static_cast<std::uint64_t>(trial)*1023ULL+371,0.99);
  const auto p=ps::paired_sign_flip_p(cases,static_cast<std::uint64_t>(trial)*1023ULL+371);
  const bool confirm=(p<=0.05)&&(interval.lower>std::log(1.2)||interval.upper< -std::log(1.2));
  if(confirm)false_positive++;
 }
 require(false_positive==0,"B11 nonstationary paired null falsely confirmed");
 ps::Json b11;b11.put("campaign","B11");b11.put("fixture","simulated ABBA/BAAB nonstationary drift and oscillating null");
 b11.put("null_trials",static_cast<std::int64_t>(40));b11.put("false_confirmations",static_cast<std::int64_t>(false_positive));
 b11.put("limitation","synthetic drift fixture does not establish control of all physical CPU nonstationarity");
 ps::write_file((out/"b11.json").string(),b11.str());
 std::cout<<"PASS B10: piecewise discovery confirmed on fresh sizes, smooth/null/deceptive direction rejected\n";
 std::cout<<"PASS B11: balanced order, unstable paired nulls 0/40 false confirmations (simulated)\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"research campaign: "<<e.what()<<'\n';return 1;}}
