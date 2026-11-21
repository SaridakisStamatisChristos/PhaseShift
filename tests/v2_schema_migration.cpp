#include "phaseshift/store.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <sqlite3.h>
#include <string>
#define CHECK(x) do{if(!(x))throw std::runtime_error("v2 migration: " #x);}while(false)
static std::int64_t scalar(sqlite3* db,const char* query){
 sqlite3_stmt* st=nullptr;
 CHECK(sqlite3_prepare_v2(db,query,-1,&st,nullptr)==SQLITE_OK);
 CHECK(sqlite3_step(st)==SQLITE_ROW);
 const auto value=sqlite3_column_int64(st,0);
 sqlite3_finalize(st);
 return value;
}
int main(int argc,char** argv){try{
 CHECK(argc==2);
 namespace fs=std::filesystem;
 fs::remove_all(argv[1]);fs::create_directories(argv[1]);
 const auto path=(fs::path(argv[1])/"v2.sqlite").string();
 std::string identity;
 {
  ps::Store s(path);
  CHECK(scalar(s.raw(),"PRAGMA user_version")==7);
  s.experiment("{\"schema_version\":1,\"policy_version\":\"v2\"}",ps::environment());
  ps::Case c;c.family="calibration";c.seed=18;c=ps::make_case(c);
  identity=c.id;
  s.add_case(c);
  s.upgrade_v2_schema();
  CHECK(scalar(s.raw(),"PRAGMA user_version")==8);
  CHECK(scalar(s.raw(),"SELECT COUNT(*) FROM cases")==1);
  CHECK(scalar(s.raw(),"SELECT COUNT(*) FROM policy_versions")==1);
  s.upgrade_v2_schema(); // idempotent
  s.exec("BEGIN IMMEDIATE");
  try{
   s.record_decision(0,identity,44);
   s.record_v2_budget_entry(identity,10000,1,true);
   s.exec("COMMIT");
  }catch(...){s.exec("ROLLBACK");throw;}
  CHECK(scalar(s.raw(),"SELECT COUNT(*) FROM budget_ledger")==1);
  CHECK(scalar(s.raw(),"SELECT cumulative_sampled_cpu_ns FROM budget_ledger")==10000);
  bool mismatch=false;
  try{s.record_v2_budget_entry("not-a-case",20000,1,true);}catch(const ps::Error&){mismatch=true;}
  CHECK(mismatch);
  CHECK(scalar(s.raw(),"SELECT COUNT(*) FROM budget_ledger")==1);
  mismatch=false;
  try{s.record_v2_budget_entry(identity,20000,1,true);}catch(const ps::Error&){mismatch=true;}
  CHECK(mismatch); // unique decision evidence
  CHECK(scalar(s.raw(),"SELECT COUNT(*) FROM budget_ledger b JOIN scheduler_decisions d ON d.seq=b.decision_seq WHERE b.case_id=d.case_id")==1);
 }
 {
  ps::Store s(path,true);
  CHECK(scalar(s.raw(),"PRAGMA user_version")==8);
  CHECK(scalar(s.raw(),"SELECT COUNT(*) FROM cases")==1);
  CHECK(scalar(s.raw(),"SELECT COUNT(*) FROM budget_ledger")==1);
 }
 const auto legacy=(fs::path(argv[1])/"v1.sqlite").string();
 {
  ps::Store s(legacy);
  CHECK(scalar(s.raw(),"PRAGMA user_version")==7);
  s.experiment("{\"schema_version\":1}",ps::environment());
 }
 {
  ps::Store s(legacy,true);
  CHECK(scalar(s.raw(),"PRAGMA user_version")==7);
 }
 const auto future=(fs::path(argv[1])/"future.sqlite").string();
 sqlite3* raw=nullptr;CHECK(sqlite3_open(future.c_str(),&raw)==SQLITE_OK);
 CHECK(sqlite3_exec(raw,"PRAGMA user_version=9",nullptr,nullptr,nullptr)==SQLITE_OK);
 sqlite3_close(raw);
 bool rejected=false;try{ps::Store future_schema(future);}catch(const ps::Error&){rejected=true;}
 CHECK(rejected);
 std::cout<<"PASS v1 remains v7; v2 opt-in v8 atomic additive migration, FK-linked ledger and readonly replay\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
