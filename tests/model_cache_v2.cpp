#include "phaseshift/model_cache_v2.hpp"
#include <filesystem>
#include <sqlite3.h>
#include <stdexcept>
#define CHECK(e) do{if(!(e))throw std::runtime_error("model_cache_v2: " #e);}while(false)
int main(int argc,char** argv){
 CHECK(argc==2);
 const std::filesystem::path dir(argv[1]);
 std::filesystem::remove_all(dir);
 std::filesystem::create_directories(dir);
 const auto cache=(dir/"checkpoint.sqlite").string();
 ps::Observation o;o.algorithm="calibration";o.status="ok";o.work=123;
 o.ns=200;o.batch=1;o.cpu_ns=1000000;o.block=0;
 const auto rejected=[&](auto&& run){try{run();}catch(const ps::Error&){return true;}return false;};
 {
  ps::ModelCheckpointCacheV2 a(cache,"config-id");
  a.remember(1,7,"case1",0xf00,{2.},2.,{o});
  a.remember(1,7,"case1",0xf00,{2.},2.,{o});
  a.verify_no_ahead(1);
  CHECK(rejected([&]{a.verify_no_ahead(0);}));
  CHECK(rejected([&]{a.remember(1,7,"case1",0xf00,{3.},2.,{o});}));
  CHECK(rejected([&]{a.remember(2,8,"case2",0xf01,{2.},0.,{o});}));
  CHECK(rejected([&]{a.remember(2,7,"case2",0xf01,{2.},2.,{o});}));
 }
 {
  ps::ModelCheckpointCacheV2 resumed(cache,"config-id");
  resumed.remember(1,7,"case1",0xf00,{2.},2.,{o});
  o.status="wrong_answer";
  o.ns=0;o.batch=0;o.work=0;o.cpu_ns=-1;
  resumed.remember(2,8,"case2",0xf01,std::nullopt,1.,{o});
  resumed.verify_no_ahead(2);
  CHECK(rejected([&]{resumed.remember(2,8,"case2",0xf01,{1.},1.,{o});}));
 }
 CHECK(rejected([&]{ps::ModelCheckpointCacheV2 mismatched(cache,"other-config");}));
 sqlite3* db=nullptr;
 CHECK(sqlite3_open(cache.c_str(),&db)==SQLITE_OK);
 CHECK(sqlite3_exec(db,"UPDATE checkpoints SET sample_sha256='tamper' WHERE ordinal=2",
                    nullptr,nullptr,nullptr)==SQLITE_OK);
 sqlite3_close(db);
 CHECK(rejected([&]{
  ps::ModelCheckpointCacheV2 resumed(cache,"config-id");
  resumed.remember(2,8,"case2",0xf01,std::nullopt,1.,{o});
 }));
 return 0;
}
