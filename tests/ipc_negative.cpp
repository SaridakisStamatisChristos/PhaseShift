#include <array>
#include <chrono>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
namespace fs=std::filesystem;
static void verify_worker_rejection(const std::string& label,const std::vector<std::uint8_t>& request,const std::string& worker){
 int descriptors[2];if(pipe(descriptors)!=0)throw std::runtime_error("pipe failed");
 const auto child=fork();if(child<0)throw std::runtime_error("fork failed");
 if(child==0){
  close(descriptors[1]);dup2(descriptors[0],STDIN_FILENO);close(descriptors[0]);
  execl(worker.c_str(),"phaseshift_worker","--worker",static_cast<char*>(nullptr));_exit(127);
 }
 close(descriptors[0]);
 std::size_t pos=0;
 while(pos<request.size()){
  const auto n=write(descriptors[1],request.data()+pos,request.size()-pos);
  if(n<0&&errno==EINTR)continue;
  if(n<=0)throw std::runtime_error("write failure in IPC fixture");
  pos+=static_cast<std::size_t>(n);
 }
 close(descriptors[1]);
 int status=0;pid_t finished=0;
 for(int t=0;t<300;t++){
  finished=waitpid(child,&status,WNOHANG);
  if(finished!=0)break;
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
 }
 if(finished==0){kill(child,SIGKILL);waitpid(child,&status,0);throw std::runtime_error(label+" worker hung on malformed IPC");}
 if(finished<0||!WIFEXITED(status)||WEXITSTATUS(status)!=5)
  throw std::runtime_error(label+" accepted malformed frame or crashed without structured failure");
}
static std::vector<std::uint8_t> frame(const std::string& magic,std::uint32_t declared,const std::string& payload){
 std::vector<std::uint8_t> bytes;
 bytes.insert(bytes.end(),magic.begin(),magic.end());
 for(int i=0;i<4;i++)bytes.push_back(static_cast<std::uint8_t>((declared>>(i*8))&255));
 bytes.insert(bytes.end(),payload.begin(),payload.end());return bytes;
}
int main(int argc,char** argv){
 try{
  if(argc!=2)throw std::runtime_error("usage: ipc_negative WORKER_BIN");
  const std::string worker=argv[1];
  verify_worker_rejection("empty",{},worker);
  verify_worker_rejection("bad version",frame("PS00",0,""),worker);
  verify_worker_rejection("oversized frame",frame("PS01",1048577,""),worker);
  verify_worker_rejection("truncated frame",frame("PS01",32,"{}"),worker);
  verify_worker_rejection("invalid JSON",frame("PS01",3,"xyz"),worker);
  verify_worker_rejection("no second binary payload",frame("PS01",2,"{}"),worker);
  std::cout<<"PASS: 6 malformed/oversized/truncated IPC frames rejected by dedicated worker\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<"IPC falsification FAILED: "<<e.what()<<'\n';return 1;}
}
