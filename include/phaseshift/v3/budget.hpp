#pragma once
// Bounded, per-event RAM ledger. Durable transactions remain Store's concern.
#include "phaseshift/v3/types.hpp"
#include <limits>
#include <set>

namespace ps::v3 {
struct CostEvent {
 std::uint64_t event_id=0,child_cpu_ns=0,parent_cpu_ns=0,host_wall_ns=0;
 bool valid_sample=false;
};
struct CostOvershoot {std::uint64_t child_cpu_ns=0,parent_cpu_ns=0,host_wall_ns=0;};
class BudgetLedger {
 BudgetSnapshot observed_{};
 BudgetSnapshot ceilings_{};
 std::set<std::uint64_t> committed_;
 static std::uint64_t checked_sum(std::uint64_t a,std::uint64_t b) {
  if(b>std::numeric_limits<std::uint64_t>::max()-a)
   throw Error("v3 budget counter overflow");
  return a+b;
 }
public:
 explicit BudgetLedger(BudgetSnapshot ceilings):ceilings_(ceilings){
  if(!ceilings_.host_wall_ns || !ceilings_.child_cpu_ns ||
     !ceilings_.attempted_cases)
   throw Error("v3 missing finite required budget ceiling");
 }
 const BudgetSnapshot& snapshot()const noexcept{return observed_;}
 bool admit(std::uint64_t expected_child,std::uint64_t expected_parent,
            std::uint64_t expected_wall) const noexcept {
  if(!expected_wall || !expected_child ||
     observed_.attempted_cases>=ceilings_.attempted_cases ||
     observed_.child_cpu_ns>=ceilings_.child_cpu_ns ||
     observed_.host_wall_ns>=ceilings_.host_wall_ns)return false;
  if(expected_child>ceilings_.child_cpu_ns-observed_.child_cpu_ns ||
     expected_wall>ceilings_.host_wall_ns-observed_.host_wall_ns)return false;
  // Parent ceiling is optional; zero means unbounded but tracked.
  if(ceilings_.parent_cpu_ns &&
     (observed_.parent_cpu_ns>=ceilings_.parent_cpu_ns ||
      expected_parent>ceilings_.parent_cpu_ns-observed_.parent_cpu_ns))
   return false;
  return true;
 }
 CostOvershoot commit(const CostEvent& event) {
  if(committed_.count(event.event_id) ||
     committed_.size()>=1000000 || !event.host_wall_ns)
   throw Error("v3 duplicate or invalid committed cost event");
  // Stage and validate every field before modifying visible state.
  auto next=observed_;
  next.child_cpu_ns=checked_sum(next.child_cpu_ns,event.child_cpu_ns);
  next.parent_cpu_ns=checked_sum(next.parent_cpu_ns,event.parent_cpu_ns);
  next.host_wall_ns=checked_sum(next.host_wall_ns,event.host_wall_ns);
  next.attempted_cases=checked_sum(next.attempted_cases,1);
  if(event.valid_sample)next.evaluated_cases=checked_sum(next.evaluated_cases,1);
  else next.invalid_cases=checked_sum(next.invalid_cases,1);
  committed_.insert(event.event_id);
  observed_=next;
  return {next.child_cpu_ns>ceilings_.child_cpu_ns?
          next.child_cpu_ns-ceilings_.child_cpu_ns:0,
          ceilings_.parent_cpu_ns && next.parent_cpu_ns>ceilings_.parent_cpu_ns?
          next.parent_cpu_ns-ceilings_.parent_cpu_ns:0,
          next.host_wall_ns>ceilings_.host_wall_ns?
          next.host_wall_ns-ceilings_.host_wall_ns:0};
 }
};
} // namespace ps::v3
