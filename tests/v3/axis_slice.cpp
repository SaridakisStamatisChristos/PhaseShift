#include "phaseshift/v3/axis_slice.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef __linux__
#include <sys/resource.h>
#endif
#define CHECK(expr) do {if(!(expr))throw std::runtime_error("v3 axis-slice gate: " #expr);}while(false)
using namespace ps;
using namespace ps::v3;

static std::vector<std::string> names(std::size_t n){
 std::vector<std::string> result;result.reserve(n);
 for(std::size_t i=0;i<n;++i)result.push_back(std::to_string(i));
 return result;
}
template<typename F> static void must_throw(F&& f){
 bool threw=false;
 try{f();}catch(const ps::Error&){threw=true;}
 CHECK(threw);
}
static void legal_finite_domain_exhaustive(){
 std::vector<LazySearchSpace::Axis> axes{
  {"group",{"rare","common"}},{"x",names(7)}};
 std::vector<AdmissibleSearchSpace::Subspace> legal_rectangles{
  {{"group",{"rare"}},{"x",{"0","1","2"}}},
  {{"group",{"common"}},{"x",{"3","4","5","6"}}}
 };
 AdmissibleSearchSpace space(axes,legal_rectangles);
 CHECK(space.cardinality()==7);
 std::set<std::uint64_t> expected;
 for(const auto& group:axes[0].second)
  for(const auto& x:axes[1].second)
   if(auto ordinal=space.index_of({{"group",group},{"x",x}}))
    CHECK(expected.insert(*ordinal).second);
 CHECK(expected.size()==7);
 AxisSlicePolicyV3 first(space,axes,"x",1923),second(space,axes,"x",1923);
 std::set<std::uint64_t> observed;
 std::size_t global=0;
 for(std::size_t i=0;i<7;++i) {
  const auto p=first.next(),q=second.next();
  CHECK(p.has_value()&&q.has_value());
  CHECK(p->virtual_index==q->virtual_index);
  CHECK(p->origin==q->origin && p->ranking_score==q->ranking_score);
  CHECK(space.index_of(space.at(p->virtual_index)).has_value());
  CHECK(observed.insert(p->virtual_index).second);
  if(p->origin=="legal_global_exploration")++global;
  const auto effect=std::optional<double>(i%3==0?0.0:1.5);
  must_throw([&]{first.on_committed(p->virtual_index,Cohort::Confirmation,"ok",effect,1.);});
  first.on_committed(p->virtual_index,Cohort::Discovery,"ok",effect,1.);
  second.on_committed(q->virtual_index,Cohort::Discovery,"ok",effect,1.);
  CHECK(first.global_rng_state()==second.global_rng_state());
 }
 CHECK(global>=2); // explicit global exploration, even with local contrast
 CHECK(observed==expected); // independent exact feasible-space oracle
 CHECK(!first.next() && !second.next());
 CHECK(first.pending_count()==0);
 CHECK(first.tracked_contexts()==2);
 must_throw([&]{first.on_committed(0,Cohort::Discovery,"ok",0.0,1.);});
}
static void censored_and_finite_fail_closed(){
 std::vector<LazySearchSpace::Axis> axes{{"x",names(16)}};
 AdmissibleSearchSpace space(axes);
 AxisSlicePolicyV3 search(space,axes,"x",3,8,16);
 const auto p=search.next();
 CHECK(p.has_value());
 must_throw([&]{search.on_committed(p->virtual_index,Cohort::Discovery,"ok",
        std::numeric_limits<double>::quiet_NaN(),1.);});
 CHECK(search.pending_count()==1);
 must_throw([&]{search.on_committed(p->virtual_index,Cohort::Discovery,"ok",1.,-1.);});
 CHECK(search.pending_count()==1);
 must_throw([&]{search.on_committed(p->virtual_index,Cohort::Discovery,
       "wrong_answer",0.5,1.);});
 CHECK(search.pending_count()==1);
 search.on_committed(p->virtual_index,Cohort::Discovery,"no_sample",std::nullopt,1.);
 CHECK(search.pending_count()==0);
 for(int i=1;i<16;++i){
  auto next=search.next();CHECK(next.has_value());
  search.on_committed(next->virtual_index,Cohort::Discovery,"no_sample",
                     std::nullopt,10.);
 }
 CHECK(search.issued_count()==16);
 CHECK(!search.next());
 must_throw([&]{AxisSlicePolicyV3 bad(space,axes,"missing",1);});
 must_throw([&]{AxisSlicePolicyV3 bad(space,axes,"x",1,0);});
}
static void billion_virtual_without_materialization(){
 std::vector<LazySearchSpace::Axis> axes{
  {"a",names(4000)},{"b",names(4000)},{"x",names(128)}};
 AdmissibleSearchSpace space(axes);
 CHECK(space.cardinality()==2048000000ULL);
#ifdef __linux__
 struct rusage before{},after{};
 CHECK(getrusage(RUSAGE_SELF,&before)==0);
#endif
 AxisSlicePolicyV3 search(space,axes,"x",777,16,80);
 for(int i=0;i<48;++i){
  auto next=search.next();
  CHECK(next.has_value());
  CHECK(next->virtual_index<space.cardinality());
  const auto coords=space.at(next->virtual_index);
  CHECK(space.index_of(coords).value()==next->virtual_index);
  search.on_committed(next->virtual_index,Cohort::Discovery,"ok",
                      std::optional<double>{static_cast<double>(i%11)},1.);
 }
 CHECK(search.issued_count()==48);
 CHECK(search.tracked_contexts()<=16);
 CHECK(search.pending_count()==0);
#ifdef __linux__
 CHECK(getrusage(RUSAGE_SELF,&after)==0);
 CHECK(after.ru_maxrss-before.ru_maxrss<65536); // less than 64 MB extra RSS
#endif
}
int main(){
 legal_finite_domain_exhaustive();
 censored_and_finite_fail_closed();
 billion_virtual_without_materialization();
 std::cout<<"PASS v3 ordered conditional slicing, exact feasible-index oracle, "
             "independent replay, holdout gate, billion-index memory bound\n";
}
