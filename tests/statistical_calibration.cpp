#include "phaseshift/statistics.hpp"
#include "phaseshift/experiment.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
namespace {
void require(bool test,const std::string& what){if(!test)throw ps::Error("statistical calibration: "+what);}
void invalid(const std::string& field,double value){
 ps::Json j;j.put("schema_version",static_cast<std::int64_t>(1));j.put("problem_family","calibration");
 ps::Json detection;detection.put(field.c_str(),value);j.put("detection",std::move(detection));
 bool rejected=false;try{(void)ps::config_from_json(j);}catch(const ps::Error&){rejected=true;}
 require(rejected,"bad parameter not rejected: "+field);
}
}
int main(){try{
 // Reference Benjamini-Hochberg step-up test, including the step-up
 // property when a smaller p fails its own per-rank threshold.
 auto accept=ps::bh_fdr({0.001,0.04,0.025,0.7},0.05);
 require(accept==std::vector<bool>({true,false,true,false}),"BH known-reference decisions");
 require(ps::bh_fdr({0.90,0.87,1.0},0.01)==std::vector<bool>({false,false,false}),"BH null abstention");
 const auto holm=ps::holm_fwer({0.001,0.04,0.025,0.7},0.05);
 require(holm==std::vector<bool>({true,false,true,false}),"Holm known-reference decisions");
 require(ps::holm_fwer({0.01,0.015,0.03},0.05)==std::vector<bool>({true,true,true}),"Holm sequential thresholds");
 require(ps::holm_fwer({0.9,1.0},0.01)==std::vector<bool>({false,false}),"Holm null abstention");
 require(ps::holm_fwer({},0.05).empty(),"empty predeclared family");
 {
  for(const double invalid_p:{-0.1,1.1,std::numeric_limits<double>::quiet_NaN()}){
   bool rejected=false;try{(void)ps::holm_fwer({invalid_p},0.05);}catch(const ps::Error&){rejected=true;}
   require(rejected,"Holm invalid p-value rejection");
  }
  bool rejected=false;try{(void)ps::holm_fwer({0.01},1.0);}catch(const ps::Error&){rejected=true;}
  require(rejected,"Holm invalid alpha rejection");
 }
 auto positive=ps::paired_case_bootstrap(std::vector<double>(25,0.4),42,0.99,2000);
 require(std::abs(positive.lower-0.4)<1e-10&&std::abs(positive.upper-0.4)<1e-10,"positive stable effect interval");
 auto zero=ps::paired_sign_flip_p(std::vector<double>(25,0.0),17,2000);
 require(zero==1.0,"exactly zero matched differences give p=1");
 auto sharp=ps::paired_sign_flip_p(std::vector<double>(25,1.0),17,2000);
 require(sharp<0.01,"all same-sign pairs reject null");
 // Empirically calibrated finite-sample synthetic null; deterministic
 // independent Rademacher paired differences. Threshold is conservative,
 // not a proof of statistical validity for all distributions.
 std::uint64_t seed=0x1010020242ULL;int false_positives=0;
 for(int trial=0;trial<120;trial++){
  std::vector<double> diffs(25);
  for(auto& d:diffs)d=ps::randbelow(seed,2)==0?-1.0:1.0;
  const auto p=ps::paired_sign_flip_p(diffs,static_cast<std::uint64_t>(trial)+2231,1000);
  if(p<0.01)false_positives++;
 }
 require(false_positives<=6,"deterministic null simulation excessive false rejections");
 {
  ps::Json j;j.put("schema_version",static_cast<std::int64_t>(1));j.put("problem_family","calibration");
  ps::Json detection;detection.put("min_practical_ratio",1.35);detection.put("confirmation_confidence",0.995);
  detection.put("fdr_q",0.025);detection.put("multiple_testing","bh_fdr");
  j.put("detection",std::move(detection));
  const auto c=ps::config_from_json(j);
  require(c.confirmation_confidence==0.995&&c.fdr_q==0.025,"configurable statistical policy preserved");
 }
 invalid("confirmation_confidence",1.0);invalid("confirmation_confidence",0.4);
 invalid("fdr_q",0.0);invalid("fdr_q",1.0);
 std::cout<<"PASS: BH reference, resampling controls, null false positives="<<false_positives<<"/120, configurable 99% policy\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
