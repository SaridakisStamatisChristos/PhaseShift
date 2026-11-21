#include "phaseshift/experiment.hpp"
#include "phaseshift/campaign_v2.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <sqlite3.h>
#define CHECK(x) do{if(!(x))throw std::runtime_error("multi-witness: " #x);}while(false)
static int count(sqlite3* db,const char* sql){
 sqlite3_stmt* s=nullptr;CHECK(sqlite3_prepare_v2(db,sql,-1,&s,nullptr)==SQLITE_OK);
 CHECK(sqlite3_step(s)==SQLITE_ROW);
 const int answer=sqlite3_column_int(s,0);sqlite3_finalize(s);return answer;
}
int main(int argc,char** argv){try{
 CHECK(argc==2);
 namespace fs=std::filesystem;
 const fs::path run(argv[1]);
 fs::remove_all(run);fs::create_directories(run);
 {
  ps::Store s((run/"run.sqlite").string());
  s.upgrade_v2_schema();
  s.experiment(R"({"schema_version":1,"experiment_id":"v2-witness-isolation",
   "policy_version":"v2","problem_family":"calibration","algorithm_a":"calibration",
   "search_method":"uniform_v2","primary_axis":"x","seed":141,
   "max_cases":3,"discovery_blocks":1,"confirm_blocks":20,"warmup":0,
   "sample_ms":2000,"minimum_batch_ms":1,"rss_mb":1024,
   "total_wall_seconds":90,"fdr_q":0.05,"confirmation_confidence":0.99,
   "base_params":{"x":0,"threshold":8},
   "axes":{"x":["6","7","8"]}})",ps::environment());
  for(int rank=1;rank<=2;++rank){
   const std::string id="H"+std::to_string(rank);
   const auto left=rank==1?7:6;
   s.hypothesis(id,"OBSERVED","x",left,8,3.0,"synthetic fixture","PERFORMANCE_CLIFF");
   s.hypothesis(id,"HYPOTHESIZED","x",left,8,3.0,"frozen geometry");
  }
  const auto manifest=ps::Json::parse(R"({"schema_version":1,
   "campaign_id":"witness-test","members":["H1","H2"],"alpha":0.05,
   "confirm_seed_pairs":20,"test":"paired_sign_flip_v1"})");
  ps::freeze_campaign_v2(s,manifest);
  ps::Observation o;o.algorithm="calibration";o.status="ok";o.batch=1;
  o.work=100;o.ns=1000;o.cpu_ns=500;
  for(int rank=1;rank<=2;++rank){
   const std::string id="H"+std::to_string(rank);
   const auto left=rank==1?7:6;
   s.hypothesis(id,"TESTED","x",left,8,3.0,"independent holdout");
   std::vector<std::uint64_t> seeds;
   for(int i=0;i<20;++i){
    const auto seed=static_cast<std::uint64_t>(100000*rank+i);
    seeds.push_back(seed);
    for(int side=0;side<2;++side){
     ps::Case c;c.family="calibration";c.threshold=8;
     c.x=side==0?left:8;c.seed=seed;c=ps::make_case(c);
     s.add_observation(c,o,true);
    }
   }
   s.seal_confirmation(id,"calibration",20,false,seeds);
   s.hypothesis(id,"CONFIRMED","x",left,8,3.0,"independently sealed");
  }
  CHECK(count(s.raw(),"SELECT COUNT(*) FROM confirmation_seals")==2);
  CHECK(count(s.raw(),"SELECT COUNT(*) FROM confirmation_evidence")==80);
 }
 ps::minimize(run.string(),"H1");
 CHECK(fs::exists(run/"witness.cpp"));
 CHECK(fs::exists(run/"witness.hex"));
 CHECK(!fs::exists(run/"witnesses"/"H2"/"witness.hex"));
 ps::minimize(run.string(),"H2");
 CHECK(fs::exists(run/"witness.cpp"));
 CHECK(fs::exists(run/"witness.hex"));
 CHECK(fs::exists(run/"witnesses"/"H2"/"witness.cpp"));
 CHECK(fs::exists(run/"witnesses"/"H2"/"witness.hex"));
 {
  ps::Store s((run/"run.sqlite").string());
  CHECK(count(s.raw(),"SELECT COUNT(*) FROM hypotheses WHERE status='MINIMIZED_CONFIRMED'")==2);
  CHECK(count(s.raw(),"SELECT COUNT(*) FROM witnesses WHERE status='MINIMIZED_CONFIRMED'")==2);
 }
 std::cout<<"PASS independent v2 H1/H2 confirmed witness artifacts and legacy H1 layout\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
