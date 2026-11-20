#include "phaseshift/algorithms.hpp"
#include "phaseshift/core.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
static void verify(bool ok,const std::string& reason){if(!ok)throw ps::Error("adversarial algorithm failure: "+reason);}
static std::vector<std::int64_t> floyd_oracle(const ps::Case& c){
 const auto encoded=ps::decode_values(c.bytes);
 const auto n=static_cast<std::size_t>(encoded.at(0)), e=static_cast<std::size_t>(encoded.at(1));
 const auto source=static_cast<std::size_t>(encoded.at(2));
 verify(n<=12 && encoded.size()==3+e*3,"tiny graph encoding");
 constexpr auto INF=std::numeric_limits<std::int64_t>::max()/4;
 std::vector<std::int64_t> d(n*n,INF);
 for(std::size_t i=0;i<n;i++)d[i*n+i]=0;
 for(std::size_t i=0;i<e;i++){
  const auto a=static_cast<std::size_t>(encoded.at(3+3*i));
  const auto b=static_cast<std::size_t>(encoded.at(4+3*i));
  const auto w=encoded.at(5+3*i);
  verify(a<n&&b<n&&w>=0&&w<INF,"valid encoded graph arcs");
  d[a*n+b]=std::min(d[a*n+b],w);
 }
 for(std::size_t k=0;k<n;k++)for(std::size_t i=0;i<n;i++)for(std::size_t j=0;j<n;j++){
  const auto ik=d[i*n+k],kj=d[k*n+j];
  if(ik<INF&&kj<INF-ik)d[i*n+j]=std::min(d[i*n+j],ik+kj);
 }
 std::vector<std::int64_t> result(n);
 for(std::size_t j=0;j<n;j++)result[j]=d[source*n+j];
 return result;
}
int main(){
 try{
  const std::array<std::string,11> distributions={"uniform","ascending","descending","nearly_sorted","organ_pipe","sawtooth","alternating","all_equal","heavy_duplicate","few_unique","zipf_like"};
  std::uint64_t seed=0x19880231a0ULL;
  std::uint64_t sorted_cases=0,topk_cases=0,graph_cases=0;
  for(int trial=0;trial<2200;trial++){
   ps::Case c;c.family="integer_array";c.generator_version=2;
   c.n=static_cast<std::int64_t>(ps::randbelow(seed,257));
   c.distribution=distributions[static_cast<std::size_t>(trial)%distributions.size()];
   c.value_range=1+static_cast<std::int64_t>(ps::randbelow(seed,1000000));
   c.sign_ppm=static_cast<std::int64_t>(ps::randbelow(seed,1000001));
   c.run_length=1+static_cast<std::int64_t>(ps::randbelow(seed,32));
   c.seed=ps::splitmix64(seed);c=ps::make_case(c);
   const auto oracle=ps::oracle(c).values;
   for(const auto& name:ps::algorithms_for("integer_array"))
    verify(ps::execute(c,name).values==oracle,"sort trial="+std::to_string(trial)+" algorithm="+name);
   sorted_cases++;
   c.family="topk";
   for(const auto k:std::array<std::int64_t,3>{0,c.n/2,c.n}){
    c.k=k;c=ps::make_case(c);
    const auto expected=ps::oracle(c).values;
    for(const auto& name:ps::algorithms_for("topk"))
     verify(ps::execute(c,name).values==expected,"TopK trial="+std::to_string(trial)+" algorithm="+name);
    topk_cases++;
   }
  }
  for(const auto& values:{
   std::vector<std::int64_t>{},
   std::vector<std::int64_t>{0},
   std::vector<std::int64_t>{std::numeric_limits<std::int64_t>::min(),std::numeric_limits<std::int64_t>::max(),0,-1,1},
   std::vector<std::int64_t>{std::numeric_limits<std::int64_t>::min(),std::numeric_limits<std::int64_t>::min(),std::numeric_limits<std::int64_t>::max(),std::numeric_limits<std::int64_t>::max()}
  }){
   ps::Case c;c.family="integer_array";c.n=static_cast<std::int64_t>(values.size());c.bytes=ps::encode_values(values);
   const auto oracle=ps::oracle(c).values;
   for(const auto& name:ps::algorithms_for("integer_array"))verify(ps::execute(c,name).values==oracle,"int64 extremes");
   c.family="topk";
   for(std::int64_t k=0;k<=c.n;k++){c.k=k;for(const auto& name:ps::algorithms_for("topk"))verify(ps::execute(c,name).values==ps::oracle(c).values,"TopK extremes");}
  }
  for(int trial=0;trial<850;trial++){
   ps::Case c;c.family="graph";c.generator_version=2;
   c.vertices=1+static_cast<std::int64_t>(ps::randbelow(seed,12));
   c.directed=(trial%2==0);const auto cap=c.directed?c.vertices*(c.vertices-1):c.vertices*(c.vertices-1)/2;
   c.edges=static_cast<std::int64_t>(ps::randbelow(seed,static_cast<std::uint64_t>(cap+1)));
   c.source=static_cast<std::int64_t>(ps::randbelow(seed,static_cast<std::uint64_t>(c.vertices)));
   c.community_count=1+static_cast<std::int64_t>(ps::randbelow(seed,static_cast<std::uint64_t>(c.vertices)));
   c.degree_skew_ppm=static_cast<std::int64_t>(ps::randbelow(seed,1000001));
   c.weight_class=(trial%3==0)?"zero_one":"general";
   c.seed=ps::splitmix64(seed);
   c=ps::make_case(c);
   const auto expected=floyd_oracle(c);
   verify(ps::oracle(c).values==expected,"Bellman-Ford differs from independent Floyd-Warshall");
   for(const auto& name:ps::algorithms_for("graph")){
    if(name=="zero_one_bfs"&&c.weight_class!="zero_one")continue;
    verify(ps::execute(c,name).values==expected,"graph algorithm="+name+" trial="+std::to_string(trial));
   }
   graph_cases++;
  }
  {
   ps::Case c;c.family="graph";c.vertices=3;c.edges=3;c.weight_class="general";c=ps::make_case(c);
   bool rejected=false;try{(void)ps::execute(c,"zero_one_bfs");}catch(const ps::Error&){rejected=true;}
   verify(rejected,"0-1 BFS must strictly reject non-zero-one graph class");
  }
  std::cout<<"PASS: "<<sorted_cases<<" sorting, "<<topk_cases<<" Top-K, "<<graph_cases<<" independently checked graph fixtures\n";
 }catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
 return 0;
}
