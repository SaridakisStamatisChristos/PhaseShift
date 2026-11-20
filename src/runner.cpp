#include "phaseshift/runner.hpp"
#include "phaseshift/plugin.h"
#include <algorithm>
#include <chrono>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
namespace ps {
static volatile std::uint64_t observable_sink=0;
static Result run_plugin(const Case& c,const std::string& path){
 if(c.family!="integer_array")throw Error("v1 plugin adapter supports integer_array only");
 void* handle=dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL);if(!handle)throw Error(std::string("dlopen: ")+dlerror());
 struct Close {void* h;~Close(){if(h)dlclose(h);}} guard{handle};
 auto get=reinterpret_cast<ps_get_algorithm_v1>(dlsym(handle,"phaseshift_get_algorithm_v1"));
 if(!get)throw Error("missing phaseshift_get_algorithm_v1");
 const ps_algorithm_v1* api=get();
 if(!api||api->abi_version!=PHASESHIFT_PLUGIN_ABI_VERSION||!api->run||!api->algorithm_id)throw Error("invalid plugin ABI/version");
 const ps_input_v1 in{c.bytes.data(),c.bytes.size()};if(api->validate_input&&api->validate_input(in)!=PHASESHIFT_PLUGIN_OK)throw Error("plugin invalid input");
 if(api->reset)api->reset();
 ps_output_v1 out{};
 int status=PHASESHIFT_PLUGIN_ERROR;
 try{status=api->run(in,&out);}catch(...){if(out.release&&out.data)out.release(out.data,out.len);throw Error("plugin threw exception across C ABI");}
 if(status!=PHASESHIFT_PLUGIN_OK){if(out.release&&out.data)out.release(out.data,out.len);throw Error("plugin run returned failure: "+std::to_string(status));}
 if(!out.release || (out.len&&!out.data)||out.len>c.bytes.size()||out.len%8){if(out.release&&out.data)out.release(out.data,out.len);throw Error("plugin output shape/ownership invalid");}
 Bytes bytes;if(out.len)bytes.assign(out.data,out.data+out.len);out.release(out.data,out.len);
 return {decode_values(bytes),0};
}
static Result run_any(const Case& c,const std::string& algorithm){if(algorithm.rfind("plugin:",0)==0)return run_plugin(c,algorithm.substr(7));return execute(c,algorithm);}
static Json measure_one(const Case& c,const std::string& algorithm,int block,int warmup){
 Json v;v.put("algorithm",algorithm);v.put("block",static_cast<std::int64_t>(block));v.put("status","ok");
 try{
  const auto expected=oracle(c).values;
  auto trial=run_any(c,algorithm);
  if(trial.values!=expected){v.put("status","wrong_answer");v.put("detail","candidate disagrees with independent oracle");v.put("digest",sha256(encode_values(trial.values)));v.put("work",static_cast<std::int64_t>(trial.work));v.put("ns",static_cast<std::int64_t>(0));v.put("batch",static_cast<std::int64_t>(0));return v;}
  for(int i=0;i<warmup;i++){auto w=run_any(c,algorithm);observable_sink=observable_sink^w.work;}
  std::uint64_t iterations=1,ns=0;Result last;
  for(;;){
   const auto start=std::chrono::steady_clock::now();
   for(std::uint64_t i=0;i<iterations;i++){last=run_any(c,algorithm);observable_sink=observable_sink^last.work;}
   const auto stop=std::chrono::steady_clock::now();ns=static_cast<std::uint64_t>(std::max<std::int64_t>(1,std::chrono::duration_cast<std::chrono::nanoseconds>(stop-start).count()));
   if(ns>=1000000||iterations>=2048||ns>500000000)break;
   iterations=std::min<std::uint64_t>(2048,iterations*2);
  }
  v.put("ns",static_cast<std::int64_t>(ns));v.put("batch",static_cast<std::int64_t>(iterations));v.put("work",static_cast<std::int64_t>(trial.work));v.put("digest",sha256(encode_values(last.values)));
 }catch(const std::bad_alloc&){v.put("status","out_of_memory");v.put("detail","std::bad_alloc");}
 catch(const std::exception& e){v.put("status","invalid_input");v.put("detail",std::string(e.what()).substr(0,400));}
 return v;
}
Json worker_measure(const Json& request){
 auto params=request.s("params");auto family=request.s("family");auto bytes=hex_decode(request.s("bytes"));
 if(bytes.size()>32ULL*1024*1024)throw Error("IPC request bytes exceed 32 MiB");
 auto c=decode_case(family,params,static_cast<std::uint64_t>(request.i("seed")),bytes);
 auto* a=request.get("algorithms");if(!a||json_object_get_type(a)!=json_type_array||json_object_array_length(a)==0||json_object_array_length(a)>2)throw Error("invalid algorithm list");
 std::vector<std::string> names;for(std::size_t i=0;i<json_object_array_length(a);i++){auto* e=json_object_array_get_idx(a,i);if(json_object_get_type(e)!=json_type_string)throw Error("algorithm name must be string");names.emplace_back(json_object_get_string(e));}
 const int blocks=static_cast<int>(request.i("blocks",9)),warmup=static_cast<int>(request.i("warmup",2));
 if(blocks<1||blocks>100||warmup<0||warmup>100)throw Error("invalid measurement bounds");
 Json answer;auto* list=json_object_new_array();
 for(int i=0;i<blocks;i++){
  std::vector<std::string> order=names;if(names.size()==2){if(i%2==0)order={names[0],names[1],names[1],names[0]};else order={names[1],names[0],names[0],names[1]};}
  for(const auto& alg:order){auto v=measure_one(c,alg,i,warmup);json_object_array_add(list,v.p);v.p=nullptr;}
 }
 json_object_object_add(answer.p,"observations",list);return answer;
}
static void all_write(int fd,const std::uint8_t* b,std::size_t n){while(n){auto m=::write(fd,b,n);if(m<0&&errno==EINTR)continue;if(m<=0)throw Error("IPC write failed");b+=m;n-=static_cast<std::size_t>(m);}}
static void write_frame(int fd,const std::string& s){if(s.size()>64ULL*1024*1024)throw Error("IPC payload too large");std::uint8_t length[4];const auto n=static_cast<std::uint32_t>(s.size());for(int i=0;i<4;i++)length[i]=static_cast<std::uint8_t>((n>>(8*i))&255);all_write(fd,length,4);all_write(fd,reinterpret_cast<const std::uint8_t*>(s.data()),s.size());}
static bool read_exact(int fd,std::uint8_t* p,std::size_t n,int timeout_ms){
 while(n){struct pollfd f{fd,POLLIN,0};auto r=poll(&f,1,timeout_ms);if(r<0&&errno==EINTR)continue;if(r<=0) return false;auto got=::read(fd,p,n);if(got<0&&errno==EINTR)continue;if(got<=0)return false;p+=got;n-=static_cast<std::size_t>(got);}return true;
}
static std::string read_frame(int fd,int timeout_ms){
 std::uint8_t h[4];if(!read_exact(fd,h,4,timeout_ms))throw Error("worker response timeout/crash");std::uint32_t n=0;for(int i=0;i<4;i++)n|=static_cast<std::uint32_t>(h[i])<<(i*8);if(n>8*1024*1024)throw Error("oversized worker response");std::string s(n,'\0');if(n&&!read_exact(fd,reinterpret_cast<std::uint8_t*>(s.data()),n,timeout_ms))throw Error("truncated worker response");return s;
}
std::vector<Observation> measure(const Case& c,const std::vector<std::string>& names,int blocks,int warmup,int timeout_ms,int memory_mb){
 if(names.empty()||names.size()>2||blocks<1||blocks>100||timeout_ms<100||timeout_ms>60000||memory_mb<64||memory_mb>65536)throw Error("invalid runner limits");
 int in[2],out[2];if(pipe(in)!=0)throw Error("worker input pipe failed");if(pipe(out)!=0){close(in[0]);close(in[1]);throw Error("worker output pipe failed");}
 const pid_t pid=fork();if(pid<0){close(in[0]);close(in[1]);close(out[0]);close(out[1]);throw Error("fork failed");}
 if(pid==0){
  close(in[1]);close(out[0]);setsid();dup2(in[0],STDIN_FILENO);dup2(out[1],STDOUT_FILENO);close(in[0]);close(out[1]);
  #ifndef PHASESHIFT_ASAN_BUILD
  struct rlimit mem{static_cast<rlim_t>(memory_mb)*1024*1024,static_cast<rlim_t>(memory_mb)*1024*1024};setrlimit(RLIMIT_AS,&mem);
#else
  (void)memory_mb; /* ASan reserves terabytes of virtual shadow address space. */
#endif
  struct rlimit cpu{static_cast<rlim_t>(std::max(1,(timeout_ms/1000)+1)),static_cast<rlim_t>(std::max(2,(timeout_ms/1000)+2))};setrlimit(RLIMIT_CPU,&cpu);
  execl("/proc/self/exe","phaseshift","--worker",static_cast<char*>(nullptr));_exit(127);
 }
 close(in[0]);close(out[1]);
 std::vector<Observation> result;
 try{
  Json req;req.put("family",c.family);req.put("params",case_params(c));req.put("seed",static_cast<std::int64_t>(c.seed));req.put("bytes",hex_encode(c.bytes));req.put("blocks",static_cast<std::int64_t>(blocks));req.put("warmup",static_cast<std::int64_t>(warmup));
  auto* arr=json_object_new_array();for(const auto& name:names)json_object_array_add(arr,json_object_new_string(name.c_str()));json_object_object_add(req.p,"algorithms",arr);
  write_frame(in[1],req.str());close(in[1]);in[1]=-1;
  const auto response=Json::parse(read_frame(out[0],timeout_ms));auto* list=response.get("observations");
  if(!list||json_object_get_type(list)!=json_type_array)throw Error("malformed worker response");
  for(std::size_t i=0;i<json_object_array_length(list);i++){
   Json v(json_object_get(json_object_array_get_idx(list,static_cast<int>(i))));Observation s;
   s.algorithm=v.s("algorithm");s.status=v.s("status","infrastructure_error");s.digest=v.s("digest");s.detail=v.s("detail");s.ns=v.i("ns");s.work=v.i("work");s.batch=v.i("batch",1);s.block=v.i("block");result.push_back(s);
  }
  if(result.empty())throw Error("empty worker response");
 }catch(const std::exception& e){
  result.clear();for(const auto& n:names){Observation s;s.algorithm=n;s.detail=e.what();s.status=s.detail.find("timeout")!=std::string::npos?"timeout":"crashed";result.push_back(s);}
 }
 if(in[1]>=0)close(in[1]);
 close(out[0]);kill(-pid,SIGKILL);kill(pid,SIGKILL);int status=0;while(waitpid(pid,&status,0)<0&&errno==EINTR){}
 return result;
}
int worker_entry(){
 try{
  std::uint8_t h[4];if(!read_exact(STDIN_FILENO,h,4,3000))return 2;std::uint32_t n=0;for(int i=0;i<4;i++)n|=static_cast<std::uint32_t>(h[i])<<(8*i);if(n>64*1024*1024)return 3;
  std::string input(n,'\0');if(!read_exact(STDIN_FILENO,reinterpret_cast<std::uint8_t*>(input.data()),n,3000))return 4;
  auto answer=worker_measure(Json::parse(input));write_frame(STDOUT_FILENO,answer.str());return 0;
 }catch(const std::exception&){return 5;}
}
}
namespace ps {int worker_entry();}
