#include "phaseshift/store.hpp"
#include <sqlite3.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#define VERIFY(x) do{if(!(x))throw std::runtime_error("v2 atomicity: " #x);}while(false)
static std::int64_t count(sqlite3* db,const char* query){
 sqlite3_stmt* stmt=nullptr;
 VERIFY(sqlite3_prepare_v2(db,query,-1,&stmt,nullptr)==SQLITE_OK);
 VERIFY(sqlite3_step(stmt)==SQLITE_ROW);
 const auto result=sqlite3_column_int64(stmt,0);
 sqlite3_finalize(stmt);return result;
}
int main(int argc,char** argv){try{
 VERIFY(argc==2);
 namespace fs=std::filesystem;
 fs::remove_all(argv[1]);fs::create_directories(argv[1]);
 const auto path=(fs::path(argv[1])/"atomic.sqlite").string();
 ps::Case c;c.family="calibration";c.seed=99;c.x=11;c.threshold=8;c=ps::make_case(c);
 ps::Observation o;o.algorithm="calibration";o.status="ok";
 o.ns=50000;o.cpu_ns=42000;o.batch=1;o.work=10010;
 {
  ps::Store s(path);s.upgrade_v2_schema();
  s.experiment(R"({"schema_version":1,"policy_version":"v2"})",ps::environment());
  const auto assert_empty=[&]{
   VERIFY(count(s.raw(),"SELECT COUNT(*) FROM cases")==0);
   VERIFY(count(s.raw(),"SELECT COUNT(*) FROM samples")==0);
   VERIFY(count(s.raw(),"SELECT COUNT(*) FROM scheduler_decisions")==0);
   VERIFY(count(s.raw(),"SELECT COUNT(*) FROM budget_ledger")==0);
   VERIFY(count(s.raw(),"SELECT COUNT(*) FROM algorithms")==0);
  };
  for(int stage=0;stage<3;++stage){
   s.exec("BEGIN IMMEDIATE");
   try{
    s.add_observation(c,o,false);
    if(stage==0)throw std::runtime_error("fault after sample");
    s.record_decision(0,c.id,0xBEEFULL);
    if(stage==1)throw std::runtime_error("fault after schedule");
    s.record_v2_budget_entry(c.id,42000,30000,true);
    throw std::runtime_error("fault after budget ledger");
   }catch(const std::runtime_error&){s.exec("ROLLBACK");}
   assert_empty();
  }
  s.exec("BEGIN IMMEDIATE");
  try{
   s.add_observation(c,o,false);
   s.record_decision(0,c.id,0xBEEFULL);
   s.record_v2_budget_entry(c.id,42000,30000,true);
   s.exec("COMMIT");
  }catch(...){s.exec("ROLLBACK");throw;}
  VERIFY(count(s.raw(),"SELECT COUNT(*) FROM samples")==1);
  VERIFY(count(s.raw(),"SELECT COUNT(*) FROM scheduler_decisions")==1);
  VERIFY(count(s.raw(),"SELECT COUNT(*) FROM budget_ledger")==1);
  s.verify_v2_budget_entry(0,c.id,42000,30000);
  bool invalid=false;
  try{s.verify_v2_budget_entry(0,c.id,42001,30000);}catch(const ps::Error&){invalid=true;}
  VERIFY(invalid);
  // Exact nonreplacement: duplicate decision cannot be committed or counted.
  s.exec("BEGIN IMMEDIATE");
  try{s.record_decision(0,c.id,0xBEEFULL);throw std::runtime_error("duplicate inserted");}
  catch(const std::exception&){s.exec("ROLLBACK");}
  VERIFY(count(s.raw(),"SELECT COUNT(*) FROM scheduler_decisions")==1);
 }
 {
  ps::Store after(path);
  VERIFY(count(after.raw(),"PRAGMA user_version")==8);
  VERIFY(count(after.raw(),"SELECT COUNT(*) FROM cases")==1);
  VERIFY(count(after.raw(),"SELECT COUNT(*) FROM samples")==1);
  VERIFY(count(after.raw(),"SELECT COUNT(*) FROM scheduler_decisions")==1);
  VERIFY(count(after.raw(),"SELECT COUNT(*) FROM budget_ledger")==1);
  after.verify_v2_budget_entry(0,c.id,42000,30000);
 }
 return 0;
 }catch(const std::exception&){return 1;}
}
