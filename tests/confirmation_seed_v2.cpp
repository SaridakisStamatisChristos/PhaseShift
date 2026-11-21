#include "phaseshift/store.hpp"
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#define CHECK(cond) do{if(!(cond))throw std::runtime_error("seed-isolation: " #cond);}while(false)
int main(int argc,char** argv){
 try{
  CHECK(argc==2);
  std::filesystem::remove_all(argv[1]);
  std::filesystem::create_directories(argv[1]);
  ps::Store s(std::string(argv[1])+"/cohort.sqlite");
  s.experiment(R"({"schema_version":1,"policy_version":"v2"})",ps::environment());
  s.upgrade_v2_schema();
  s.hypothesis("H2","OBSERVED","x",7,8,2,"frozen","SYNTHETIC_CALIBRATION");
  s.hypothesis("H2","HYPOTHESIZED","x",7,8,2,"frozen");
  s.hypothesis("H2","TESTED","x",7,8,2,"holdout");
  ps::Observation v;v.algorithm="calibration";v.status="ok";v.ns=1000000;v.batch=1;v.work=11;
  // Twenty exact paired HOLDOUT groups exist, but only for a different
  // hypothesis; a naive database-wide seal would accept them.
  for(int i=0;i<20;++i)for(int side=0;side<2;++side){
   ps::Case c;c.family="calibration";c.x=side?8:7;c.seed=10000+static_cast<std::uint64_t>(i);
   c=ps::make_case(c);s.add_observation(c,v,true);
  }
  const std::vector<std::uint64_t> intended=[]{
   std::vector<std::uint64_t> x;for(int i=0;i<20;++i)x.push_back(20000+static_cast<std::uint64_t>(i));
   return x;
  }();
  bool fail=false;
  try{s.seal_confirmation("H2","calibration",20,false,intended);}
  catch(const ps::Error&){fail=true;}CHECK(fail);
  CHECK(sqlite3_get_autocommit(s.raw())==1);
  // A broken/duplicated frozen seed manifest must also fail.
  auto duplicate=intended;duplicate[2]=duplicate[1];fail=false;
  try{s.seal_confirmation("H2","calibration",20,false,duplicate);}
  catch(const ps::Error&){fail=true;}CHECK(fail);
  // Genuine exact cohorts pass. Unrelated previous twenty pairs must not be
  // attached to the final H2 seal.
  for(int i=0;i<20;++i)for(int side=0;side<2;++side){
   ps::Case c;c.family="calibration";c.x=side?8:7;c.seed=20000+static_cast<std::uint64_t>(i);
   c=ps::make_case(c);s.add_observation(c,v,true);
  }
  s.seal_confirmation("H2","calibration",20,false,intended);
  sqlite3_stmt* st=nullptr;
  CHECK(sqlite3_prepare_v2(s.raw(),"SELECT case_count FROM confirmation_seals WHERE hypothesis_id='H2'",-1,&st,nullptr)==SQLITE_OK);
  CHECK(sqlite3_step(st)==SQLITE_ROW&&sqlite3_column_int(st,0)==40);
  sqlite3_finalize(st);
  CHECK(sqlite3_prepare_v2(s.raw(),"SELECT COUNT(DISTINCT seed_group) FROM confirmation_evidence WHERE hypothesis_id='H2'",-1,&st,nullptr)==SQLITE_OK);
  CHECK(sqlite3_step(st)==SQLITE_ROW&&sqlite3_column_int(st,0)==20);
  sqlite3_finalize(st);
  return 0;
 }catch(const std::exception&){return 1;}
}
