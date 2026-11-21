#include "phaseshift/v3/conditional_pair.hpp"
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
#define CHECK(expr) do {if(!(expr))throw std::runtime_error("v3 conditional gate: " #expr);}while(false)
using namespace ps;
using namespace ps::v3;
template<class F>void rejects(F&& f){
 bool ok=false;try{f();}catch(const ps::Error&){ok=true;}
 CHECK(ok);
}
static std::vector<std::string> names(std::size_t n){
 std::vector<std::string> out;
 for(std::size_t i=0;i<n;++i)out.push_back(std::to_string(i));
 return out;
}
static void rare_admissible_strata_and_repeat(){
 std::vector<LazySearchSpace::Axis> axes{
  {"group",{"rare","common"}},{"x",names(6)}};
 std::vector<AdmissibleSearchSpace::Subspace> subspaces{
  {{"group",{"rare"}},{"x",{"0","1","2"}}},
  {{"group",{"common"}},{"x",{"2","3","4","5"}}}};
 AdmissibleSearchSpace legal(axes,subspaces);
 CHECK(legal.cardinality()==7);
 ConditionalPairPortfolio a(legal,axes,"x",subspaces,101,12,1.2),
                          b(legal,axes,"x",subspaces,101,12,1.2);
 std::set<std::string> observed;
 std::size_t replicates=0,censored=0;
 for(std::size_t i=0;i<12;++i){
  const auto p=a.next(),q=b.next();
  CHECK(p && q && *p==*q);
  CHECK(p->left.size()==2&&p->right.size()==2);
  CHECK(p->left[0]==p->right[0]);
  const auto first=legal.index_of(p->left),second=legal.index_of(p->right);
  CHECK(first&&second&&*first!=*second);
  if(i==0)CHECK(p->left[0].second=="rare");
  if(i==1)CHECK(p->left[0].second=="common");
  if(p->arm=="explore")observed.insert(p->left[0].second+":"+p->left[1].second);
  else {CHECK(p->arm=="replicate");++replicates;}
  rejects([&]{(void)a.next();}); // cannot run policy ahead of committed evidence
  rejects([&]{a.on_committed(*p,Cohort::Confirmation,0.2);});
  CHECK(a.committed()==i);
  const auto metric=(i==3)?std::optional<double>{}:
    std::optional<double>{0.15+static_cast<double>(i%4)*0.02};
  if(!metric)++censored;
  a.on_committed(*p,Cohort::Discovery,metric);
  b.on_committed(*q,Cohort::Discovery,metric);
  CHECK(a.evidence_digest()==b.evidence_digest());
 }
 CHECK(observed.size()>=2);
 CHECK(replicates>0&&censored==1);
 CHECK(a.tracked_contexts()>=2&&a.tracked_contexts()<=3);
 CHECK(a.committed()==12);
 CHECK(!a.next());
}
static void numerical_and_feedback_falsification(){
 std::vector<LazySearchSpace::Axis> axes{{"tag",{"a","b"}},{"x",{"0","2","3"}}};
 AdmissibleSearchSpace legal(axes);
 rejects([&]{ConditionalPairPortfolio p(legal,axes,"tag",{},1,3,1.2);});
 rejects([&]{ConditionalPairPortfolio p(legal,axes,"x",{},1,0,1.2);});
 rejects([&]{ConditionalPairPortfolio p(legal,axes,"x",{},1,3,1.);});
 rejects([&]{ConditionalPairPortfolio p(legal,axes,"x",{},1,3,
  std::numeric_limits<double>::infinity());});
 ConditionalPairPortfolio p(legal,axes,"x",{},23,4,1.2);
 auto a=p.next();CHECK(a);
 rejects([&]{p.on_committed(*a,Cohort::Discovery,
                          std::numeric_limits<double>::quiet_NaN());});
 auto forged=*a;forged.arm="replicate";
 rejects([&]{p.on_committed(forged,Cohort::Discovery,0.01);});
 p.on_committed(*a,Cohort::Discovery,std::nullopt);
 auto b=p.next();CHECK(b);
 p.on_committed(*b,Cohort::Discovery,0.13);
 const auto c=p.next();CHECK(c);
 CHECK(c->arm=="replicate");
 p.on_committed(*c,Cohort::Discovery,0.13);
 const auto d=p.next();CHECK(d);
 p.on_committed(*d,Cohort::Discovery,std::nullopt);
 CHECK(!p.next());
 std::vector<LazySearchSpace::Axis> wrong{{"tag",{"a","b"}},{"x",{"2","1","3"}}};
 AdmissibleSearchSpace wrong_space(wrong);
 rejects([&]{ConditionalPairPortfolio t(wrong_space,wrong,"x",{},1,3,1.2);});
}
static void categorical_interaction_independence(){
 std::vector<LazySearchSpace::Axis> axes{
  {"a",{"0","1"}},{"b",{"0","1"}},{"x",{"0","1","2","3"}}};
 std::vector<AdmissibleSearchSpace::Subspace> blocks;
 for(const auto& a:axes[0].second)for(const auto& b:axes[1].second)
  blocks.push_back({{"a",{a}},{"b",{b}}});
 AdmissibleSearchSpace legal(axes,blocks);
 ConditionalPairPortfolio policy(legal,axes,"x",blocks,33,12,1.2);
 std::set<std::string> contexts;
 for(std::size_t i=0;i<12;++i){
  const auto p=policy.next();CHECK(p);
  CHECK(p->left[0]==p->right[0]&&p->left[1]==p->right[1]);
  CHECK(legal.index_of(p->left)&&legal.index_of(p->right));
  if(p->arm=="explore")contexts.insert(p->left[0].second+p->left[1].second);
  // True XOR interaction: contrast reverses in different categorical cells.
  const bool xor_label=p->left[0].second!=p->left[1].second;
  policy.on_committed(*p,Cohort::Discovery,xor_label?0.5:-0.5);
 }
 CHECK(contexts.size()==4); // every pre-registered rare category screened
}
static void billion_virtual_rss(){
 std::vector<LazySearchSpace::Axis> axes{
  {"a",names(4000)},{"b",names(4000)},{"x",names(128)}};
 AdmissibleSearchSpace legal(axes);
 CHECK(legal.cardinality()==2048000000ULL);
#ifdef __linux__
 struct rusage before{},after{};
 CHECK(getrusage(RUSAGE_SELF,&before)==0);
#endif
 ConditionalPairPortfolio policy(legal,axes,"x",{},971,30,1.2,12);
 for(int i=0;i<30;++i){
  const auto p=policy.next();CHECK(p);
  CHECK(legal.index_of(p->left)&&legal.index_of(p->right));
  CHECK(p->left[0]==p->right[0]&&p->left[1]==p->right[1]);
  policy.on_committed(*p,Cohort::Discovery,static_cast<double>(i%5)/10.);
 }
 CHECK(policy.tracked_contexts()<=12);
 CHECK(policy.resident_global_entries()<500);
#ifdef __linux__
 CHECK(getrusage(RUSAGE_SELF,&after)==0);
 CHECK(after.ru_maxrss-before.ru_maxrss<65536); // <64 MiB additional RSS
#endif
}
int main(){
 rare_admissible_strata_and_repeat();
 numerical_and_feedback_falsification();
 categorical_interaction_independence();
 billion_virtual_rss();
 std::cout<<"PASS conditional paired portfolio: rare strata, legal XOR, rejected holdout, deterministic resume, 2.048B sparse universe\n";
}
