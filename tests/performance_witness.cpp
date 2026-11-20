#include "phaseshift/experiment.hpp"
#include "phaseshift/store.hpp"
#include <filesystem>
#include <iostream>
#include <string>
#include <sqlite3.h>
namespace fs=std::filesystem;
static void require(bool condition,const std::string& reason){if(!condition)throw ps::Error("TEST FAILURE: "+reason);}
int main(int argc,char** argv){
 try{
  if(argc!=2)throw ps::Error("usage: performance_witness RUN_ROOT");
  const auto root=fs::path(argv[1]);fs::remove_all(root);fs::create_directories(root);
  ps::Config c;c.experiment_id="forged-boundary-must-abstain";
  c.family="integer_array";c.base.family=c.family;c.base.distribution="uniform";
  c.a="std_sort";c.b="mergesort";c.method="grid";c.axis="n";
  c.axes["n"]={"16","32","64","128"};c.blocks=1;c.confirm_blocks=20;
  c.warmup=0;c.max_cases=4;c.total_wall_s=90;c.sample_ms=1000;c.seed=19273;
  const auto run=(root/"run").string();ps::explore(c,run);
  {
   ps::Store store(run+"/run.sqlite");
   /* Intentionally forge an internal CONFIRMED row ONLY in an isolated
      test fixture. A fresh independent minimization study must reject it. */
   const auto frozen=ps::case_params(c.base);
   /* Fabricate a strong *exploratory* contrast while both real algorithms
      and their actual holdout results remain identical. This tests that
      selection data cannot be recycled into minimization confirmation. */
   store.exec("UPDATE samples SET ns=ns*8 WHERE algorithm='std_sort' AND case_id IN (SELECT case_id FROM cases WHERE CAST(json_extract(params,'$.n') AS INTEGER)=128)");
   sqlite3_stmt* stmt=nullptr;
   const char* query="UPDATE hypotheses SET status='CONFIRMED',kind='PERFORMANCE_CLIFF',left_value=64,right_value=128,frozen_params=? WHERE id='H1'";
   require(sqlite3_prepare_v2(store.raw(),query,-1,&stmt,nullptr)==SQLITE_OK,"fixture status SQL prepare");
   sqlite3_bind_text(stmt,1,frozen.c_str(),-1,SQLITE_TRANSIENT);
   require(sqlite3_step(stmt)==SQLITE_DONE,"fixture status SQL update");
   sqlite3_finalize(stmt);
  }
  ps::minimize(run,"H1");
  const auto witness=ps::Json::file(run+"/witness.json");
  require(witness.s("category")=="PERFORMANCE_WITNESS","performance witness category");
  require(witness.s("status")=="MINIMIZED_PROVISIONAL","independent null controls must reject forged boundary");
  require(!witness.b("baseline_independently_valid"),"forged statistical result did not survive recheck");
  require(fs::exists(run+"/minimization.sqlite"),"independently isolated raw minimization data");
  const auto bundle=(root/"export").string();ps::export_run(run,bundle);ps::verify_bundle(bundle);
  require(fs::exists(bundle+"/minimization.sqlite"),"portable minimization source evidence");
  std::cout<<"PASS: forged timing boundary rejected on new held-out inputs, witnessed and exported\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
