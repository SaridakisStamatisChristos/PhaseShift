#include "phaseshift/v3/conditional_pair.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef __linux__
#include <sys/resource.h>
#endif

#define CHECK(x) do {if(!(x))throw std::runtime_error("V3-04 region-tree falsifier: " #x);}while(false)
using namespace ps;
using namespace ps::v3;
template<class F> void reject(F&& f){
 bool raised=false;try{f();}catch(const ps::Error&){raised=true;}
 CHECK(raised);
}
static std::vector<std::string> numbers(std::size_t n){
 std::vector<std::string> result;
 for(std::size_t i=0;i<n;++i)result.push_back(std::to_string(i));
 return result;
}
static void two_cliffs_and_categorical_xor(){
 const std::vector<LazySearchSpace::Axis> axes{
   {"group",{"rare","common"}},{"x",numbers(32)}};
 const std::vector<AdmissibleSearchSpace::Subspace> strata{
   {{"group",{"rare"}}},{{"group",{"common"}}}};
 const AdmissibleSearchSpace legal(axes,strata);
 CHECK(legal.cardinality()==64);
 ConditionalPairPortfolio a(legal,axes,"x",strata,117,62,1.2,8,true);
 ConditionalPairPortfolio b(legal,axes,"x",strata,117,62,1.2,8,true);
 std::set<std::string> encountered;
 std::size_t global_or_new=0,cliffs=0;
 for(std::size_t i=0;i<62;++i){
  const auto p=a.next(),q=b.next();CHECK(p&&q&&*p==*q);
  CHECK(p->arm=="explore");
  CHECK(legal.index_of(p->left)&&legal.index_of(p->right));
  CHECK(p->left.at(0)==p->right.at(0));
  CHECK(std::stoi(p->right.at(1).second)==std::stoi(p->left.at(1).second)+1);
  CHECK(encountered.insert(p->left.at(0).second+":"+p->left.at(1).second).second);
  if(i==0)CHECK(p->left.at(0).second=="rare");
  if(i==1)CHECK(p->left.at(0).second=="common");
  if(i%4==0)++global_or_new;
  reject([&]{(void)a.next();});
  reject([&]{a.on_committed(*p,Cohort::Confirmation,0.2);});
  reject([&]{a.on_committed(*p,Cohort::Discovery,
                          std::numeric_limits<double>::quiet_NaN());});
  const int x=std::stoi(p->left.at(1).second);
  const bool rare=p->left.at(0).second=="rare";
  double contrast=0.;
  if(x==7){contrast=rare?0.65:-0.65;++cliffs;}
  if(x==23){contrast=rare?-0.75:0.75;++cliffs;}
  a.on_committed(*p,Cohort::Discovery,contrast);
  b.on_committed(*q,Cohort::Discovery,contrast);
  CHECK(a.evidence_digest()==b.evidence_digest());
  CHECK(a.region_nodes()<=64); // <= live legal intervals, never Cartesian
 }
 CHECK(encountered.size()==62 && cliffs==4);
 CHECK(global_or_new>=15);
 CHECK(!a.next()&&!b.next());
 CHECK(a.region_nodes()==0);
 CHECK(a.tracked_contexts()==2);
 reject([&]{a.on_committed(ConditionalPairChoice{},Cohort::Discovery,0.2);});
}
static void sparse_virtual_and_malformed(){
 const std::vector<LazySearchSpace::Axis> axes{
   {"a",numbers(4000)},{"b",numbers(4000)},{"x",numbers(128)}};
 const AdmissibleSearchSpace legal(axes);
 CHECK(legal.cardinality()==2048000000ULL);
#ifdef __linux__
 struct rusage before{},after{};
 CHECK(getrusage(RUSAGE_SELF,&before)==0);
#endif
 ConditionalPairPortfolio p(legal,axes,"x",{},110,40,1.2,2,true);
 for(int i=0;i<40;++i){
  const auto c=p.next();CHECK(c);
  CHECK(legal.index_of(c->left)&&legal.index_of(c->right));
  CHECK(c->left[0]==c->right[0]&&c->left[1]==c->right[1]);
  CHECK(c->arm=="explore");
  p.on_committed(*c,Cohort::Discovery,std::nullopt);
 }
 CHECK(p.tracked_contexts()==2);
 CHECK(p.region_nodes()<=42);
 CHECK(p.resident_global_entries()<=2);
#ifdef __linux__
 CHECK(getrusage(RUSAGE_SELF,&after)==0);
 CHECK(after.ru_maxrss-before.ru_maxrss<65536); // extra peak <64 MiB
#endif
 reject([&]{ConditionalPairPortfolio wrong(legal,axes,"absent",{},1,2,1.2,2,true);});
 reject([&]{ConditionalPairPortfolio wrong(legal,axes,"x",{},1,0,1.2,2,true);});
 reject([&]{ConditionalPairPortfolio wrong(legal,axes,"x",{},1,2,1.0,2,true);});
}
int main(){
 two_cliffs_and_categorical_xor();
 sparse_virtual_and_malformed();
 std::cout<<"PASS bounded region splits, exhaustive legal paired cliffs/XOR, holdout, replay, 2.048B sparse universe\n";
}
