#include "phaseshift/minimize.hpp"
#include <algorithm>
#include <set>
#include <tuple>
#include <utility>
namespace ps {
namespace {
std::uint64_t complexity(const Case& c){
 if(c.family=="graph")return static_cast<std::uint64_t>(c.vertices)*2+static_cast<std::uint64_t>(c.edges);
 if(c.family=="integer_array"||c.family=="topk")
  return static_cast<std::uint64_t>(c.value_range)+static_cast<std::uint64_t>(c.run_length)+static_cast<std::uint64_t>(c.unique_count)+
         static_cast<std::uint64_t>(c.k>0?c.k:0);
 return static_cast<std::uint64_t>(c.x>=0?c.x:0);
}
auto ordering(const Case& c){return std::pair{c.bytes.size(),complexity(c)};}
void add_candidate(std::vector<std::pair<Case,std::string>>& out,const Case& c,const std::string& why){out.emplace_back(c,why);}
std::vector<std::pair<Case,std::string>> proposals(const Case& initial){
 std::vector<std::pair<Case,std::string>> out;
 auto edit=[&](const std::string& why,auto fn){auto candidate=initial;fn(candidate);add_candidate(out,candidate,why);};
 if(initial.family=="integer_array"||initial.family=="topk"){
  for(auto n:{initial.n/2,initial.n-1,initial.n/4})if(n>=0&&n<initial.n)
   edit("shrink_array_length",[&](Case& c){c.n=n;c.k=std::min(c.k,n);c.unique_count=std::min(c.unique_count,n);});
  if(initial.family=="topk"&&initial.k>0)for(auto k:{initial.k/2,initial.k-1})if(k>=0&&k<initial.k)
   edit("shrink_topk_output",[&](Case& c){c.k=k;});
  for(auto v:{initial.value_range/2,initial.value_range-1})if(v>=1&&v<initial.value_range)
   edit("shrink_value_range",[&](Case& c){c.value_range=v;c.unique_count=std::min(c.unique_count,2*v+1);});
  if(initial.run_length>1)edit("shrink_run_length",[](Case& c){c.run_length=1;});
  if(initial.unique_count>1)edit("shrink_unique_values",[](Case& c){c.unique_count=1;});
 }else if(initial.family=="graph"){
  for(auto v:{initial.vertices/2,initial.vertices-1})if(v>=1&&v<initial.vertices&&initial.source<v)
   edit("shrink_vertices",[&](Case& c){
    c.vertices=v;c.community_count=std::min(c.community_count,v);
    const auto cap=c.directed?v*(v-1):v*(v-1)/2;
    c.edges=std::min(c.edges,cap);
    if(c.connectivity=="strong")c.edges=std::max(c.edges,v);
    if(c.connectivity=="weak")c.edges=std::max(c.edges,v-1);
    c.density_ppm=-1; /* finite explicit edge count preserves independent replay */
   });
  if(initial.edges>0)for(auto e:{initial.edges/2,initial.edges-1})if(e>=0&&e<initial.edges)
   edit("shrink_edges",[&](Case& c){c.edges=e;c.density_ppm=-1;});
  for(auto w:{initial.max_weight/2,initial.max_weight-1})if(w>=0&&w<initial.max_weight)
   edit("shrink_weight_range",[&](Case& c){c.max_weight=w;});
  if(initial.community_count>1)edit("shrink_communities",[](Case& c){c.community_count=1;});
 }else if(initial.family=="calibration"){
  if(initial.x>0)edit("shrink_calibration_x",[](Case& c){c.x=0;});
 }
 return out;
}
}
ShrinkResult shrink_case(const Case& original,
                         const std::function<bool(const Case&)>& predicate,
                         std::size_t max_evaluations,std::uint64_t max_case_bytes){
 if(!predicate||max_evaluations<1||max_evaluations>10000)throw Error("invalid shrinking predicate/budget");
 ShrinkResult result;
 result.original=original;
 auto regenerated=make_case(original,max_case_bytes);
 if(regenerated.id!=original.id||regenerated.bytes!=original.bytes)throw Error("initial shrinking case not canonical");
 result.smallest=original;
 if(!predicate(original))throw Error("original case does not preserve minimization predicate");
 result.evaluations++;
 std::set<std::string> visited{original.id};
 bool improved=true;
 while(improved&&result.evaluations<max_evaluations){
  improved=false;
  for(const auto& [params,action]:proposals(result.smallest)){
   Case candidate;
   try{candidate=make_case(params,max_case_bytes);}catch(const Error&){continue;}
   if(!visited.insert(candidate.id).second||ordering(candidate)>=ordering(result.smallest))continue;
   if(result.evaluations>=max_evaluations)break;
   auto retained=predicate(candidate);result.evaluations++;
   result.trace.push_back(ShrinkStep{candidate.id,candidate.bytes.size(),action,retained});
   if(retained){result.smallest=std::move(candidate);result.reduced=true;improved=true;break;}
  }
 }
 if(ordering(result.smallest)>ordering(original))throw Error("minimization unexpectedly increased witness size");
 return result;
}
}
