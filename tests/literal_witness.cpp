#include "phaseshift/core.hpp"
#include "phaseshift/minimize.hpp"
#include "phaseshift/algorithms.hpp"
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {
void require(bool value,const std::string& detail){if(!value)throw ps::Error("LITERAL WITNESS TEST: "+detail);}
void replay(const ps::Case& c){
 const auto reconstructed=ps::decode_case(c.family,ps::case_params(c),c.seed,c.bytes);
 require(reconstructed.id==c.id&&reconstructed.bytes==c.bytes,"exact byte-identity replay");
 require(ps::make_case(c).id==c.id,"stable versioned identity");
}
void invalid(const ps::Case& c,const std::string& desc){
 bool rejected=false;try{(void)ps::make_case(c);}catch(const ps::Error&){rejected=true;}
 require(rejected,desc);
}
}
int main(){try{
 // Keep two extremes present. A parameter-generated ascending family cannot
 // remove arbitrary interior positions, whereas literal ddmin can.
 {
  ps::Case init;init.family="integer_array";init.generator_version=2;
  init.distribution="ascending";init.n=16;init.seed=141;init.value_range=16;
  const auto original=ps::make_case(init);
  auto pred=[](const ps::Case& c){
   const auto v=ps::decode_values(c.bytes);
   return std::find(v.begin(),v.end(),-8)!=v.end() && std::find(v.begin(),v.end(),7)!=v.end();
  };
  const auto shrunk=ps::shrink_case(original,pred,400);
  require(shrunk.reduced,"array witness reduced");
  require(shrunk.smallest.generator_version==3,"array structural ddmin used literal v3");
  require(shrunk.smallest.n==2,"array reduced to the irreducible two values");
  require(shrunk.smallest.bytes.size()==16,"exact array witness size");
  require(pred(shrunk.smallest),"array predicate retained");
  replay(shrunk.smallest);
 }
 {
  ps::Case init;init.family="topk";init.generator_version=2;
  init.distribution="ascending";init.n=24;init.k=2;init.seed=142;init.value_range=30;
  const auto original=ps::make_case(init);
  const auto expected=ps::oracle(original).values;
  auto pred=[&](const ps::Case& c){return c.k==2&&ps::oracle(c).values==expected;};
  const auto shrunk=ps::shrink_case(original,pred,500);
  require(shrunk.reduced,"TopK witness reduced");
  require(shrunk.smallest.generator_version==3,"TopK structural ddmin used literal v3");
  require(shrunk.smallest.n==2,"TopK witness retains exactly the top two elements");
  require(ps::execute(shrunk.smallest,"topk_nth").values==expected,"TopK semantic replay");
  replay(shrunk.smallest);
 }
 {
  ps::Case init;init.family="graph";init.generator_version=2;
  init.vertices=5;init.edges=20;init.source=0;init.seed=26;init.max_weight=8;init.directed=true;
  const auto original=ps::make_case(init);
  const auto expected=ps::oracle(original).values;
  auto pred=[&](const ps::Case& c){return c.vertices==5&&ps::oracle(c).values==expected;};
  const auto shrunk=ps::shrink_case(original,pred,500);
  require(shrunk.reduced,"graph witness reduced");
  require(shrunk.smallest.generator_version==3,"graph structural ddmin used literal v3");
  require(shrunk.smallest.edges<original.edges,"graph contains fewer preserved arcs");
  require(pred(shrunk.smallest),"graph distance contract retained");
  require(ps::execute(shrunk.smallest,"dijkstra_heap").values==expected,"graph algorithm agreement");
  replay(shrunk.smallest);
 }
 {
  // Original graph has a non-source, non-terminal isolated branch. The
  // shortest paths to the other two vertices stay valid when vertex 1 is
  // removed and vertex indices 2,3 become 1,2.
  ps::Case c;c.family="graph";c.generator_version=3;c.directed=true;
  c.vertices=4;c.edges=3;c.source=0;
  c.literal_hex=ps::hex_encode(ps::encode_values({4,3,0,0,1,9,0,2,7,2,3,2}));
  auto original=ps::make_case(c);
  auto pred=[](const ps::Case& g){
   if(g.vertices<3||g.source!=0)return false;
   const auto d=ps::oracle(g).values;
   return std::find(d.begin(),d.end(),7)!=d.end() &&
          std::find(d.begin(),d.end(),9)!=d.end() &&
          std::find(d.begin(),d.end(),0)!=d.end();
  };
  auto shrunk=ps::shrink_case(original,pred,128);
  require(shrunk.reduced,"renumberable graph reduced");
  require(shrunk.smallest.vertices==3,"non-terminal graph vertex removed");
  require(pred(shrunk.smallest),"renumbered graph still preserves distances");
  replay(shrunk.smallest);
 }
 {
  // The undirected graph generator stores reciprocal arcs, but the schema
  // counts logical edges. Shrink logical edges atomically and preserve that
  // invariant even after vertex deletion and exact byte replay.
  ps::Case c;c.family="graph";c.generator_version=2;c.directed=false;
  c.vertices=6;c.edges=10;c.source=0;c.seed=972;c.weight_class="zero_one";
  const auto original=ps::make_case(c);
  const auto original_distances=ps::oracle(original).values;
  auto pred=[&](const ps::Case& x){
   if(x.vertices!=6||x.edges<1||x.directed)return false;
   return ps::oracle(x).values==original_distances;
  };
  const auto reduced=ps::shrink_case(original,pred,500);
  require(reduced.reduced,"undirected logical edge minimization reduced input");
  require(reduced.smallest.generator_version==3,"undirected reduction uses literal graph v3");
  require(!reduced.smallest.directed,"undirected interpretation preserved");
  require(reduced.smallest.edges<original.edges,"undirected edge count reduced");
  require(reduced.smallest.bytes.size()<original.bytes.size(),"undirected witness byte reduction");
  require(ps::execute(reduced.smallest,"zero_one_bfs").values==original_distances,"undirected zero-one oracle retained");
  replay(reduced.smallest);
 }
 {
  ps::Case c;c.family="graph";c.generator_version=3;c.directed=false;
  c.vertices=4;c.edges=3;c.source=0;
  c.literal_hex=ps::hex_encode(ps::encode_values({4,6,0,
   0,1,9,1,0,9,0,2,7,2,0,7,2,3,2,3,2,2}));
  const auto original=ps::make_case(c);
  auto keep=[](const ps::Case& x){
   if(x.directed||x.source!=0||x.vertices<3)return false;
   const auto distances=ps::oracle(x).values;
   return std::find(distances.begin(),distances.end(),7)!=distances.end() &&
          std::find(distances.begin(),distances.end(),9)!=distances.end();
  };
  const auto shrunk=ps::shrink_case(original,keep,250);
  require(shrunk.reduced,"undirected renumber witness reduced");
  require(shrunk.smallest.vertices==3,"undirected nonessential vertex renumbered");
  require(shrunk.smallest.edges==2,"only essential reciprocal pairs remain");
  require(keep(shrunk.smallest),"undirected distance witness preserved");
  replay(shrunk.smallest);
 }
 // A literal case is not an untyped input bypass: validate all lengths,
 // header fields, edge indices, restricted weights and upload caps.
 {
  ps::Case c;c.family="integer_array";c.generator_version=3;c.distribution="literal";c.n=2;
  c.literal_hex=ps::hex_encode(ps::encode_values({-3,7}));
  replay(ps::make_case(c));
  c.n=3;invalid(c,"array count mismatch");c.n=2;
  c.literal_hex="ab";invalid(c,"unaligned payload");
  c.literal_hex="zz";invalid(c,"invalid hexadecimal input");
  c.literal_hex=std::string(131074,'0');invalid(c,"oversized payload");
  c.literal_hex=ps::hex_encode(ps::encode_values({-3,7}));
  c.family="topk";c.k=3;invalid(c,"TopK requires k<=n");
 }
 {
  ps::Case c;c.family="graph";c.generator_version=3;c.directed=true;
  c.vertices=2;c.source=0;c.edges=1;c.weight_class="zero_one";
  c.literal_hex=ps::hex_encode(ps::encode_values({2,1,0,0,1,1}));
  replay(ps::make_case(c));
  c.literal_hex=ps::hex_encode(ps::encode_values({2,1,0,0,1,2}));invalid(c,"zero_one graph weight restriction");
  c.literal_hex=ps::hex_encode(ps::encode_values({2,1,0,0,2,1}));invalid(c,"graph index restriction");
  c.literal_hex=ps::hex_encode(ps::encode_values({2,2,0,0,1,1}));invalid(c,"graph stored arc count restriction");
  c.directed=false;
  c.literal_hex=ps::hex_encode(ps::encode_values({2,2,0,0,1,1,1,0,1}));
  replay(ps::make_case(c));
  c.literal_hex=ps::hex_encode(ps::encode_values({2,2,0,0,1,1,1,0,0}));invalid(c,"undirected reciprocal weight mismatch");
  c.literal_hex=ps::hex_encode(ps::encode_values({2,2,0,0,1,1,0,1,1}));invalid(c,"undirected missing reciprocal direction");
  c.literal_hex=ps::hex_encode(ps::encode_values({2,2,0,0,0,1,0,0,1}));invalid(c,"undirected self loop rejection");
 }
 std::cout<<"PASS schema-aware literal ddmin, semantic preservation, replay and malformed inputs\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
