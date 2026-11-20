#include "phaseshift/store.hpp"
#include "phaseshift/morphology.hpp"
#include <sqlite3.h>
#include <filesystem>
#include <iostream>
#include <string>
namespace fs=std::filesystem;
static void verify(bool condition,const char* why){if(!condition)throw ps::Error(std::string("EVIDENCE_SCHEMA: ")+why);}
static std::int64_t count(sqlite3* db,const char* query){
 sqlite3_stmt* stmt=nullptr;
 verify(sqlite3_prepare_v2(db,query,-1,&stmt,nullptr)==SQLITE_OK,"prepare evidence SQL");
 const auto ok=sqlite3_step(stmt)==SQLITE_ROW;
 const auto value=ok?sqlite3_column_int64(stmt,0):-1;
 sqlite3_finalize(stmt);
 verify(ok,"read evidence SQL");return value;
}
int main(int argc,char** argv){try{
 verify(argc==2,"need scratch directory");
 const fs::path root(argv[1]);fs::remove_all(root);fs::create_directories(root);
 const auto db=(root/"run.sqlite").string();
 {
  ps::Store s(db);
  s.experiment("{\"schema_version\":1}",ps::environment());
  ps::Case c;c.family="integer_array";c.n=32;c.seed=7;c.distribution="ascending";c=ps::make_case(c);
  ps::Observation valid;valid.algorithm="std_sort";valid.status="ok";valid.ns=1200000;valid.batch=24;
  valid.work=40;valid.block=2;valid.cpu_ns=1100000;valid.peak_rss_bytes=3200000;
  valid.order_position=0;valid.order_seed="aabbcc";
  s.add_observation(c,valid,false);
  valid.ns=800000;valid.block=3;valid.order_position=3;s.add_observation(c,valid,false);
  ps::Observation failure;failure.algorithm="test_wrong_answer";failure.status="wrong_answer";
  failure.ns=999;failure.work=8;failure.block=3;failure.cpu_ns=999;
  s.add_observation(c,failure,false);
  s.refresh_aggregates();
  s.hypothesis("H1","HYPOTHESIZED","n",16,32,0.6,"frozen model","PERFORMANCE_CLIFF",ps::case_params(c));
  s.hypothesis("H1","TESTED","n",16,32,0.6,"independent test initiated");
  s.hypothesis("H1","NOT_CONFIRMED","n",16,32,0.6,"99% CI overlaps null");
  bool rejected=false;try{s.hypothesis("H1","CONFIRMED","n",16,32,0.6,"forbidden promotion");}catch(const ps::Error&){rejected=true;}
  verify(rejected,"rejected evidence must not be promoted");
  s.hypothesis("H1","INVALIDATED","n",16,32,0.6,"invalidated after independent audit");
  s.artifact("samples.csv",ps::sha256("one"),"test-fixture");
  verify(count(s.raw(),"PRAGMA user_version")==6,"schema version 6");
  verify(count(s.raw(),"SELECT COUNT(*) FROM environments")==1,"environment registration");
  verify(count(s.raw(),"SELECT COUNT(*) FROM algorithms")==2,"algorithm registry");
  verify(count(s.raw(),"SELECT COUNT(*) FROM generators")==1,"generator registry");
  verify(count(s.raw(),"SELECT COUNT(*) FROM aggregates")==1,"aggregate rows");
  verify(count(s.raw(),"SELECT COUNT(*) FROM confirmations")==1,"frozen holdout verdict");
  verify(count(s.raw(),"SELECT COUNT(*) FROM artifacts")==1,"artifact provenance");
  verify(count(s.raw(),"SELECT COUNT(*) FROM samples WHERE cpu_ns IS NULL AND ns IS NULL")==1,"invalid counters and timings must be missing");
  verify(count(s.raw(),"SELECT COUNT(*) FROM samples WHERE cpu_ns=1100000 AND order_seed='aabbcc'")==2,"raw CPU/order provenance");
  auto reads=s.observations(c.id,false);
  verify(reads.size()==3&&reads[0].cpu_ns==1100000&&reads[0].order_seed=="aabbcc"&&reads[0].order_position==0,"raw sample roundtrip");
 }
 /* Independently reconstruct a legacy schema-1 file and exercise every
    forward migration with no destructive recreation or missing columns. */
 const auto legacy=(root/"legacy.sqlite").string();
 sqlite3* raw=nullptr;verify(sqlite3_open(legacy.c_str(),&raw)==SQLITE_OK,"open schema-1 fixture");
 const char* ddl=R"SQL(
 CREATE TABLE experiments(id INTEGER PRIMARY KEY CHECK(id=1),config TEXT NOT NULL,config_hash TEXT NOT NULL,environment TEXT NOT NULL,created_at TEXT NOT NULL);
 CREATE TABLE cases(case_id TEXT PRIMARY KEY,family TEXT NOT NULL,params TEXT NOT NULL,seed INTEGER NOT NULL,input BLOB NOT NULL,input_sha256 TEXT NOT NULL);
 CREATE TABLE samples(id INTEGER PRIMARY KEY AUTOINCREMENT,case_id TEXT NOT NULL REFERENCES cases(case_id),algorithm TEXT NOT NULL,status TEXT NOT NULL,ns INTEGER,batch INTEGER,work INTEGER,block INTEGER,holdout INTEGER NOT NULL,output_digest TEXT,detail TEXT);
 CREATE TABLE hypotheses(id TEXT PRIMARY KEY,status TEXT NOT NULL,axis TEXT NOT NULL,left_value INTEGER NOT NULL,right_value INTEGER NOT NULL,effect REAL NOT NULL,detail TEXT NOT NULL);
 CREATE TABLE witnesses(id INTEGER PRIMARY KEY AUTOINCREMENT,hypothesis_id TEXT REFERENCES hypotheses(id),case_id TEXT NOT NULL REFERENCES cases(case_id),status TEXT NOT NULL,trace TEXT NOT NULL);
 CREATE TABLE events(id INTEGER PRIMARY KEY AUTOINCREMENT,at TEXT NOT NULL,event TEXT NOT NULL,detail TEXT NOT NULL);
 PRAGMA user_version=1;
 )SQL";
 char* err=nullptr;auto result=sqlite3_exec(raw,ddl,nullptr,nullptr,&err);
 if(result!=SQLITE_OK){const auto msg=std::string(err?err:"sqlite error");sqlite3_free(err);sqlite3_close(raw);throw ps::Error(msg);}
 sqlite3_close(raw);
 {
  ps::Store upgraded(legacy);
  verify(count(upgraded.raw(),"PRAGMA user_version")==6,"schema 1->6 migration");
  verify(count(upgraded.raw(),"SELECT COUNT(*) FROM environments")==0,"new metadata initially empty");
  ps::Case c;c.family="calibration";c=ps::make_case(c);
  upgraded.add_case(c);verify(count(upgraded.raw(),"SELECT COUNT(*) FROM cases")==1,"legacy data insert after migration");
 }
 {
  ps::Store readonly(db,true);
  verify(count(readonly.raw(),"SELECT COUNT(*) FROM samples")==3,"read-only immutable evidence open");
 }
 std::cout<<"PASS SQLite v6 provenance, nullable counters, schema-1 migration, immutable read\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
