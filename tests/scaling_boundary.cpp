#include "phaseshift/scaling.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
int main(){
 try{
  std::vector<std::pair<double,double>> piecewise,smooth,linear,flat;
  for(int i=0;i<12;i++){
   const double n=16.0*std::pow(2.0,static_cast<double>(i));
   const double change=16.0*std::pow(2.0,6.0);
   piecewise.emplace_back(n,n<=change?n:change*std::pow(n/change,3.0));
   smooth.emplace_back(n,n*std::log(n));
   linear.emplace_back(n,7.0*n);
   flat.emplace_back(n,500.0);
  }
  const auto sharp=ps::detect_scaling_boundary(piecewise);
  if(!sharp.candidate||(sharp.split<6||sharp.split>7)||sharp.slope_right-sharp.slope_left<1.3||sharp.bic_gain<10.0)
   throw std::runtime_error("known slope change not localized");
  for(const auto& null:{smooth,linear,flat})if(ps::detect_scaling_boundary(null).candidate)
   throw std::runtime_error("smooth or homogeneous growth misclassified as a discrete scaling change");
  const auto stable=ps::evaluate_scaling_holdout(std::vector<double>(20,2.0),2.0,721);
  if(!stable.confirmed||stable.ci_lower<1.9||stable.p_value>0.05)
   throw std::runtime_error("independent stable holdout did not support known slope change");
  const auto null_holdout=ps::evaluate_scaling_holdout(std::vector<double>(20,0.0),2.0,721);
  if(null_holdout.confirmed)throw std::runtime_error("null holdout was wrongly confirmed");
  const auto reverse=ps::evaluate_scaling_holdout(std::vector<double>(20,-2.0),2.0,721);
  if(reverse.confirmed)throw std::runtime_error("contradictory held-out slope was wrongly confirmed");
  if(ps::evaluate_scaling_holdout(std::vector<double>(19,2.0),2.0,721).confirmed)
   throw std::runtime_error("insufficient independent seed pairs wrongly confirmed");
  std::cout<<"Synthetic piecewise scaling candidate at split "<<sharp.split
           <<"; independent confirmation still required\n";return 0;
 }catch(const std::exception& e){std::cerr<<"Scaling classification failed: "<<e.what()<<'\n';return 1;}
}
