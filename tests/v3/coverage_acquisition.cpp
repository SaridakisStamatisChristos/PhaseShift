#include "phaseshift/v3/conditional_pair.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#define CHECK(condition) do{if(!(condition))throw std::runtime_error("v18 coverage test: " #condition);}while(false)
using namespace ps;
using namespace ps::v3;
int main(){
 const std::vector<LazySearchSpace::Axis> axes{
   {"region",{"a"}},{"x",{"0","1","2","3","4","5","6","7","8","9"}}};
 AdmissibleSearchSpace legal(axes);
 ConditionalPairPortfolio incumbent(legal,axes,"x",{},123,12,1.2,64,true,true);
 ConditionalPairPortfolio candidate(legal,axes,"x",{},123,12,1.2,64,true,true,true);
 ConditionalPairPortfolio replay(legal,axes,"x",{},123,12,1.2,64,true,true,true);
 CHECK(candidate.evidence_digest()==replay.evidence_digest());
 CHECK(incumbent.evidence_digest()!=candidate.evidence_digest());
 std::size_t exploration_incumbent=0, exploration_candidate=0;
 for(std::size_t i=0;i<12;++i){
   auto old=incumbent.next(),p=candidate.next(),q=replay.next();
   CHECK(old && p && q && *p==*q);
   CHECK(legal.index_of(p->left) && legal.index_of(p->right));
   CHECK(p->left[0].second==p->right[0].second);
   CHECK(!p->acquisition_audit.empty());
   exploration_incumbent+=(old->arm=="explore");
   exploration_candidate+=(p->arm=="explore");
   if(i==3){
     CHECK(old->arm=="replicate"); // incumbent's forced modulo-5 replication
     CHECK(p->arm=="explore"); // v18 reinvests query into frontier
   }
   // Do not pass any planted-truth label to the candidate scheduler.
   const double effect=i%3==0?0.38:0.07;
   const PairMeasuredCost cost{15000+static_cast<std::int64_t>(i)*127,2000};
   incumbent.on_committed(*old,Cohort::Discovery,effect,cost);
   candidate.on_committed(*p,Cohort::Discovery,effect,cost);
   replay.on_committed(*q,Cohort::Discovery,effect,cost);
   CHECK(candidate.evidence_digest()==replay.evidence_digest());
 }
 CHECK(exploration_candidate>exploration_incumbent);
 CHECK(!incumbent.next()&&!candidate.next()&&!replay.next());
 bool bad=false;
 try{ConditionalPairPortfolio invalid(legal,axes,"x",{},123,12,1.2,64,false,true,true);}
 catch(const Error&){bad=true;}
 CHECK(bad);
 std::cout<<"PASS v18 distinct policy digest, extra exploration, replay, and v15 stability\n";
}
