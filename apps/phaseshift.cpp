#include "phaseshift/experiment.hpp"
#include "phaseshift/scaling.hpp"
#include "phaseshift/reproducer.hpp"
#include <csignal>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
namespace ps {int worker_entry();}
static void usage(){std::cout<<R"(PHASESHIFT v1.0.0-rc1 (CPU-only; empirical analysis)
Usage:
  phaseshift --help | --version | doctor
  phaseshift list families | algorithms
  phaseshift validate --config CONFIG
  phaseshift demo --out RUN_DIRECTORY
  phaseshift explore --config CONFIG --out RUN_DIRECTORY [--pause-after N]
  phaseshift resume --run RUN_DIRECTORY [--pause-after N]
  phaseshift ablate --config CONFIG --out DIRECTORY
  phaseshift intervene --run RUN_DIRECTORY --out NEW_DIRECTORY [--hypothesis H1] [--pairs 9]
  phaseshift scaling --run RUN_DIRECTORY --out NEW_DIRECTORY [--seeds 3]
  phaseshift confirm --run RUN_DIRECTORY [--hypothesis H1]
  phaseshift minimize --run RUN_DIRECTORY [--hypothesis H1]
  phaseshift minimize-case --run RUN_DIRECTORY --case CASE_ID --algorithm ALGORITHM
  phaseshift reproduce --run RUN_DIRECTORY --case CASE_ID --algorithm BUILTIN --out FILE.cpp
  phaseshift replay --run RUN_DIRECTORY --case CASE_ID_OR_PREFIX [--db SQLITE_PATH]
  phaseshift report --run RUN_DIRECTORY [--format markdown,json,csv,html]
  phaseshift export --run RUN_DIRECTORY --out BUNDLE_DIRECTORY
  phaseshift verify-bundle --bundle BUNDLE_DIRECTORY
  phaseshift import --bundle BUNDLE_DIRECTORY --out NEW_RUN_DIRECTORY
  phaseshift compare --a RUN_DIRECTORY --b RUN_DIRECTORY
)";}
int main(int argc,char** argv){
 std::signal(SIGPIPE,SIG_IGN);
 try{
  if(argc>1&&std::string(argv[1])=="--worker")return ps::worker_entry();
  if(argc<2){usage();return 0;}
  if(std::string(argv[1])=="--help"||std::string(argv[1])=="help"){if(argc!=2)throw ps::Error("unexpected help arguments");usage();return 0;}
  auto cmd=std::string(argv[1]);
  if(cmd=="--version"||cmd=="doctor"||cmd=="--help"||cmd=="help"){
   if(argc!=2)throw ps::Error("unexpected arguments to "+cmd);
  }
  if(cmd=="list"){
   if(argc!=3)throw ps::Error("list requires exactly one category: families or algorithms");
  }else if(cmd!="--version"&&cmd!="doctor"){
   const std::map<std::string,std::set<std::string>> allowed={
    {"validate",{"--config"}},{"demo",{"--out"}},
    {"explore",{"--config","--out","--pause-after"}},{"resume",{"--run","--pause-after"}},
    {"ablate",{"--config","--out"}},{"intervene",{"--run","--out","--hypothesis","--pairs"}},
    {"scaling",{"--run","--out","--seeds"}},{"confirm",{"--run","--hypothesis"}},
    {"minimize",{"--run","--hypothesis"}},{"minimize-case",{"--run","--case","--algorithm"}},
    {"reproduce",{"--run","--case","--algorithm","--out"}},
    {"replay",{"--run","--case","--algorithm","--expect","--db"}},
    {"report",{"--run","--format"}},{"export",{"--run","--out"}},
    {"verify-bundle",{"--bundle"}},{"import",{"--bundle","--out"}},
    {"compare",{"--a","--b"}}
   };
   if(const auto it=allowed.find(cmd);it!=allowed.end()){
    std::set<std::string> seen;
    for(int i=2;i<argc;i+=2){
     const std::string key=argv[i];
     if(!it->second.count(key))throw ps::Error("unknown option "+key+" for "+cmd);
     if(i+1>=argc||std::string(argv[i+1]).rfind("--",0)==0)throw ps::Error("missing value for "+key);
     if(!seen.insert(key).second)throw ps::Error("duplicate option: "+key);
    }
    if(cmd=="replay"&&seen.count("--algorithm")!=seen.count("--expect"))
     throw ps::Error("replay --algorithm and --expect must be provided together");
   }
  }
  if(cmd=="--version"){std::cout<<"PHASESHIFT 1.0.0-rc1 (config schema 1, SQLite schema 7, plugin ABI 1)\n";return 0;}
  if(cmd=="doctor"){std::cout<<"CPU-only runtime available\n"<<ps::environment()<<"\n";return 0;}
  if(cmd=="list"){
   if(argc<3)throw ps::Error("expected list families or algorithms");auto part=std::string(argv[2]);
   if(part=="families"){for(const auto& f:ps::families())std::cout<<f<<'\n';return 0;}
   if(part=="algorithms"){for(const auto& f:ps::families())for(const auto& alg:ps::algorithms_for(f))std::cout<<f<<": "<<alg<<'\n';return 0;}
   throw ps::Error("unknown list category");
  }
  auto option=[&](const std::string& key,const std::string& fallback=""){for(int i=2;i+1<argc;i++)if(std::string(argv[i])==key)return std::string(argv[i+1]);return fallback;};
  const auto run=option("--run");
  auto pause_after=[&]()->std::size_t{auto v=option("--pause-after");if(v.empty())return 0;std::size_t idx=0;auto n=std::stoull(v,&idx);if(idx!=v.size()||n==0||n>20000)throw ps::Error("invalid --pause-after");return static_cast<std::size_t>(n);};
  if(cmd=="validate"){auto c=ps::parse_config(option("--config"));std::cout<<"VALID: "<<c.experiment_id<<" family="<<c.family<<"\n";return 0;}
  if(cmd=="demo"){
   const auto out=option("--out","runs/demo");ps::Config c;c.experiment_id="known-synthetic-threshold";c.family="calibration";c.base.family=c.family;c.a="calibration";c.method="adaptive_partition";c.axis="x";c.axes["x"]={"0","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15"};c.blocks=2;c.confirm_blocks=20;c.base.threshold=8;
   ps::explore(c,out);ps::confirm(out);ps::minimize(out);return 0;
  }
  if(cmd=="ablate"){auto config=option("--config"),out=option("--out");if(config.empty()||out.empty())throw ps::Error("ablate requires --config and --out");ps::ablate(ps::parse_config(config),out);return 0;}
  if(cmd=="explore"){const auto config=option("--config"),out=option("--out");if(config.empty()||out.empty())throw ps::Error("explore requires --config and --out");ps::explore(ps::parse_config(config),out,pause_after());return 0;}
  if(cmd=="resume"){if(run.empty())throw ps::Error("resume requires --run");ps::resume(run,pause_after());return 0;}
  if(run.empty() && cmd!="compare" && cmd!="verify-bundle" && cmd!="import")throw ps::Error("command requires --run");
  if(cmd=="intervene"){
   const auto out=option("--out");if(out.empty())throw ps::Error("intervene requires --out");
   const auto raw=option("--pairs","9");std::size_t chars=0;const auto count=std::stoi(raw,&chars);
   if(chars!=raw.size())throw ps::Error("invalid --pairs");
   ps::intervene(run,out,option("--hypothesis","H1"),count);return 0;
  }
  if(cmd=="confirm"){ps::confirm(run,option("--hypothesis","H1"));return 0;}
  if(cmd=="minimize"){ps::minimize(run,option("--hypothesis","H1"));return 0;}
  if(cmd=="minimize-case"){ps::minimize_failure_case(run,option("--case"),option("--algorithm"));return 0;}
  if(cmd=="reproduce"){ps::reproduce_case(run,option("--case"),option("--algorithm"),option("--out"));return 0;}
  if(cmd=="replay"){auto c=option("--case");if(c.empty())throw ps::Error("replay requires --case");
   const auto expected=option("--expect");if(expected.empty())ps::replay(run,c,option("--db"));else ps::replay_expected_failure(run,c,option("--algorithm"),expected,option("--db"));return 0;}
  if(cmd=="report"){ps::report(run);std::cout<<"Generated markdown/json/csv/html in "<<run<<"\n";return 0;}
  if(cmd=="export"){auto out=option("--out");if(out.empty())throw ps::Error("export requires --out");ps::export_run(run,out);return 0;}
  if(cmd=="verify-bundle"){auto bundle=option("--bundle");if(bundle.empty())throw ps::Error("verify-bundle requires --bundle");ps::verify_bundle(bundle);return 0;}
  if(cmd=="import"){auto bundle=option("--bundle"),out=option("--out");if(bundle.empty()||out.empty())throw ps::Error("import requires --bundle and --out");ps::import_bundle(bundle,out);return 0;}
  if(cmd=="scaling"){auto out=option("--out");if(run.empty()||out.empty())throw ps::Error("scaling requires --run and --out");int seeds=3;auto value=option("--seeds");if(!value.empty())seeds=std::stoi(value);ps::scaling_study(run,out,seeds);return 0;}
  if(cmd=="compare"){auto a=option("--a"),b=option("--b");if(a.empty()||b.empty())throw ps::Error("compare requires --a and --b");ps::compare(a,b);return 0;}
  usage();throw ps::Error("unknown command: "+cmd);
 }catch(const std::exception& e){std::cerr<<"PHASESHIFT error: "<<e.what()<<"\n";return 2;}
}
