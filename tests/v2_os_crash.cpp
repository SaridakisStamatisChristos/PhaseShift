#include "phaseshift/store.hpp"
#include <filesystem>
#include <iostream>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define CHECK(x) do{if(!(x))throw std::runtime_error("os-kill recovery: " #x);}while(false)
static std::int64_t count(sqlite3* db,const char* sql){
 sqlite3_stmt* statement=nullptr;
 CHECK(sqlite3_prepare_v2(db,sql,-1,&statement,nullptr)==SQLITE_OK);
 CHECK(sqlite3_step(statement)==SQLITE_ROW);
 const auto result=sqlite3_column_int64(statement,0);
 sqlite3_finalize(statement);
 return result;
}
int main(int argc,char** argv){try{
 CHECK(argc==2);
 const auto dir=std::filesystem::path(argv[1]);
 std::filesystem::remove_all(dir);
 std::filesystem::create_directories(dir);
 const auto db=(dir/"run.sqlite").string();
 {
  ps::Store baseline(db);
  baseline.upgrade_v2_schema();
  baseline.experiment(R"({"policy_version":"v2"})",ps::environment());
 }
 ps::Case case_info;case_info.family="calibration";case_info.x=6;case_info.seed=123;
 const auto c=ps::make_case(case_info);
 ps::Observation observation;observation.algorithm="calibration";observation.status="ok";
 observation.ns=1200;observation.cpu_ns=500;observation.batch=1;
 observation.work=16;observation.block=0;
 for(int phase=0;phase<3;++phase){
  const auto child=fork();
  CHECK(child>=0);
  if(child==0){
   // A fresh connection after fork simulates a real process dying before
   // COMMIT at three separate durable-evidence write boundaries.
   try{
    ps::Store interrupted(db);
    interrupted.exec("BEGIN IMMEDIATE");
    interrupted.add_observation(c,observation,false);
    if(phase>=1)interrupted.record_decision(0,c.id,123456);
    if(phase>=2)interrupted.record_v2_budget_entry(c.id,500,1000000,true);
    kill(getpid(),SIGKILL);
   }catch(...){}
   _exit(67);
  }
  int status=0;
  CHECK(waitpid(child,&status,0)==child);
  CHECK(WIFSIGNALED(status)&&WTERMSIG(status)==SIGKILL);
  ps::Store recovered(db);
  CHECK(count(recovered.raw(),"SELECT COUNT(*) FROM cases")==0);
  CHECK(count(recovered.raw(),"SELECT COUNT(*) FROM samples")==0);
  CHECK(count(recovered.raw(),"SELECT COUNT(*) FROM scheduler_decisions")==0);
  CHECK(count(recovered.raw(),"SELECT COUNT(*) FROM budget_ledger")==0);
 }
 {
  ps::Store complete(db);
  complete.exec("BEGIN IMMEDIATE");
  try{
   complete.add_observation(c,observation,false);
   complete.record_decision(0,c.id,123456);
   complete.record_v2_budget_entry(c.id,500,1000000,true);
   complete.exec("COMMIT");
  }catch(...){complete.exec("ROLLBACK");throw;}
  complete.verify_v2_budget_entry(0,c.id,500,1000000);
 }
 {
  ps::Store resumed(db);
  CHECK(count(resumed.raw(),"SELECT COUNT(*) FROM cases")==1);
  CHECK(count(resumed.raw(),"SELECT COUNT(*) FROM samples")==1);
  CHECK(count(resumed.raw(),"SELECT COUNT(*) FROM scheduler_decisions")==1);
  CHECK(count(resumed.raw(),"SELECT COUNT(*) FROM budget_ledger")==1);
  resumed.verify_v2_budget_entry(0,c.id,500,1000000);
 }
 std::cout<<"PASS v2 OS-SIGKILL rollback at observation/decision/budget boundaries and exact retry\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
