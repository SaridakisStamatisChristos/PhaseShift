#pragma once
#include "phaseshift/algorithms.hpp"
namespace ps {
struct Observation {
 std::string status="infrastructure_error",algorithm,digest,detail,order_seed;
 std::int64_t ns=0,work=0,batch=1,block=0,cpu_ns=-1,peak_rss_bytes=-1,order_position=-1;
};
std::vector<Observation> measure(const Case& c,const std::vector<std::string>& names,int blocks,int warmup,int timeout_ms,int memory_mb,int minimum_batch_ms=2,bool spawn_per_sample=false);
Json worker_measure(const Json& request,const Bytes& payload);
}
