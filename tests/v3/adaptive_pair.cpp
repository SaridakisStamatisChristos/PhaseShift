#include "phaseshift/v3/adaptive_pair.hpp"
#include "phaseshift/v3/replicate_planner.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(x) do {if(!(x))throw std::runtime_error("v3 adaptive gate: " #x);}while(false)
using namespace ps;
using namespace ps::v3;

template<class F>static void rejected(F&& fn){
 bool caught=false;try{fn();}catch(const ps::Error&){caught=true;}
 CHECK(caught);
}
static void replay_and_exploration(){
 AdaptivePairPolicy a({"0","1","2","3","4","5"},91,12,1.2);
 AdaptivePairPolicy b({"0","1","2","3","4","5"},91,12,1.2);
 std::set<std::size_t> explored;
 std::size_t replicate_count=0;
 for(std::size_t i=0;i<12;++i){
  const auto x=a.next(),y=b.next();
  CHECK(x&&y&&*x==*y);
  CHECK(x->left_label!=x->right_label);
  CHECK(x->prior_evidence_digest==y->prior_evidence_digest);
  if(x->arm=="explore")explored.insert(x->edge);
  else {CHECK(x->arm=="replicate");++replicate_count;}
  if(i<2)CHECK(x->arm=="explore");
  if(i==2)CHECK(x->arm=="replicate");
  if(i==4)CHECK(x->arm=="explore");
  rejected([&]{(void)a.next();}); // cannot plan ahead of committed evidence
  const auto contrast=0.17+static_cast<double>(i%3)*0.015;
  a.on_committed(*x,Cohort::Discovery,contrast);
  b.on_committed(*y,Cohort::Discovery,contrast);
  CHECK(a.evidence_digest()==b.evidence_digest());
 }
 CHECK(explored.size()==4); // first two plus every fourth among twelve
 CHECK(replicate_count>=5);
 CHECK(a.committed_pairs()==12);
 CHECK(!a.next().has_value());
}
static void rejection_and_null_evidence(){
 rejected([]{AdaptivePairPolicy p({"x","y"},1,4,1.2);});
 rejected([]{AdaptivePairPolicy p({"0","0","2"},1,4,1.2);});
 rejected([]{AdaptivePairPolicy p({"0","1","2"},1,0,1.2);});
 rejected([]{AdaptivePairPolicy p({"0","1","2"},1,4,1.0);});
 rejected([]{AdaptivePairPolicy p({"0","1","2"},1,4,
           std::numeric_limits<double>::infinity());});
 AdaptivePairPolicy p({"0","1","2"},12,8,1.2);
 const auto first=p.next();
 CHECK(first.has_value());
 rejected([&]{p.on_committed(*first,Cohort::Confirmation,0.1);});
 CHECK(p.committed_pairs()==0);
 rejected([&]{p.on_committed(*first,Cohort::Discovery,
          std::numeric_limits<double>::quiet_NaN());});
 AdaptivePairChoice altered=*first;altered.arm="replicate";
 rejected([&]{p.on_committed(altered,Cohort::Discovery,0.1);});
 p.on_committed(*first,Cohort::Discovery,std::nullopt); // censored
 CHECK(p.summary(first->edge).valid_seeds==0);
 CHECK(p.summary(first->edge).censored_seeds==1);
 const auto second=p.next();
 CHECK(second && second->arm=="explore"); // no finite evidence yet
 p.on_committed(*second,Cohort::Discovery,0.185);
 const auto third=p.next();
 CHECK(third && third->arm=="replicate");
 CHECK(third->edge==second->edge); // failures are not numeric speed
 p.on_committed(*third,Cohort::Discovery,0.195);
 for(std::size_t i=0;i<5;++i){
  const auto next=p.next();CHECK(next);
  p.on_committed(*next,Cohort::Discovery,0.185+static_cast<double>(i)*0.005);
 }
 CHECK(p.committed_pairs()==8);
 rejected([&]{p.summary(2);});
 rejected([&]{p.on_committed(*third,Cohort::Discovery,0.2);});
}
static void independent_paired_seed_confounding_control(){
 // This synthetic oracle deliberately gives *identical* seed noise to both
 // structures. Unpaired seeds hallucinate differences; paired seeds cancel.
 std::size_t spurious=0;
 for(int seed=0;seed<40;++seed){
  auto noise=[](int s){return (s&1)?0.7:-0.7;};
  const double matched=(2.0+noise(seed)+0.10)-(2.0+noise(seed));
  const double unmatched=(2.0+noise(seed+1)+0.10)-(2.0+noise(seed));
  CHECK(std::abs(matched-0.10)<1e-12);
  if(std::abs(unmatched)>0.4)++spurious;
 }
 CHECK(spurious==40);
}
int main(){
 replay_and_exploration();
 rejection_and_null_evidence();
 independent_paired_seed_confounding_control();
 std::cout<<"PASS deterministic v3 adaptive allocation, censored feedback and seed confounding oracle\n";
}
