#include "phaseshift/statistics.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>
#define CHECK(x) do{if(!(x))throw std::runtime_error("family-null: " #x);}while(false)
namespace {
double exact_two_sided_sign_p(int positives,int count){
 const int tail=std::min(positives,count-positives);
 std::uint64_t term=1,sum=1;
 for(int j=1;j<=tail;++j){
  term=(term*static_cast<std::uint64_t>(count-j+1))/static_cast<std::uint64_t>(j);
  sum+=term;
 }
 return std::min(1.0,2.0*static_cast<double>(sum)/
  static_cast<double>(std::uint64_t{1}<<static_cast<unsigned>(count)));
}
}
int main(){
 constexpr int N=20,CAMPAIGNS=3000;
 CHECK(exact_two_sided_sign_p(10,20)==1.0);
 CHECK(exact_two_sided_sign_p(0,20)<0.00001);
 CHECK(exact_two_sided_sign_p(20,20)==exact_two_sided_sign_p(0,20));
 for(const int family_size:{1,2,4,8,16})
  for(int scenario=0;scenario<3;++scenario){
   std::uint64_t state=0x549a3e11a8277c0dULL+
      static_cast<std::uint64_t>(family_size*91+scenario*10007);
   int false_positive_campaigns=0;
   for(int campaign=0;campaign<CAMPAIGNS;++campaign){
    std::vector<int> positives(static_cast<std::size_t>(family_size),0);
    for(int block=0;block<N;++block){
     const int shared=static_cast<int>(ps::randbelow(state,2));
     for(int hypothesis=0;hypothesis<family_size;++hypothesis){
      // 0: mutually independent; 1: perfectly dependent (identical);
      // 2: correlated mixtures per sample (1/2 shared, 1/2 private).
      int bit=0;
      if(scenario==1)bit=shared;
      else if(scenario==0)bit=static_cast<int>(ps::randbelow(state,2));
      else bit=ps::randbelow(state,2)==0?shared:
       static_cast<int>(ps::randbelow(state,2));
      positives[static_cast<std::size_t>(hypothesis)]+=bit;
     }
    }
    std::vector<double> marginal;
    for(const auto hits:positives)
     marginal.push_back(exact_two_sided_sign_p(hits,N));
    const auto verdict=ps::holm_fwer(marginal,0.05);
    if(std::any_of(verdict.begin(),verdict.end(),[](bool x){return x;}))
     ++false_positive_campaigns;
   }
   const auto measured=static_cast<double>(false_positive_campaigns)/CAMPAIGNS;
   // Conservative 3k-campaign finite Monte Carlo smoke tolerance;
   // never interpret it as rigorous 95% population FWER certification.
   CHECK(measured<=0.075);
  }
 return 0;
}
