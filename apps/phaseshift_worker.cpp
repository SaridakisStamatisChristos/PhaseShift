#include "phaseshift/runner.hpp"
#include <csignal>
#include <iostream>
#include <string>
namespace ps {int worker_entry();}
int main(int argc,char** argv){
 std::signal(SIGPIPE,SIG_IGN);
 if(argc!=2||std::string(argv[1])!="--worker"){
  std::cerr<<"phaseshift_worker is a private IPC endpoint; launch via phaseshift\n";
  return 2;
 }
 return ps::worker_entry();
}
