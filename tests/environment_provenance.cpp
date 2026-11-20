#include "phaseshift/experiment.hpp"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
int main(int argc,char** argv){
 try{
  if(argc!=2)throw std::runtime_error("require fixture directory");
  const auto env=ps::Json::parse(ps::environment());
  for(const auto& key:{"compiler_id","compiler_version","build_flags","source_revision","build_type","cpu_affinity","stdlib","kernel","architecture","process_priority"})
   if(!env.get(key))throw std::runtime_error(std::string("missing provenance ")+key);
  const auto folder=std::filesystem::path(argv[1]);std::filesystem::remove_all(folder);
  std::filesystem::create_directories(folder/"a");std::filesystem::create_directories(folder/"b");
  const std::string config=R"({"schema_version":1,"problem_family":"calibration","algorithms":["calibration"]})";
  {
   ps::Store a((folder/"a"/"run.sqlite").string());a.experiment(config,env.str());
   ps::Store b((folder/"b"/"run.sqlite").string());
   ps::Json other=ps::Json::parse(env.str());other.put("cpu_model","deliberately different fixture CPU");
   b.experiment(config,other.str());
  }
  std::ostringstream capture;
  auto* original=std::cout.rdbuf(capture.rdbuf());
  try{ps::compare((folder/"a").string(),(folder/"b").string());}
  catch(...){std::cout.rdbuf(original);throw;}
  std::cout.rdbuf(original);
  if(capture.str().find("MISMATCH: cpu_model")==std::string::npos || capture.str().find("ENVIRONMENT_COMPATIBILITY: MISMATCH")==std::string::npos)
   throw std::runtime_error("environment compatibility did not expose mismatch");
  std::cout<<"Build provenance and comparison mismatch PASS\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
