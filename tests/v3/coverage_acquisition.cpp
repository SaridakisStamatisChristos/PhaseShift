#include "phaseshift/v3/conditional_pair.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#define CHECK(condition) do{if(!(condition))throw std::runtime_error("v18 coverage test: " #condition);}while(false)
using namespace ps;
using namespace ps::v3;

static std::vector<LazySearchSpace::Axis> axes_with_edges(std::size_t edges){
 std::vector<std::string> values;
 for(std::size_t i=0;i<=edges;++i)values.push_back(std::to_string(i));
 return {{"region",{"a"}},{"x",std::move(values)}};
}

static void abundant_unseen_edges_reinvest_replication_budget(){
 // 31 admissible edges > 12-query budget: the v15 forced-replication
 // schedule must be observable as a real reduction in total coverage.
 auto axes=axes_with_edges(31);
 AdmissibleSearchSpace legal(axes);
 ConditionalPairPortfolio incumbent(legal,axes,"x",{},123,12,1.2,64,true,true);
 ConditionalPairPortfolio candidate(legal,axes,"x",{},123,12,1.2,64,true,true,true);
 ConditionalPairPortfolio replay(legal,axes,"x",{},123,12,1.2,64,true,true,true);
 CHECK(candidate.evidence_digest()==replay.evidence_digest());
 CHECK(incumbent.evidence_digest()!=candidate.evidence_digest());
 std::size_t exploration_incumbent=0,exploration_candidate=0;
 for(std::size_t i=0;i<12;++i){
  const auto old=incumbent.next(),p=candidate.next(),q=replay.next();
  CHECK(old&&p&&q&&*p==*q);
  CHECK(legal.index_of(p->left)&&legal.index_of(p->right));
  CHECK(p->left[0].second==p->right[0].second);
  CHECK(!p->acquisition_audit.empty());
  exploration_incumbent+=(old->arm=="explore");
  exploration_candidate+=(p->arm=="explore");
  if(i==3||i==8){
   CHECK(old->arm=="replicate"); // existing v15 forced modulo-5 replication
   CHECK(p->arm=="explore"); // v18 uses legally unseen interval edges
  }
  const double effect=i%3==0?0.38:0.07;
  const PairMeasuredCost cost{15000+static_cast<std::int64_t>(i)*127,2000};
  incumbent.on_committed(*old,Cohort::Discovery,effect,cost);
  candidate.on_committed(*p,Cohort::Discovery,effect,cost);
  replay.on_committed(*q,Cohort::Discovery,effect,cost);
  CHECK(candidate.evidence_digest()==replay.evidence_digest());
 }
 CHECK(exploration_incumbent==10);
 CHECK(exploration_candidate==12);
 CHECK(exploration_candidate>exploration_incumbent);
 CHECK(!incumbent.next()&&!candidate.next()&&!replay.next());
}

static void exhausted_frontier_must_not_invent_exploration(){
 // Only 9 unique edges: after exhausting them, a valid scheduler MUST
 // replicate. Equal final exploration here is CORRECT, not a regression.
 auto axes=axes_with_edges(9);
 AdmissibleSearchSpace legal(axes);
 ConditionalPairPortfolio incumbent(legal,axes,"x",{},123,12,1.2,64,true,true);
 ConditionalPairPortfolio candidate(legal,axes,"x",{},123,12,1.2,64,true,true,true);
 std::size_t incumbent_explore=0,candidate_explore=0;
 for(std::size_t i=0;i<12;++i){
  const auto old=incumbent.next(),p=candidate.next();
  CHECK(old&&p);
  incumbent_explore+=(old->arm=="explore");
  candidate_explore+=(p->arm=="explore");
  const PairMeasuredCost cost{20000+static_cast<std::int64_t>(i)*73,2100};
  incumbent.on_committed(*old,Cohort::Discovery,0.35,cost);
  candidate.on_committed(*p,Cohort::Discovery,0.35,cost);
 }
 CHECK(incumbent_explore==9);
 CHECK(candidate_explore==9);
 CHECK(!incumbent.next()&&!candidate.next());
}

int main(){
 abundant_unseen_edges_reinvest_replication_budget();
 exhausted_frontier_must_not_invent_exploration();
 auto axes=axes_with_edges(31);
 AdmissibleSearchSpace legal(axes);
 bool rejected=false;
 try{ConditionalPairPortfolio invalid(legal,axes,"x",{},123,12,1.2,64,false,true,true);}
 catch(const Error&){rejected=true;}
 CHECK(rejected);
 std::cout<<"PASS v18 available-edge coverage gain, exhausted-edge fallback, separate replay digest and unchanged v15\n";
}
