#include "phaseshift/statistics.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
namespace ps {
double sample_median(std::vector<double> v){
 if(v.empty())throw Error("median requires at least one case");
 for(auto x:v)if(!std::isfinite(x))throw Error("non-finite statistic sample");
 std::sort(v.begin(),v.end());auto n=v.size();return n%2?v[n/2]:0.5*(v[n/2-1]+v[n/2]);
}
static Interval bootstrap(const std::vector<double>& values,std::uint64_t state,double confidence,std::size_t replicates){
 if(values.size()<2||replicates<1000||replicates>100000||!std::isfinite(confidence)||confidence<0.5||confidence>=1.0)
  throw Error("invalid bootstrap sample size/confidence/iterations");
 const auto point=sample_median(values);
 std::vector<double> results;results.reserve(replicates);
 std::vector<double> chosen(values.size());
 for(std::size_t b=0;b<replicates;b++){
  for(std::size_t i=0;i<values.size();i++)chosen[i]=values[static_cast<std::size_t>(randbelow(state,values.size()))];
  results.push_back(sample_median(chosen));
 }
 std::sort(results.begin(),results.end());
 const auto alpha=(1.0-confidence)*0.5;
 const auto lo=std::min<std::size_t>(results.size()-1,static_cast<std::size_t>(std::floor(alpha*static_cast<double>(results.size()))));
 const auto hi=std::min<std::size_t>(results.size()-1,static_cast<std::size_t>(std::ceil((1.0-alpha)*static_cast<double>(results.size())))-1);
 return {point,results[lo],results[hi],values.size()};
}
Interval paired_case_bootstrap(const std::vector<double>& d,std::uint64_t seed,double confidence,std::size_t draws){return bootstrap(d,seed,confidence,draws);}
Interval case_bootstrap(const std::vector<double>& v,std::uint64_t seed,double confidence,std::size_t draws){return bootstrap(v,seed,confidence,draws);}
std::vector<bool> bh_fdr(const std::vector<double>& p,double q){
 if(!std::isfinite(q)||q<=0||q>=1)throw Error("invalid FDR q");
 std::vector<std::size_t> indexes(p.size());std::iota(indexes.begin(),indexes.end(),0);
 for(auto value:p)if(!std::isfinite(value)||value<0||value>1)throw Error("invalid p-value");
 std::stable_sort(indexes.begin(),indexes.end(),[&](auto a,auto b){return p[a]<p[b];});
 std::size_t accepted=0;
 for(std::size_t i=0;i<indexes.size();i++)if(p[indexes[i]]<=q*static_cast<double>(i+1)/static_cast<double>(p.size()))accepted=i+1;
 std::vector<bool> verdict(p.size(),false);for(std::size_t i=0;i<accepted;i++)verdict[indexes[i]]=true;return verdict;
}
std::vector<bool> holm_fwer(const std::vector<double>& p,double alpha){
 if(!std::isfinite(alpha)||alpha<=0||alpha>=1)throw Error("invalid Holm FWER alpha");
 for(const auto value:p)if(!std::isfinite(value)||value<0||value>1)throw Error("invalid Holm p-value");
 std::vector<std::size_t> sorted(p.size());std::iota(sorted.begin(),sorted.end(),0);
 std::stable_sort(sorted.begin(),sorted.end(),[&](std::size_t a,std::size_t b){return p[a]<p[b];});
 std::vector<bool> verdict(p.size(),false);
 for(std::size_t i=0;i<sorted.size();++i){
  const auto threshold=alpha/static_cast<double>(sorted.size()-i);
  if(p[sorted[i]]>threshold)break;
  verdict[sorted[i]]=true;
 }
 return verdict;
}
double paired_sign_flip_p(const std::vector<double>& d,std::uint64_t seed,std::size_t draws){
 if(d.size()<2||draws<100||draws>200000)throw Error("invalid sign-flip parameters");
 for(auto x:d)if(!std::isfinite(x))throw Error("non-finite matched difference");
 const auto observed=std::abs(std::accumulate(d.begin(),d.end(),0.0)/static_cast<double>(d.size()));
 std::size_t extreme=0;
 for(std::size_t b=0;b<draws;b++){
  double s=0;for(auto x:d)s+=(randbelow(seed,2)==0?-x:x);
  if(std::abs(s/static_cast<double>(d.size()))>=observed-1e-12)extreme++;
 }
 return static_cast<double>(extreme+1)/static_cast<double>(draws+1);
}
}
