#include "phaseshift/experiment.hpp"
#include <csignal>
#include <filesystem>
#include <iostream>
namespace ps {int worker_entry();}
static void usage(){std::cout<<R"(PHASESHIFT v0.1.0 (CPU-only; empirical analysis)
Usage:
  phaseshift --help | --version | doctor
  phaseshift list families | algorithms
  phaseshift validate --config CONFIG
  phaseshift demo --out RUN_DIRECTORY
  phaseshift explore --config CONFIG --out RUN_DIRECTORY
  phaseshift confirm --run RUN_DIRECTORY [--hypothesis H1]
  phaseshift minimize --run RUN_DIRECTORY [--hypothesis H1]
  phaseshift replay --run RUN_DIRECTORY --case CASE_ID_OR_PREFIX
  phaseshift report --run RUN_DIRECTORY [--format markdown,json,csv,html]
  phaseshift export --run RUN_DIRECTORY --out BUNDLE_DIRECTORY
  phaseshift compare --a RUN_DIRECTORY --b RUN_DIRECTORY
)";}
int main(int argc,char** argv){
 std::signal(SIGPIPE,SIG_IGN);
 try{
  if(argc>1&&std::string(argv[1])=="--worker")return ps::worker_entry();
  if(argc<2||std::string(argv[1])=="--help"||std::string(argv[1])=="help"){usage();return 0;}
  auto cmd=std::string(argv[1]);if(cmd=="--version"){std::cout<<"PHASESHIFT 0.1.0 (schema 1, plugin ABI 1)\n";return 0;}
  if(cmd=="doctor"){std::cout<<"CPU-only runtime available\n"<<ps::environment()<<"\n";return 0;}
  if(cmd=="list"){
   if(argc<3)throw ps::Error("expected list families or algorithms");auto part=std::string(argv[2]);
   if(part=="families"){for(const auto& f:ps::families())std::cout<<f<<'\n';return 0;}
   if(part=="algorithms"){for(const auto& f:ps::families())for(const auto& alg:ps::algorithms_for(f))std::cout<<f<<": "<<alg<<'\n';return 0;}
   throw ps::Error("unknown list category");
  }
  auto option=[&](const std::string& key,const std::string& fallback=""){for(int i=2;i+1<argc;i++)if(std::string(argv[i])==key)return std::string(argv[i+1]);return fallback;};
  const auto run=option("--run");
  if(cmd=="validate"){auto c=ps::parse_config(option("--config"));std::cout<<"VALID: "<<c.experiment_id<<" family="<<c.family<<"\n";return 0;}
  if(cmd=="demo"){
   const auto out=option("--out","runs/demo");ps::Config c;c.experiment_id="known-synthetic-threshold";c.family="calibration";c.base.family=c.family;c.a="calibration";c.method="adaptive_partition";c.axis="x";c.axes["x"]={"0","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15"};c.blocks=2;c.confirm_blocks=20;c.base.threshold=8;
   ps::explore(c,out);ps::confirm(out);ps::minimize(out);return 0;
  }
  if(cmd=="explore"){const auto config=option("--config"),out=option("--out");if(config.empty()||out.empty())throw ps::Error("explore requires --config and --out");ps::explore(ps::parse_config(config),out);return 0;}
  if(run.empty() && cmd!="compare")throw ps::Error("command requires --run");
  if(cmd=="confirm"){ps::confirm(run,option("--hypothesis","H1"));return 0;}
  if(cmd=="minimize"){ps::minimize(run,option("--hypothesis","H1"));return 0;}
  if(cmd=="replay"){auto c=option("--case");if(c.empty())throw ps::Error("replay requires --case");ps::replay(run,c);return 0;}
  if(cmd=="report"){ps::report(run);std::cout<<"Generated markdown/json/csv/html in "<<run<<"\n";return 0;}
  if(cmd=="export"){auto out=option("--out");if(out.empty())throw ps::Error("export requires --out");ps::export_run(run,out);return 0;}
  if(cmd=="compare"){auto a=option("--a"),b=option("--b");if(a.empty()||b.empty())throw ps::Error("compare requires --a and --b");ps::compare(a,b);return 0;}
  usage();throw ps::Error("unknown command: "+cmd);
 }catch(const std::exception& e){std::cerr<<"PHASESHIFT error: "<<e.what()<<"\n";return 2;}
}
