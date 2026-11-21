#include "phaseshift/v3/legacy_policy_adapter.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#define CHECK(expr) do {if(!(expr))throw std::runtime_error("v3 selector characterization: " #expr);}while(false)

using namespace ps;
using ps::v3::LegacyV2PolicyAdapter;

static Config basic(const std::string& method){
 Config cfg;
 cfg.method=method;cfg.policy_version="v2";cfg.seed=113;
 cfg.axis="x";cfg.axes={{"x",{"0","1","2","3","4","5","6","7"}}};
 cfg.exploration_fraction=0.25;
 return cfg;
}
static void uniform_and_invalid(){
 auto cfg=basic("uniform_v2");
 AdmissibleSearchSpace space(std::vector<LazySearchSpace::Axis>(
  cfg.axes.begin(),cfg.axes.end()));
 LegacyV2PolicyAdapter adapter(cfg,space,1,true);
 SparsePermutation oracle(space.cardinality(),cfg.seed^0xb7d2f937c4abULL);
 for(std::size_t i=0;i<space.cardinality();++i) {
  // The old v2 scheduler rejects certain invalid virtual indices.
  const auto allow=[](std::uint64_t idx){return idx%3!=0;};
  std::optional<std::uint64_t> expected;
  while(oracle.has_next()){
   auto candidate=oracle.next();if(allow(candidate)){expected=candidate;break;}
  }
  auto actual=adapter.next_index(allow);
  CHECK(actual==expected);
  CHECK(adapter.rng_state()==oracle.state());
  if(!actual)break;
 }
 CHECK(!adapter.next_index([](auto){return true;}));
}
static void stratified_oracle(){
 auto cfg=basic("stratified_v2");
 cfg.axes["mode"]={"alpha","beta"};
 cfg.axis="x";
 std::vector<LazySearchSpace::Axis> axes(cfg.axes.begin(),cfg.axes.end());
 AdmissibleSearchSpace space(axes);
 LegacyV2PolicyAdapter adapter(cfg,space,2,true);
 StratifiedIndexSamplerV2 oracle(axes,{},"x",2,cfg.seed^0xb4ad19f14acULL);
 for(std::uint64_t i=0;i<space.cardinality()*2;++i) {
  const auto expected=oracle.next();
  const auto actual=adapter.next_index([](auto){return true;});
  CHECK(actual.has_value()&&*actual==expected);
  CHECK(adapter.rng_state()==oracle.state());
  CHECK(adapter.acquisition_origin().value()=="origin=stratified_primary_axis");
 }
 CHECK(!adapter.next_index([](auto){return true;}));
}
static void hierarchical_oracle(bool cliff){
 auto cfg=basic(cliff?"cliff_bracket_v2":"hierarchical_boundary_v2");
 std::vector<LazySearchSpace::Axis> axes(cfg.axes.begin(),cfg.axes.end());
 AdmissibleSearchSpace space(axes);
 LegacyV2PolicyAdapter adapter(cfg,space,1,true);
 HierarchicalPolicyV2 oracle(static_cast<std::size_t>(space.cardinality()),
  cfg.seed^0xd9af2e13ULL,cfg.exploration_fraction,cliff);
 for(std::size_t i=0;i<space.cardinality();++i) {
  const auto expected=oracle.next();
  const auto actual=adapter.next_index([](auto){return false;});
  CHECK(expected.has_value()&&actual==expected);
  CHECK(adapter.rng_state()==oracle.rng_state());
  CHECK(adapter.acquisition_origin().value()==("origin="+oracle.origin()));
  const auto metric=(i%3==0)?std::optional<double>{}:
   std::optional<double>(static_cast<double>(*expected%3));
  oracle.observe(*expected,metric,1.+static_cast<double>(i));
  adapter.observe(*actual,metric,1.+static_cast<double>(i));
 }
 CHECK(!adapter.next_index([](auto){return true;}));
}
static void hybrid_oracle(){
 auto cfg=basic("hybrid_mixed_v2");
 cfg.axes["mode"]={"alpha","beta"};
 cfg.replicate_count=2;
 std::vector<LazySearchSpace::Axis> axes(cfg.axes.begin(),cfg.axes.end());
 AdmissibleSearchSpace space(axes);
 LegacyV2PolicyAdapter adapter(cfg,space,2,true);
 HybridMixedPolicyV2 oracle(axes,{},cfg.axis,2,cfg.seed^0x827d391bULL,
                           cfg.exploration_fraction,true);
 for(std::uint64_t i=0;i<space.cardinality()*2;++i) {
  auto expected=oracle.next();
  auto actual=adapter.next_index([](auto){return true;});
  CHECK(actual==expected);
  CHECK(adapter.rng_state()==oracle.rng_state());
  CHECK(adapter.acquisition_origin().value()=="origin="+oracle.origin());
  if(!actual)break;
  const auto metric=std::optional<double>(static_cast<double>(*expected%5));
  oracle.observe(*expected,metric,2.);
  adapter.observe(*actual,metric,2.);
 }
 CHECK(!adapter.next_index([](auto){return true;}));
}
int main(){
 uniform_and_invalid();
 stratified_oracle();
 hierarchical_oracle(false);
 hierarchical_oracle(true);
 hybrid_oracle();
 std::cout<<"PASS v3 v2-selector characterization across four policy arms, "
 "invalid indices, RNG, origins and feedback\n";
}
