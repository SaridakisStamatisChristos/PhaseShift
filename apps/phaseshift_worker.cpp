#include "phaseshift/runner.hpp"
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
namespace ps {int worker_entry();}
namespace {
// Test-only fault injection for the V3-09 live-worker containment matrix.
// Inert unless PHASESHIFT_WORKER_FAULT is set; never alters a measurement.
void inject_fault(){
 const char* raw=std::getenv("PHASESHIFT_WORKER_FAULT");
 if(raw==nullptr)return;
 const std::string mode=raw;
 const char* file=std::getenv("PHASESHIFT_WORKER_FAULT_PIDFILE");
 auto note=[file](const char* role){if(file)std::ofstream(file,std::ios::app)<<role<<' '<<getpid()<<'\n';};
 note("worker");
 if(mode.rfind("stall_ms=",0)==0)std::this_thread::sleep_for(std::chrono::milliseconds(std::stoi(mode.substr(9))));
 else if((mode=="group_grandchild"||mode=="escaped_grandchild")&&fork()==0){
  // Escaped: new session plus double fork, leaving the worker's process group.
  if(mode=="escaped_grandchild"&&(setsid()<0||fork()!=0))_exit(0);
  note("grandchild");
  for(int i=0;i<600;++i)usleep(100000); // bounded 60 s even if containment fails
  _exit(0);
 }
}
}
int main(int argc,char** argv){
 std::signal(SIGPIPE,SIG_IGN);
 if(argc!=2||std::string(argv[1])!="--worker"){
  std::cerr<<"phaseshift_worker is a private IPC endpoint; launch via phaseshift\n";
  return 2;
 }
 inject_fault();
 return ps::worker_entry();
}
