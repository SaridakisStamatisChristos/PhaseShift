#include "phaseshift/v3/types.hpp"
#include "phaseshift/v3/budget.hpp"
#include "phaseshift/v3/discovery_view.hpp"
#include "phaseshift/v3/replicate_planner.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <vector>
#include <iostream>

using namespace ps;
using namespace ps::v3;

template<typename F> static void must_throw(F&& f) {
 bool threw=false;
 try {f();} catch(const ps::Error&) {threw=true;}
 assert(threw);
}
static StructuralKey key(std::string x,std::string mode="a") {
 return {{{"x",std::move(x)},{"mode",std::move(mode)}},2};
}
static ObservationSummary obs(StructuralKey s,std::uint64_t seed) {
 return {{std::move(s),{seed,0}},"ok",0.5,0.04,100,7,900,"raw-test-seal"};
}
int main() {
 const AdmissibleSearchSpace legal({
   {"x",{"0","1","2","3"}},{"mode",{"a","b"}}});
 assert(legal.cardinality()==8);
 for(std::uint64_t i=0;i<8;++i)
  assert(legal.index_of(legal.at(i)).value()==i); // independent old v2 legal mapping
 auto a=key("0"),b=key("3","b");
 a.validate();b.validate();
 must_throw([]{key("0","").validate();});
 must_throw([]{StructuralKey{{{"x","0"},{"x","1"}},2}.validate();});
 must_throw([]{StructuralKey{{{"x","0"}},0}.validate();});
 ReplicatePlanner first(9181,4),repeat(9181,4);
 auto p=first.propose(legal,a,b);
 auto q=repeat.propose(legal,a,b);
 assert(p.first.key==q.first.key && p.second.key==q.second.key);
 assert(p.first_runs_first==q.first_runs_first);
 assert(p.first.key.replicate.seed==p.second.key.replicate.seed);
 assert(p.first.key.structure!=p.second.key.structure);
 assert(p.first.proposal_number+1==p.second.proposal_number);
 auto p2=first.propose(legal,a,b);
 assert(p2.first.key.replicate.seed!=p.first.key.replicate.seed);
 must_throw([&]{first.propose(legal,a,a);});
 must_throw([&]{first.propose(legal,key("99"),b);});
 ReplicatePlanner forbids(9181,4,{p.first.key.replicate.seed});
 assert(forbids.propose(legal,a,b).first.key.replicate.seed!=p.first.key.replicate.seed);
 first.propose(legal,a,b);first.propose(legal,a,b);
 must_throw([&]{first.propose(legal,a,b);});

 DiscoveryEvidenceSnapshot view(10),restored(10);
 auto row=obs(a,8);
 must_throw([&]{view.append_committed(row,Cohort::Confirmation,1);});
 assert(view.size()==0 && view.source_watermark()==0);
 view.append_committed(row,Cohort::Discovery,1);
 restored.append_committed(row,Cohort::Discovery,1);
 assert(view.canonical_digest()==restored.canonical_digest());
 assert(view.find(row.key)!=nullptr);
 must_throw([&]{view.append_committed(row,Cohort::Discovery,2);});
 must_throw([&]{view.append_committed(obs(a,9),Cohort::Discovery,1);});
 auto another=obs(a,9);
 view.append_committed(another,Cohort::Discovery,3);
 restored.append_committed(another,Cohort::Discovery,3);
 assert(view.size()==2 && view.canonical_digest()==restored.canonical_digest());
 auto invalid=obs(b,10);
 invalid.status="wrong_answer";
 must_throw([&]{view.append_committed(invalid,Cohort::Discovery,4);});
 invalid.paired_log_ratio=std::nullopt;
 invalid.robust_dispersion=std::nullopt;
 view.append_committed(invalid,Cohort::Discovery,4);
 assert(view.find(invalid.key)!=nullptr && !view.find(invalid.key)->paired_log_ratio);
 auto nan=obs(b,11);nan.paired_log_ratio=std::numeric_limits<double>::quiet_NaN();
 must_throw([&]{view.append_committed(nan,Cohort::Discovery,5);});
 assert(view.source_watermark()==4);

 BudgetSnapshot ceilings{};
 ceilings.child_cpu_ns=100;ceilings.parent_cpu_ns=50;
 ceilings.host_wall_ns=1000;ceilings.attempted_cases=2;
 BudgetLedger ledger(ceilings);
 assert(ledger.admit(40,10,300));
 auto overshoot=ledger.commit({1,40,10,300,true});
 assert(overshoot.child_cpu_ns==0 && ledger.snapshot().evaluated_cases==1);
 assert(ledger.admit(50,10,600));
 auto overshoot2=ledger.commit({2,90,60,900,false});
 assert(overshoot2.child_cpu_ns==30 && overshoot2.parent_cpu_ns==20 &&
        overshoot2.host_wall_ns==200);
 assert(ledger.snapshot().invalid_cases==1 && ledger.snapshot().attempted_cases==2);
 assert(!ledger.admit(1,1,1));
 const auto snap=ledger.snapshot();
 must_throw([&]{ledger.commit({2,1,0,1,true});});
 assert(ledger.snapshot().child_cpu_ns==snap.child_cpu_ns);
 must_throw([&]{ledger.commit({3,std::numeric_limits<std::uint64_t>::max(),0,1,true});});
 assert(ledger.snapshot().attempted_cases==snap.attempted_cases);
 std::cout<<"PASS v3 legal keys, fresh matched seeds, discovery-only snapshot, "
             "canonical replay and atomic RAM-budget accounting\n";
}
