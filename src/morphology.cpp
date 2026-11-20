#include "phaseshift/morphology.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <set>
namespace ps {
Json morphology_features(const Case& c){
 const auto values=decode_values(c.bytes);Json f;
 f.put("generator_version",std::string("generator-v")+std::to_string(c.generator_version));
 if(c.family=="integer_array"||c.family=="topk"){
  const auto n=values.size();f.put("observed_n",static_cast<std::int64_t>(n));
  std::map<std::int64_t,std::size_t> counts;std::size_t decreases=0,longest=0,run=0;
  for(std::size_t i=0;i<n;i++){
   counts[values[i]]++;
   if(i>0&&values[i]<values[i-1])decreases++;
   if(i>0&&values[i]==values[i-1])run++;else run=1;
   longest=std::max(longest,run);
  }
  f.put("unique_count",static_cast<std::int64_t>(counts.size()));
  f.put("adjacent_disorder_fraction",n>1?static_cast<double>(decreases)/static_cast<double>(n-1):0.0);
  f.put("duplicate_fraction",n?1.0-static_cast<double>(counts.size())/static_cast<double>(n):0.0);
  f.put("max_equal_run",static_cast<std::int64_t>(longest));
  double entropy=0;
  for(const auto& [value,count]:counts){(void)value;auto p=static_cast<double>(count)/static_cast<double>(n);entropy-=p*std::log2(p);}
  f.put("entropy_bits",entropy);
  if(n){const auto [min_it,max_it]=std::minmax_element(values.begin(),values.end());
   f.put("minimum",*min_it);f.put("maximum",*max_it);
   f.put("range_width",static_cast<double>(static_cast<long double>(*max_it)-static_cast<long double>(*min_it)));
  }else f.put("range_width",0.0);
 }else if(c.family=="graph"){
  if(values.size()<3||values[0]<1||(values.size()-3)%3)throw Error("invalid graph feature input");
  auto n=static_cast<std::size_t>(values[0]);
  if(values[1]<0||values.size()!=3+static_cast<std::size_t>(values[1])*3)throw Error("invalid graph feature edge count");
  std::vector<std::size_t> degree(n,0);std::size_t zero=0,loops=0,parallel=0;
  std::set<std::pair<std::int64_t,std::int64_t>> seen;
  for(std::size_t i=3;i<values.size();i+=3){auto a=values[i],b=values[i+1],w=values[i+2];
   if(a<0||b<0||a>=values[0]||b>=values[0]||w<0)throw Error("invalid graph feature edge");
   degree[static_cast<std::size_t>(a)]++;
   if(w==0)zero++;
   if(a==b)loops++;
   if(!seen.emplace(a,b).second)parallel++;
  }
  const auto arcs=static_cast<std::size_t>(values[1]);
  f.put("observed_vertices",static_cast<std::int64_t>(n));f.put("observed_arcs",static_cast<std::int64_t>(arcs));
  f.put("directed_density",n>1?static_cast<double>(arcs)/static_cast<double>(n*(n-1)):0.0);
  f.put("max_out_degree",static_cast<std::int64_t>(*std::max_element(degree.begin(),degree.end())));
  f.put("mean_out_degree",static_cast<double>(arcs)/static_cast<double>(n));
  f.put("zero_weight_fraction",arcs?static_cast<double>(zero)/static_cast<double>(arcs):0.0);
  f.put("self_loops",static_cast<std::int64_t>(loops));f.put("parallel_arcs",static_cast<std::int64_t>(parallel));
 }else if(c.family=="calibration"){
  f.put("x",c.x);f.put("threshold",c.threshold);
 }else throw Error("unknown morphology family");
 return f;
}
}
