#include "phaseshift/store.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
static void check(bool x,const std::string& msg){if(!x)throw std::runtime_error(msg);}
static bool rejects(const auto& f){try{f();return false;}catch(const ps::Error&){return true;}}
static int count(sqlite3* db,const char* sql){sqlite3_stmt* p=nullptr;if(sqlite3_prepare_v2(db,sql,-1,&p,nullptr)!=SQLITE_OK)throw std::runtime_error(sql);if(sqlite3_step(p)!=SQLITE_ROW)throw std::runtime_error(sql);const auto n=sqlite3_column_int(p,0);sqlite3_finalize(p);return n;}
int main(int argc,char** argv){try{
 check(argc==2,"scratch dir required");
 std::filesystem::remove_all(argv[1]);std::filesystem::create_directories(argv[1]);
 ps::Store s(std::string(argv[1])+"/case.sqlite");s.experiment("{\"schema_version\":1}",ps::environment());
 s.hypothesis("H1","OBSERVED","x",7,8,1.5,"synthetic calibration","SYNTHETIC_CALIBRATION");
 s.hypothesis("H1","HYPOTHESIZED","x",7,8,1.5,"frozen selection");
 check(rejects([&]{s.hypothesis("H1","CONFIRMED","x",7,8,1.5,"unsupported");}),"direct promotion accepted");
 check(rejects([&]{s.seal_confirmation("H1","calibration",20);}),"premature seal accepted");
 s.hypothesis("H1","TESTED","x",7,8,1.5,"holdout started");
 check(rejects([&]{s.hypothesis("H1","CONFIRMED","x",7,8,1.5,"no samples");}),"zero-sample confirmation accepted");
 ps::Observation v;v.algorithm="calibration";v.status="ok";v.ns=1200000;v.batch=1;v.work=42;
 for(int i=0;i<19;i++)for(int side=0;side<2;side++){
  ps::Case c;c.family="calibration";c.x=side?8:7;c.seed=static_cast<std::uint64_t>(101+i);c=ps::make_case(c);
  s.add_observation(c,v,true);
 }
 check(rejects([&]{s.seal_confirmation("H1","calibration",20);}),"19 seed pairs accepted as 20");
 // Reused discovery case must be rejected, even when there are 20 pairs.
 for(int side=0;side<2;side++){
  ps::Case c;c.family="calibration";c.x=side?8:7;c.seed=120;c=ps::make_case(c);
  s.add_observation(c,v,true);
  if(side==0)s.add_observation(c,v,false);
 }
 check(rejects([&]{s.seal_confirmation("H1","calibration",20);}),"discovery/holdout overlap accepted");
 check(count(s.raw(),"SELECT count(*) FROM confirmation_seals")==0,"failed seal mutated evidence");
 // Fresh extra pair cannot hide a persisted overlapping case: fail closed.
 for(int side=0;side<2;side++){
  ps::Case c;c.family="calibration";c.x=side?8:7;c.seed=121;c=ps::make_case(c);
  s.add_observation(c,v,true);
 }
 check(rejects([&]{s.seal_confirmation("H1","calibration",20);}),"overlap laundered by unrelated pair");
 // A clean fixture can be sealed, then immediately audited for exact links.
 ps::Store good(std::string(argv[1])+"/good.sqlite");good.experiment("{\"schema_version\":1}",ps::environment());
 good.hypothesis("H1","OBSERVED","x",7,8,1.5,"frozen","SYNTHETIC_CALIBRATION");
 good.hypothesis("H1","HYPOTHESIZED","x",7,8,1.5,"frozen");
 good.hypothesis("H1","TESTED","x",7,8,1.5,"holdout start");
 for(int i=0;i<20;i++)for(int side=0;side<2;side++){
  ps::Case c;c.family="calibration";c.x=side?8:7;c.seed=static_cast<std::uint64_t>(1001+i);c=ps::make_case(c);
  good.add_observation(c,v,true);
 }
 good.seal_confirmation("H1","calibration",20);
 check(count(good.raw(),"SELECT COUNT(*) FROM confirmation_evidence")==40,"positive seal lacks 40 linked cases");
 check(count(good.raw(),"SELECT COUNT(DISTINCT seed_group) FROM confirmation_evidence")==20,"nonindependent seed pairs sealed");
 good.hypothesis("H1","CONFIRMED","x",7,8,1.5,"evidence-backed");
 check(count(good.raw(),"SELECT COUNT(*) FROM confirmations WHERE verdict='CONFIRMED'")==1,"confirmation not recorded");
 std::cout<<"PASS direct-promotion, no-holdout, insufficient pairs, overlap rejection, exact positive seal\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"confirmation integrity: "<<e.what()<<'\n';return 1;}}
