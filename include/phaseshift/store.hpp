#pragma once
#include "phaseshift/runner.hpp"
#include <sqlite3.h>
namespace ps {
class Store {
 sqlite3* db_=nullptr;
public:
 explicit Store(const std::string& path,bool readonly=false); ~Store();
 Store(const Store&)=delete;Store& operator=(const Store&)=delete;
 void exec(const std::string& sql);
 // Explicit additive v7->v8 migration for v2 only; legacy v1 stays at v7.
 void upgrade_v2_schema();
 void record_v2_budget_entry(const std::string& case_id,std::uint64_t cumulative_sampled_cpu_ns,
   std::uint64_t max_sampled_cpu_ns,bool accounting_complete);
 void experiment(const std::string& config,const std::string& env);
 void add_case(const Case& c);
 void add_observation(const Case& c,const Observation& s,bool holdout);
 void seal_confirmation(const std::string& id,const std::string& algorithm,int required_seed_pairs,bool scaling=false,
  const std::vector<std::uint64_t>& prescribed_seed_groups={});
 void hypothesis(const std::string& id,const std::string& status,const std::string& axis,std::int64_t left,std::int64_t right,double effect,const std::string& detail,const std::string& kind="UNSTABLE_OR_INCONCLUSIVE",const std::string& frozen_params="");
 struct SchedulerDecision {std::int64_t pool_index=0;std::string case_id;std::uint64_t next_rng_state=0;};
 std::vector<SchedulerDecision> scheduler_decisions();
 void record_decision(std::size_t index,const std::string& case_id,std::uint64_t next_rng_state);
 void event(const std::string& name,const std::string& detail);
 void refresh_aggregates();
 void artifact(const std::string& relative_path,const std::string& digest,const std::string& kind);
 void witness(const std::string& hypothesis_id,const std::string& case_id,const std::string& status,const std::string& trace);
 std::vector<Case> cases(bool include_holdout=true);
 std::vector<Observation> observations(const std::string& case_id,bool holdout=false);
 Json summary();
 std::string config();
 std::string environment();
 sqlite3* raw(){return db_;}
};
}
