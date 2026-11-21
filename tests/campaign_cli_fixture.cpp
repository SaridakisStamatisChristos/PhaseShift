#include "phaseshift/store.hpp"
#include <filesystem>
#include <iostream>
int main(int argc,char** argv){
 try{
  if(argc!=2)throw ps::Error("campaign fixture requires output directory");
  const std::filesystem::path root(argv[1]);
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  ps::Store s((root/"run.sqlite").string());
  s.upgrade_v2_schema();
  s.experiment(R"({"schema_version":1,"policy_version":"v2","confirm_blocks":20})",ps::environment());
  for(int i=1;i<=2;++i){
   const auto id="H"+std::to_string(i);
   s.hypothesis(id,"OBSERVED","x",static_cast<std::int64_t>(i),static_cast<std::int64_t>(i+1),1.2,"discovered","PERFORMANCE_CLIFF");
   s.hypothesis(id,"HYPOTHESIZED","x",static_cast<std::int64_t>(i),static_cast<std::int64_t>(i+1),1.2,"frozen");
  }
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
