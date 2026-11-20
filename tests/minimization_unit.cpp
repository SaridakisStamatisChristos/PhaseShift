#include "phaseshift/minimize.hpp"
#include "phaseshift/algorithms.hpp"
#include <iostream>
#include <algorithm>
#include <string>
static void check(bool pass,const std::string& description){if(!pass)throw ps::Error("MINIMIZATION TEST: "+description);}
int main(){try{
 {
  ps::Case c;c.family="integer_array";c.generator_version=2;c.n=512;c.seed=7;c.distribution="ascending";
  c.value_range=100000;
  auto input=ps::make_case(c);
  auto preserve=[](const ps::Case& candidate){auto values=ps::decode_values(candidate.bytes);
   std::reverse(values.begin(),values.end());return values!=ps::oracle(candidate).values;};
  auto outcome=ps::shrink_case(input,preserve,50);
  check(outcome.reduced,"array witness must shrink");
  check(outcome.smallest.n==2,"ascending reverse-sorted failure minimal n=2");
  check(outcome.smallest.bytes.size()<outcome.original.bytes.size(),"array bytes strictly smaller");
  check(outcome.evaluations<=50,"array bounded evaluation");
  check(ps::decode_case(outcome.smallest.family,ps::case_params(outcome.smallest),outcome.smallest.seed,outcome.smallest.bytes).id==outcome.smallest.id,"array replay reconstruction");
  check(preserve(outcome.smallest),"array retained semantic failure");
 }
 {
  ps::Case c;c.family="graph";c.generator_version=2;c.vertices=30;c.edges=50;c.seed=137;
  c.weight_class="zero_one";c.source=0;c.directed=false;c.connectivity="weak";
  auto input=ps::make_case(c);
  auto preserve=[](const ps::Case& x){return x.vertices>=3&&x.edges>=2&&ps::oracle(x).values.size()==static_cast<std::size_t>(x.vertices);};
  auto result=ps::shrink_case(input,preserve,50);
  check(result.reduced,"graph witness must shrink");
  check(result.smallest.bytes.size()<result.original.bytes.size(),"graph byte shrink");
  check(preserve(result.smallest),"graph structural predicate preserved");
  check(ps::decode_case(result.smallest.family,ps::case_params(result.smallest),result.smallest.seed,result.smallest.bytes).id==result.smallest.id,"graph exact replay");
  check(ps::execute(result.smallest,"dijkstra_heap").values==ps::execute(result.smallest,"zero_one_bfs").values,"shrunk graph oracle");
 }
 {
  ps::Case c;c.family="topk";c.n=64;c.k=20;c.distribution="all_equal";c.value_range=42;
  auto input=ps::make_case(c);
  bool threw=false;try{ps::shrink_case(input,[](const ps::Case&){return false;});}catch(const ps::Error&){threw=true;}
  check(threw,"reject unproven original predicate");
  threw=false;try{ps::shrink_case(input,[](const ps::Case&){return true;},0);}catch(const ps::Error&){threw=true;}
  check(threw,"reject zero budget");
 }
 std::cout<<"PASS schema-aware bounded minimization, witness predicate and exact replay\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
