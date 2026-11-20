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
/* Literal v3 cases preserve the exact serialized bytes after structural
 * ddmin. They have a separately versioned generator identity, so no legacy
 * v1/v2 PRNG case can ever replay as a different reduced input. */
Case literal_array(const Case& parent,const std::vector<std::int64_t>& values){
 Case out=parent;out.generator_version=3;out.distribution="literal";
 out.n=static_cast<std::int64_t>(values.size());out.k=std::min(out.k,out.n);
 out.unique_count=0;out.duplicate_ppm=-1;
 out.literal_hex=hex_encode(encode_values(values));return out;
}
Case literal_graph(const Case& parent,const std::vector<std::int64_t>& data){
 Case out=parent;out.generator_version=3;out.directed=true;
 out.connectivity="none";out.community_count=1;out.density_ppm=-1;
 out.vertices=data[0];out.edges=data[1];out.source=data[2];
 out.literal_hex=hex_encode(encode_values(data));return out;
}
void structural_proposals(const Case& initial,std::vector<std::pair<Case,std::string>>& out){
 if(initial.bytes.size()>64ULL*1024 || initial.bytes.size()%8)return;
 const auto data=decode_values(initial.bytes);
 if(initial.family=="integer_array"||initial.family=="topk"){
  const auto n=data.size();if(n<=1)return;
  std::vector<std::size_t> chunks={n/2,n/4,1};
  chunks.erase(std::remove(chunks.begin(),chunks.end(),0),chunks.end());
  chunks.erase(std::unique(chunks.begin(),chunks.end()),chunks.end());
  std::size_t emitted=0;
  for(const auto chunk:chunks){
   for(std::size_t offset=0;offset<n&&emitted<80;offset+=chunk){
    const auto end=std::min(n,offset+chunk);
    std::vector<std::int64_t> result;result.reserve(n-(end-offset));
    result.insert(result.end(),data.begin(),data.begin()+static_cast<std::ptrdiff_t>(offset));
    result.insert(result.end(),data.begin()+static_cast<std::ptrdiff_t>(end),data.end());
    add_candidate(out,literal_array(initial,result),"ddmin_delete_array_span");emitted++;
   }
  }
  return;
 }
 if(initial.family!="graph"||!initial.directed||data.size()<3||data[1]<0||
    data.size()!=3+static_cast<std::size_t>(data[1])*3)return;
 const auto count=static_cast<std::size_t>(data[1]);
 std::vector<std::size_t> chunks={count/2,count/4,1};
 chunks.erase(std::remove(chunks.begin(),chunks.end(),0),chunks.end());
 chunks.erase(std::unique(chunks.begin(),chunks.end()),chunks.end());
 std::size_t emitted=0;
 for(const auto chunk:chunks)for(std::size_t offset=0;offset<count&&emitted<80;offset+=chunk){
  const auto end=std::min(count,offset+chunk);
  std::vector<std::int64_t> remaining{data[0],static_cast<std::int64_t>(count-(end-offset)),data[2]};
  remaining.reserve(3+3*(count-(end-offset)));
  for(std::size_t e=0;e<count;e++)if(e<offset||e>=end){
   remaining.push_back(data[3+3*e]);remaining.push_back(data[4+3*e]);remaining.push_back(data[5+3*e]);
  }
  add_candidate(out,literal_graph(initial,remaining),"ddmin_delete_graph_arcs");emitted++;
 }
 /* Removing any non-source vertex renumbers every surviving arc. This
    preserves the chosen source and serializes an independently replayable
    directed graph; no dangling vertex IDs are permitted. */
 if(data[0]>1){
  for(std::int64_t drop=0;drop<data[0]&&emitted<90;drop++){
   if(drop==data[2])continue;
   std::vector<std::int64_t> remaining{data[0]-1,0,data[2]-(data[2]>drop?1:0)};
   for(std::size_t e=0;e<count;e++){
    auto from=data[3+3*e],to=data[4+3*e];
    if(from==drop||to==drop)continue;
    remaining.push_back(from-(from>drop?1:0));
    remaining.push_back(to-(to>drop?1:0));
    remaining.push_back(data[5+3*e]);
    remaining[1]++;
   }
   add_candidate(out,literal_graph(initial,remaining),"ddmin_remove_and_renumber_graph_vertex");
   emitted++;
  }
 }

}
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
 structural_proposals(initial,out);
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
