#include "phaseshift/algorithms.hpp"
#include "phaseshift/experiment.hpp"
#include "phaseshift/morphology.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <utility>

static void check(bool condition, const std::string& message) {
 if(!condition)throw ps::Error("MORPHOLOGY_V2: "+message);
}
template<typename Function> static void rejects(Function fn, const std::string& message){
 bool threw=false;try{fn();}catch(const ps::Error&){threw=true;}
 check(threw,message);
}
static void check_array(const ps::Case& c){
 auto original=ps::make_case(c);
 check(original.id==ps::make_case(c).id,"array exact deterministic case ID");
 check(original.bytes==ps::decode_case(c.family,ps::case_params(original),c.seed,original.bytes).bytes,"array canonical replay");
 auto values=ps::decode_values(original.bytes);
 check(values.size()==static_cast<std::size_t>(c.n),"array length");
 for(auto v:values)check(v>=-c.value_range&&v<=c.value_range,"array value range");
 const auto expected=ps::oracle(original).values;
 for(auto name:ps::algorithms_for(c.family)){
  if(std::string(name)=="insertion_sort"&&c.n>4096)continue;
  if(std::string(name)=="quicksort_first_pivot"&&c.n>20000)continue;
  check(ps::execute(original,name).values==expected,"array oracle: "+name);
 }
 auto features=ps::morphology_features(original);
 check(features.s("generator_version")=="generator-v2","array generator metadata");
 if(c.distribution=="all_equal"&&c.n>0)check(features.i("unique_count")==1,"all-equal observed shape");
 if(c.distribution=="ascending")check(std::is_sorted(values.begin(),values.end()),"ascending shape");
 if(c.distribution=="descending")check(std::is_sorted(values.rbegin(),values.rend()),"descending shape");
}
static void check_graph(ps::Case c){
 auto generated=ps::make_case(c);
 auto regenerated=ps::decode_case(c.family,ps::case_params(generated),c.seed,generated.bytes);
 check(generated.id==regenerated.id&&generated.bytes==regenerated.bytes,"graph canonical replay");
 auto values=ps::decode_values(generated.bytes);
 const auto arc_count=static_cast<std::size_t>(values.at(1));
 check(values.size()==3+arc_count*3,"graph serialized arc count");
 check(arc_count==static_cast<std::size_t>(generated.edges)*(c.directed?1ULL:2ULL),"graph directed logical count");
 std::set<std::pair<std::int64_t,std::int64_t>> arcs;
 for(std::size_t i=3;i<values.size();i+=3){
  check(values[i]!=values[i+1],"forbid graph self-loop");
  check(arcs.emplace(values[i],values[i+1]).second,"forbid graph duplicate directed arc");
  check(values[i+2]>=0&&values[i+2]<=c.max_weight,"graph bounded nonnegative weight");
  if(c.weight_class=="zero_one")check(values[i+2]<=1,"graph zero-one domain");
 }
 if(!c.directed)for(auto [u,v]:arcs)check(arcs.count({v,u})==1,"undirected reverse arc");
 auto expected=ps::oracle(generated).values;
 for(auto name:ps::algorithms_for("graph")){
  if(std::string(name)=="zero_one_bfs"&&c.weight_class!="zero_one")continue;
  check(ps::execute(generated,name).values==expected,"graph oracle: "+name);
 }
 auto features=ps::morphology_features(generated);
 check(features.i("observed_arcs")==static_cast<std::int64_t>(arc_count),"graph morphology observation");
 check(features.i("self_loops")==0&&features.i("parallel_arcs")==0,"graph morphology invariants");
 if(c.connectivity=="strong"||(c.connectivity=="weak"&&!c.directed))for(auto d:expected)check(d<std::numeric_limits<std::int64_t>::max()/4,"connected graph vertex reachable from source");
}
int main(){
 try{
  for(auto distribution:{"uniform","ascending","descending","nearly_sorted","organ_pipe","sawtooth","alternating","all_equal","heavy_duplicate","few_unique","zipf_like"}){
   for(auto n:{0,1,2,17,63,128}){
    ps::Case c;c.family="integer_array";c.generator_version=2;c.distribution=distribution;
    c.n=n;c.seed=1449+static_cast<std::uint64_t>(n);c.value_range=97;c.run_length=11;c.unique_count=distribution==std::string("few_unique")?std::min(8,n):0;
    c.duplicate_ppm=distribution==std::string("ascending")||distribution==std::string("descending")?-1:200000;
    check_array(c);
    c.family="topk";for(auto k:{0,1,n/2,n})if(k<=n){c.k=k;check_array(c);}
   }
  }
  for(auto seed:std::array<std::uint64_t,4>{0,1,137,UINT64_MAX})for(auto directed:{true,false})for(auto topology:{"none","weak","strong"}){
   if(!directed&&std::string(topology)=="strong")continue;
   for(auto cls:{"general","zero_one"}){
    ps::Case c;c.family="graph";c.generator_version=2;c.seed=seed;
    c.vertices=15;c.edges=66;c.source=3;c.directed=directed;c.weight_class=cls;
    c.connectivity=topology;c.max_weight=97;c.community_count=3;c.degree_skew_ppm=800000;
    check_graph(c);
   }
  }
  {
   ps::Case c;c.family="graph";c.generator_version=2;c.vertices=18;c.edges=0;c.density_ppm=500000;
   c.source=0;c.directed=false;c.weight_class="zero_one";c.max_weight=1;
   check_graph(c);
  }
  {
   /* Above 200,000 stored arcs, still within 200,000 logical undirected edges. */
   ps::Case c;c.family="graph";c.generator_version=2;c.vertices=451;c.edges=100001;
   c.directed=false;c.max_weight=10;c.source=1;c.weight_class="zero_one";
   auto actual=ps::make_case(c);
   check(ps::decode_values(actual.bytes)[1]==200002,"large undirected arc encoding");
   check(ps::execute(actual,"dijkstra_heap").values==ps::execute(actual,"zero_one_bfs").values,"large graph dijkstra/bfs match");
  }
  {
   ps::Case old;old.generator_version=1;old.family="integer_array";old.n=32;old.seed=7;
   auto c=ps::make_case(old);
   check(ps::case_params(c).find("generator_version")==std::string::npos,"legacy params remain byte-identical");
   check(c.id==ps::decode_case(c.family,ps::case_params(c),c.seed,c.bytes).id,"v1 identity compatibility");
  }
  {
   ps::Case c;c.family="graph";c.vertices=5;c.edges=2;c.connectivity="strong";
   rejects([&]{ps::make_case(c);},"reject impossible requested strong connectivity");
   c.connectivity="none";c.density_ppm=999999;c.edges=0;c.vertices=4096;
   rejects([&]{ps::make_case(c,1024);},"graph input budget preallocation");
   c=ps::Case{};c.family="integer_array";c.n=12;c.unique_count=100;
   rejects([&]{ps::make_case(c);},"forbid unique_count outside few_unique distribution");
   c=ps::Case{};c.family="graph";c.vertices=5;c.edges=10;c.directed=false;c.connectivity="strong";
   rejects([&]{ps::make_case(c);},"reject undirected strong connectivity setting");
  }
  {
   const auto conf=ps::config_from_json(ps::Json::parse(R"JSON({"schema_version":1,"problem_family":"integer_array","algorithms":["std_sort","mergesort"],"fixed":{"distribution":"nearly_sorted","disorder_fraction":0.05,"value_range":200},"axes":{"n":{"values":[16,32,64,128]}},"limits":{"max_search_queue":4},"measurement":{"paired_discovery_blocks":1,"paired_confirm_blocks":2}})JSON"));
   check(conf.max_queue==4&&conf.base.disorder_ppm==50000&&conf.base.generator_version==2,"fixed-point config parsing and exact queue max");
   rejects([]{ps::config_from_json(ps::Json::parse(R"JSON({"problem_family":"integer_array","fixed":{"disorder_fraction":1.000001}})JSON"));},"reject fraction > 1");
   rejects([]{ps::config_from_json(ps::Json::parse(R"JSON({"problem_family":"integer_array","fixed":{"disorder_fraction":-0.1}})JSON"));},"reject negative fraction");
   rejects([]{ps::config_from_json(ps::Json::parse(R"JSON({"problem_family":"graph","fixed":{"connectivity":"strong","directed":false}})JSON"));},"reject impossible undirected topology");
  }
  std::cout<<"PASS morphology-v2 deterministic generators / independent oracles / replay / configuration validation\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
