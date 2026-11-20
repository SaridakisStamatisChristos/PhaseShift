#pragma once
#include "phaseshift/algorithms.hpp"
namespace ps {
struct Observation {std::string status="infrastructure_error",algorithm,digest,detail;std::int64_t ns=0,work=0,batch=1,block=0;};
std::vector<Observation> measure(const Case& c,const std::vector<std::string>& names,int blocks,int warmup,int timeout_ms,int memory_mb);
Json worker_measure(const Json& request,const Bytes& payload);
}
