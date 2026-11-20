#include "phaseshift/runner.hpp"
#include "phaseshift/plugin.h"
#include <algorithm>
#include <chrono>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <dlfcn.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
namespace ps {
static volatile std::uint64_t observable_sink=0;
/* One persistent child-worker registry per experiment case. Shared libraries
   are loaded exactly once before calibration and kept mapped until worker
   exit; dlopen/dlsym/dlclose are never part of the timed operation. */
struct LoadedPlugin {
 void* handle=nullptr;
 const ps_algorithm_v1* api=nullptr;
 explicit LoadedPlugin(const std::string& path){
  handle=dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL);
  if(!handle)throw Error(std::string("dlopen: ")+dlerror());
  auto get=reinterpret_cast<ps_get_algorithm_v1>(dlsym(handle,"phaseshift_get_algorithm_v1"));
  if(!get){dlclose(handle);handle=nullptr;throw Error("missing phaseshift_get_algorithm_v1");}
  api=get();
  if(!api||api->abi_version!=PHASESHIFT_PLUGIN_ABI_VERSION||!api->run||!api->algorithm_id){
   dlclose(handle);handle=nullptr;throw Error("invalid plugin ABI/version");
  }
 }
 ~LoadedPlugin(){if(handle)dlclose(handle);}
 LoadedPlugin(const LoadedPlugin&)=delete;
 LoadedPlugin& operator=(const LoadedPlugin&)=delete;
};
static LoadedPlugin& get_plugin(const std::string& path){
 static std::map<std::string,std::unique_ptr<LoadedPlugin>> registry;
 auto it=registry.find(path);
 if(it==registry.end())it=registry.emplace(path,std::make_unique<LoadedPlugin>(path)).first;
 return *it->second;
}
static Result run_plugin(const Case& c,const std::string& path){
 if(c.family!="integer_array")throw Error("v1 plugin adapter supports integer_array only");
 const auto* api=get_plugin(path).api;
 const ps_input_v1 in{c.bytes.data(),c.bytes.size()};
 if(api->validate_input&&api->validate_input(in)!=PHASESHIFT_PLUGIN_OK)throw Error("plugin invalid input");
 if(api->reset)api->reset();
 ps_output_v1 out{};
 int status=PHASESHIFT_PLUGIN_ERROR;
 try{status=api->run(in,&out);}catch(const std::bad_alloc&){if(out.release&&out.data)out.release(out.data,out.len);throw;}
 catch(...){if(out.release&&out.data)out.release(out.data,out.len);throw Error("plugin threw exception across C ABI");}
 if(status!=PHASESHIFT_PLUGIN_OK){
  if(out.release&&out.data)out.release(out.data,out.len);
  if(status==PHASESHIFT_PLUGIN_OUT_OF_MEMORY)throw std::bad_alloc();
  throw Error("plugin run returned failure: "+std::to_string(status));
 }
 if(!out.release || (out.len&&!out.data)||out.len>c.bytes.size()||out.len%8){
  if(out.release&&out.data)out.release(out.data,out.len);
  throw Error("plugin output shape/ownership invalid");
 }
 Bytes bytes;
 if(out.len)bytes.assign(out.data,out.data+out.len);
 out.release(out.data,out.len);
 return {decode_values(bytes),0};
}
static Result run_any(const Case& c,const std::string& algorithm){if(algorithm.rfind("plugin:",0)==0)return run_plugin(c,algorithm.substr(7));return execute(c,algorithm);}
static Json measure_one(const Case& c,const std::string& algorithm,int block,int warmup,int order_position,std::uint64_t order_seed,int sample_ms,int minimum_batch_ms){
 Json v;v.put("algorithm",algorithm);v.put("block",static_cast<std::int64_t>(block));v.put("status","ok");
 v.put("order_position",static_cast<std::int64_t>(order_position));
 {std::ostringstream h;h<<std::hex<<order_seed;v.put("order_seed",h.str());}
 try{
  const auto expected=oracle(c).values;
  auto trial=run_any(c,algorithm);
  if(trial.values!=expected){v.put("status","wrong_answer");v.put("detail","candidate disagrees with independent oracle");v.put("digest",sha256(encode_values(trial.values)));v.put("work",static_cast<std::int64_t>(trial.work));v.put("ns",static_cast<std::int64_t>(0));v.put("batch",static_cast<std::int64_t>(0));return v;}
  for(int i=0;i<warmup;i++){
   auto w=run_any(c,algorithm);
   if(w.values!=expected){v.put("status","wrong_answer");v.put("detail","warmup output disagrees with oracle");return v;}
   observable_sink=observable_sink^w.work;
  }
  /* Retain each independently executed output until after the clock stops.
     Per-invocation oracle validation therefore cannot pollute the measured
     algorithm latency, and a bad *intermediate* iteration cannot be hidden
     by a correct final iteration. Output retention uses a strict 16 MiB cap;
     unlike discarding each result, this can alter allocator/cache behavior,
     so the protocol records the actual batch size. */
  const auto output_bytes=expected.size()*sizeof(std::int64_t)+sizeof(Result);
  const std::uint64_t max_iterations=std::max<std::uint64_t>(1,
    std::min<std::uint64_t>(2048,16ULL*1024*1024/std::max<std::size_t>(output_bytes,1)));
  std::uint64_t iterations=1,ns=0;Result last;
  struct rusage before{},after{};int cpu_valid=0;
  for(;;){
   std::vector<Result> completed;completed.reserve(static_cast<std::size_t>(iterations));
   const bool cpu_before=getrusage(RUSAGE_SELF,&before)==0;
   const auto start=std::chrono::steady_clock::now();
   for(std::uint64_t i=0;i<iterations;i++)completed.push_back(run_any(c,algorithm));
   const auto stop=std::chrono::steady_clock::now();
   const bool cpu_after=cpu_before&&getrusage(RUSAGE_SELF,&after)==0;
   for(const auto& observed:completed){
    observable_sink=observable_sink^observed.work;
    if(observed.values!=expected){v.put("status","wrong_answer");v.put("detail","one or more timed-batch outputs disagree with the oracle; timings excluded");return v;}
   }
   last=std::move(completed.back());
   cpu_valid=cpu_after;
   ns=static_cast<std::uint64_t>(std::max<std::int64_t>(1,std::chrono::duration_cast<std::chrono::nanoseconds>(stop-start).count()));
   if(ns>static_cast<std::uint64_t>(sample_ms)*1000000ULL){
   v.put("status","timeout");v.put("detail","measured operation exceeded max_sample_wall_ms; censored, not a valid timing");return v;
  }
  if(ns>=static_cast<std::uint64_t>(minimum_batch_ms)*1000000ULL||iterations>=max_iterations||ns>500000000)break;
   iterations=std::min<std::uint64_t>(max_iterations,iterations*2);
  }
  if(cpu_valid){
   const auto before_us=static_cast<std::int64_t>(before.ru_utime.tv_sec)*1000000+before.ru_utime.tv_usec+
                        static_cast<std::int64_t>(before.ru_stime.tv_sec)*1000000+before.ru_stime.tv_usec;
   const auto after_us=static_cast<std::int64_t>(after.ru_utime.tv_sec)*1000000+after.ru_utime.tv_usec+
                       static_cast<std::int64_t>(after.ru_stime.tv_sec)*1000000+after.ru_stime.tv_usec;
   v.put("cpu_ns",(after_us-before_us)*1000);
   v.put("peak_rss_bytes",static_cast<std::int64_t>(after.ru_maxrss)*1024); /* Linux ru_maxrss in KiB */
  }
  v.put("ns",static_cast<std::int64_t>(ns));v.put("batch",static_cast<std::int64_t>(iterations));v.put("work",static_cast<std::int64_t>(trial.work));v.put("digest",sha256(encode_values(last.values)));
 }catch(const std::bad_alloc&){v.put("status","out_of_memory");v.put("detail","std::bad_alloc");}
 catch(const std::exception& e){const auto message=std::string(e.what());v.put("status",message.find("plugin invalid input")!=std::string::npos?"invalid_input":"infrastructure_error");v.put("detail",message.substr(0,400));}
 return v;
}
Json worker_measure(const Json& request,const Bytes& bytes){
 if(request.i("protocol")!=1)throw Error("unsupported IPC protocol version");
 auto params=request.s("params");auto family=request.s("family");
 if(bytes.size()>64ULL*1024*1024||request.i("bytes_length")!=static_cast<std::int64_t>(bytes.size())||request.s("bytes_sha256")!=sha256(bytes))throw Error("invalid worker payload length/checksum");
 auto c=decode_case(family,params,static_cast<std::uint64_t>(request.i("seed")),bytes);
 auto* a=request.get("algorithms");if(!a||json_object_get_type(a)!=json_type_array||json_object_array_length(a)==0||json_object_array_length(a)>2)throw Error("invalid algorithm list");
 std::vector<std::string> names;for(std::size_t i=0;i<json_object_array_length(a);i++){auto* e=json_object_array_get_idx(a,i);if(json_object_get_type(e)!=json_type_string)throw Error("algorithm name must be string");names.emplace_back(json_object_get_string(e));}
 const int blocks=static_cast<int>(request.i("blocks",9)),warmup=static_cast<int>(request.i("warmup",2));
 const int sample_ms=static_cast<int>(request.i("sample_ms",3000));
 const int minimum_batch_ms=static_cast<int>(request.i("minimum_batch_ms",2));
 if(blocks<1||blocks>100||warmup<0||warmup>100||sample_ms<100||sample_ms>60000||minimum_batch_ms<1||minimum_batch_ms>100)throw Error("invalid measurement bounds");
 Json answer;auto* list=json_object_new_array();
 for(int i=0;i<blocks;i++){
  const std::uint64_t order_seed=c.seed^static_cast<std::uint64_t>(0xbdb8244b79c7ULL)^
      (static_cast<std::uint64_t>(i)*static_cast<std::uint64_t>(0x9e3779b97f4a7c15ULL));
  std::vector<std::string> order=names;
  if(names.size()==2){
   auto rand_state=order_seed;
   if((splitmix64(rand_state)&1ULL)==0)order={names[0],names[1],names[1],names[0]};
   else order={names[1],names[0],names[0],names[1]};
  }
  for(std::size_t position=0;position<order.size();position++){
   auto v=measure_one(c,order[position],i,warmup,static_cast<int>(position),order_seed,sample_ms,minimum_batch_ms);
   json_object_array_add(list,v.p);v.p=nullptr;
  }
 }
 json_object_object_add(answer.p,"observations",list);return answer;
}
/* Versioned, length-prefixed binary IPC: each frame uses PS01 + little-endian
   u32 length + byte payload. A fixed deadline applies to entire read/write,
   not to each arriving byte (which would admit slow-drip hangs). */
static constexpr std::uint8_t magic[4]={'P','S','0','1'};
static auto deadline_ms(int ms){return std::chrono::steady_clock::now()+std::chrono::milliseconds(ms);}
/* RLIMIT_AS is a virtual-address-space guard, NOT an RSS limit. The parent
   also samples Linux VmRSS throughout the child's response wait, including
   sanitizer runs where RLIMIT_AS cannot be used. Sampling is best effort:
   a short spike may escape detection between checks. */
static std::uint64_t worker_resident_kib(pid_t pid){
 std::ifstream status("/proc/"+std::to_string(pid)+"/status");
 std::string line;
 while(std::getline(status,line))if(line.rfind("VmRSS:",0)==0){
  std::istringstream in(line.substr(6));std::uint64_t kib=0;
  if(in>>kib)return kib;
 }
 return 0;
}
static bool io_exact(int fd,std::uint8_t* b,std::size_t n,bool writing,std::chrono::steady_clock::time_point deadline,
 pid_t rss_pid=-1,std::uint64_t rss_limit_kib=0,bool* over_rss=nullptr){
 while(n){
  if(rss_pid>0&&rss_limit_kib>0&&worker_resident_kib(rss_pid)>rss_limit_kib){
   if(over_rss)*over_rss=true;
   return false;
  }
  auto remain=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
  if(remain<=0)return false;
  struct pollfd p{fd,static_cast<short>(writing?POLLOUT:POLLIN),0};
  auto ready=poll(&p,1,static_cast<int>(std::min<std::int64_t>(remain,rss_pid>0?25:60000)));
  if(ready<0&&errno==EINTR)continue;
  if(ready==0&&rss_pid>0)continue;
  if(ready<=0||p.revents&(POLLERR|POLLNVAL))return false;
  auto count=writing ? ::write(fd,b,n) : ::read(fd,b,n);
  if(count<0&&(errno==EINTR||errno==EAGAIN))continue;
  if(count<=0)return false;
  b+=count;n-=static_cast<std::size_t>(count);
 }
 return true;
}
static void write_frame(int fd,const std::uint8_t* b,std::size_t n,std::chrono::steady_clock::time_point deadline){
 if(n>64ULL*1024*1024)throw Error("oversized IPC request");
 std::uint8_t h[8]={'P','S','0','1',0,0,0,0};
 for(int i=0;i<4;i++)h[4+i]=static_cast<std::uint8_t>((n>>(i*8))&255);
 if(!io_exact(fd,h,8,true,deadline) || (n&&!io_exact(fd,const_cast<std::uint8_t*>(b),n,true,deadline)))throw Error("IPC write timeout/crash");
}
static void write_frame(int fd,const std::string& s,std::chrono::steady_clock::time_point deadline){write_frame(fd,reinterpret_cast<const std::uint8_t*>(s.data()),s.size(),deadline);}
static Bytes read_frame(int fd,int timeout_ms,std::size_t limit,
 std::chrono::steady_clock::time_point hard_deadline=std::chrono::steady_clock::time_point::max(),
 pid_t rss_pid=-1,std::uint64_t rss_limit_kib=0){
 const auto until=std::min(deadline_ms(timeout_ms),hard_deadline);std::uint8_t h[8];
 bool over_rss=false;
 if(!io_exact(fd,h,8,false,until,rss_pid,rss_limit_kib,&over_rss))
  throw Error(over_rss?"worker RSS exceeded max_worker_rss_mb":"worker response timeout/crash");
 if(!std::equal(magic,magic+4,h))throw Error("malformed IPC version/magic");
 std::uint32_t n=0;for(int i=0;i<4;i++)n|=static_cast<std::uint32_t>(h[4+i])<<(i*8);
 if(n>limit)throw Error("oversized IPC frame");
 Bytes buf(n);if(n&&!io_exact(fd,buf.data(),n,false,until,rss_pid,rss_limit_kib,&over_rss))
  throw Error(over_rss?"worker RSS exceeded max_worker_rss_mb":"truncated worker frame");
 return buf;
}
std::vector<Observation> measure(const Case& c,const std::vector<std::string>& names,int blocks,int warmup,int timeout_ms,int memory_mb,int minimum_batch_ms,bool spawn_per_sample,
 std::chrono::steady_clock::time_point hard_deadline){
 if(std::chrono::steady_clock::now()>=hard_deadline)throw Error("total_wall_seconds budget exhausted before worker start");
 if(spawn_per_sample){
  if(names.empty()||names.size()>2||blocks<1||blocks>100)throw Error("invalid per-sample isolation bounds");
  std::vector<Observation> all;all.reserve(static_cast<std::size_t>(blocks)*(names.size()==2?4:1));
  for(int block=0;block<blocks;block++){
   const std::uint64_t order_seed=c.seed^0xbdb8244b79c7ULL^(static_cast<std::uint64_t>(block)*0x9e3779b97f4a7c15ULL);
   auto rng=order_seed;std::vector<std::string> order=names;
   if(names.size()==2){
    if((splitmix64(rng)&1ULL)==0)order={names[0],names[1],names[1],names[0]};
    else order={names[1],names[0],names[0],names[1]};
   }
   for(std::size_t position=0;position<order.size();position++){
    /* New child for *each* sample; no native plugin state survives a sample.
       Spawn and IPC overhead remain outside the timed operation. */
    auto result=measure(c,{order[position]},1,warmup,timeout_ms,memory_mb,minimum_batch_ms,false,hard_deadline);
    if(result.size()!=1)throw Error("per-sample worker did not return one observation");
    auto& value=result.front();value.block=block;value.order_position=static_cast<std::int64_t>(position);
    std::ostringstream hex;hex<<std::hex<<order_seed;value.order_seed=hex.str();
    all.push_back(std::move(value));
   }
  }
  return all;
 }
 if(names.empty()||names.size()>2||blocks<1||blocks>100||timeout_ms<100||timeout_ms>60000||memory_mb<64||memory_mb>65536||minimum_batch_ms<1||minimum_batch_ms>100)throw Error("invalid runner limits");
 /* Do not exec /proc/self/exe: library consumers/tests may not implement
    --worker, causing recursive fork chains. Only a dedicated worker binary
    with the same source build is eligible to execute native plugins. */
 const auto worker=std::filesystem::read_symlink("/proc/self/exe").parent_path()/"phaseshift_worker";
 if(::access(worker.c_str(),X_OK)!=0)throw Error("dedicated phaseshift_worker binary absent or not executable alongside caller");
 const auto worker_path=worker.string();
 int in[2],out[2];if(pipe(in)!=0)throw Error("worker input pipe failed");if(pipe(out)!=0){close(in[0]);close(in[1]);throw Error("worker output pipe failed");}
 const pid_t pid=fork();if(pid<0){close(in[0]);close(in[1]);close(out[0]);close(out[1]);throw Error("fork failed");}
 if(pid==0){
  close(in[1]);close(out[0]);setsid();dup2(in[0],STDIN_FILENO);dup2(out[1],STDOUT_FILENO);close(in[0]);close(out[1]);
  #ifndef PHASESHIFT_ASAN_BUILD
  struct rlimit mem{static_cast<rlim_t>(memory_mb)*1024*1024,static_cast<rlim_t>(memory_mb)*1024*1024};setrlimit(RLIMIT_AS,&mem);
#else
  (void)memory_mb; /* ASan reserves terabytes of virtual shadow address space. */
#endif
  struct rlimit no_core{0,0};setrlimit(RLIMIT_CORE,&no_core);
  const auto max_operations=static_cast<std::int64_t>(blocks)*static_cast<std::int64_t>(names.size()==2?4:1);
  const auto cpu_budget=std::min<std::int64_t>(3600,1+(max_operations*timeout_ms+999)/1000);
  struct rlimit cpu{static_cast<rlim_t>(cpu_budget),static_cast<rlim_t>(std::min<std::int64_t>(3600,cpu_budget+2))};
  setrlimit(RLIMIT_CPU,&cpu);
  execl(worker_path.c_str(),"phaseshift_worker","--worker",static_cast<char*>(nullptr));_exit(127);
 }
 close(in[0]);close(out[1]);
 /* Writing to a pipe must never block indefinitely after the worker exits. */
 const int flags=fcntl(in[1],F_GETFL,0);if(flags>=0)fcntl(in[1],F_SETFL,flags|O_NONBLOCK);
 std::vector<Observation> result;
 try{
  Json req;req.put("protocol",static_cast<std::int64_t>(1));req.put("family",c.family);req.put("params",case_params(c));req.put("seed",static_cast<std::int64_t>(c.seed));req.put("bytes_length",static_cast<std::int64_t>(c.bytes.size()));req.put("bytes_sha256",sha256(c.bytes));req.put("blocks",static_cast<std::int64_t>(blocks));req.put("warmup",static_cast<std::int64_t>(warmup));req.put("sample_ms",static_cast<std::int64_t>(timeout_ms));req.put("minimum_batch_ms",static_cast<std::int64_t>(minimum_batch_ms));
  auto* arr=json_object_new_array();for(const auto& name:names)json_object_array_add(arr,json_object_new_string(name.c_str()));json_object_object_add(req.p,"algorithms",arr);
  /* Allow one per-sample deadline per operation, plus bounded IPC transfer. */
  const auto operation_count=static_cast<std::int64_t>(blocks)*static_cast<std::int64_t>(names.size()==2?4:1);
  const auto deadline=static_cast<int>(std::min<std::int64_t>(3600000,operation_count*timeout_ms+1000));
  const auto until=std::min(deadline_ms(deadline),hard_deadline);
  write_frame(in[1],req.str(),until);
  write_frame(in[1],c.bytes.data(),c.bytes.size(),until);close(in[1]);in[1]=-1;
  auto raw=read_frame(out[0],deadline,8ULL*1024*1024,hard_deadline,pid,static_cast<std::uint64_t>(memory_mb)*1024ULL);
  const auto response=Json::parse(std::string(raw.begin(),raw.end()));auto* list=response.get("observations");
  if(!list||json_object_get_type(list)!=json_type_array)throw Error("malformed worker response");
  for(std::size_t i=0;i<json_object_array_length(list);i++){
   Json v(json_object_get(json_object_array_get_idx(list,i)));Observation s;
   s.algorithm=v.s("algorithm");s.status=v.s("status","infrastructure_error");s.digest=v.s("digest");s.detail=v.s("detail");s.ns=v.i("ns");s.work=v.i("work");s.batch=v.i("batch",1);s.block=v.i("block");
   s.cpu_ns=v.i("cpu_ns",-1);s.peak_rss_bytes=v.i("peak_rss_bytes",-1);
   s.order_position=v.i("order_position",-1);s.order_seed=v.s("order_seed");
   result.push_back(s);
  }
  if(result.empty())throw Error("empty worker response");
 }catch(const std::exception& e){
  result.clear();for(const auto& n:names){Observation s;s.algorithm=n;s.detail=e.what();s.status="infrastructure_error";result.push_back(s);}
 }
 if(in[1]>=0)close(in[1]);
 close(out[0]);int status=0;
 pid_t reaped=waitpid(pid,&status,WNOHANG);
 // A failed read may observe pipe EOF before SIGCHLD becomes waitable.
 // Reap a genuinely crashed worker before deciding it timed out; a single
 // WNOHANG check can race the kernel's final exit notification and mislabel
 // SIGSEGV as an infrastructure failure. Never block indefinitely on hangs.
 if(reaped==0){
  const auto reap_until=std::min(std::chrono::steady_clock::now()+std::chrono::milliseconds(150),hard_deadline);
  while(reaped==0&&std::chrono::steady_clock::now()<reap_until){
   (void)poll(nullptr,0,5);
   reaped=waitpid(pid,&status,WNOHANG);
   if(reaped<0&&errno==EINTR)reaped=0;
  }
 }
 if(reaped==0){
  for(auto& obs:result)if(obs.status=="infrastructure_error"&&obs.detail.find("timeout")!=std::string::npos)obs.status="timeout";
  kill(-pid,SIGKILL);kill(pid,SIGKILL);while((reaped=waitpid(pid,&status,0))<0&&errno==EINTR){}
  reaped=-1; /* Parent-enforced kill is not a worker crash. */
 }
 else if(reaped<0&&errno==EINTR){while((reaped=waitpid(pid,&status,0))<0&&errno==EINTR){}}
 for(auto& obs:result)if(obs.status=="infrastructure_error"&&
   obs.detail.find("worker RSS exceeded max_worker_rss_mb")!=std::string::npos)
   obs.status="out_of_memory";
 if(reaped==pid&&WIFEXITED(status)&&WEXITSTATUS(status)!=0){
  for(auto& obs:result)if(obs.status=="infrastructure_error"){
   obs.status="crashed";obs.detail+="; worker nonzero exit "+std::to_string(WEXITSTATUS(status));
  }
 }
 if(reaped==pid&&WIFSIGNALED(status)){
  for(auto& obs:result)if(obs.status=="infrastructure_error"||obs.status=="crashed"){
   obs.status="signaled";obs.detail+="; worker signal "+std::to_string(WTERMSIG(status));
  }
 }
 if(std::chrono::steady_clock::now()>=hard_deadline)throw Error("total_wall_seconds budget exhausted during worker execution");
 return result;
}
int worker_entry(){
 try{
  auto json=read_frame(STDIN_FILENO,30000,1ULL*1024*1024);
  auto bytes=read_frame(STDIN_FILENO,30000,64ULL*1024*1024);
  auto answer=worker_measure(Json::parse(std::string(json.begin(),json.end())),bytes);
  write_frame(STDOUT_FILENO,answer.str(),deadline_ms(30000));return 0;
 }catch(const std::exception&){return 5;}
}
}
namespace ps {int worker_entry();}
