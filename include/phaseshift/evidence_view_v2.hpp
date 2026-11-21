#pragma once
#include "phaseshift/runner.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ps {
// Read-only after append, commit-boundary discovery evidence for v2 policies.
// Never accepts holdout rows: callers must supply only Store observations with
// holdout=false (or the exact newly committed discovery batch). No raw rows
// are discarded or reinterpreted by this derived cache.
struct DiscoverySummaryV2 {
 std::optional<double> metric;
 double observed_cpu_ms=1.;
 std::uint64_t valid_samples=0,failed_samples=0,missing_cpu=0;
 std::int64_t peak_rss_bytes=-1;
};
class DiscoveryEvidenceViewV2 {
 std::map<std::string,DiscoverySummaryV2> by_case_;
 std::size_t capacity_;
public:
 explicit DiscoveryEvidenceViewV2(std::size_t max_cases):capacity_(max_cases){
  if(capacity_==0||capacity_>20000)
   throw Error("invalid bounded v2 discovery view capacity");
 }
 void append_committed(const std::string& case_id,
  const std::vector<Observation>& samples,std::optional<double> metric){
  if(case_id.empty()||samples.empty()||by_case_.size()>=capacity_||
     by_case_.count(case_id)||(metric&&!std::isfinite(*metric)))
   throw Error("invalid or duplicate committed v2 discovery evidence");
  DiscoverySummaryV2 summary;
  long double cpu_ns=0.;
  for(const auto& o:samples){
   if(o.status=="ok")++summary.valid_samples;
   else ++summary.failed_samples;
   if(o.status=="ok"&&o.cpu_ns>=0)
    cpu_ns+=static_cast<long double>(o.cpu_ns);
   else ++summary.missing_cpu;
   if(o.peak_rss_bytes>=0)
    summary.peak_rss_bytes=std::max(summary.peak_rss_bytes,o.peak_rss_bytes);
  }
  if(!std::isfinite(cpu_ns)||cpu_ns>static_cast<long double>(std::numeric_limits<std::int64_t>::max()))
   throw Error("v2 discovery CPU sum cannot be represented");
  summary.observed_cpu_ms=std::max(1.,static_cast<double>(cpu_ns/1000000.L));
  summary.metric=metric;
  by_case_.emplace(case_id,summary);
 }
 const DiscoverySummaryV2& lookup(const std::string& case_id)const{
  const auto it=by_case_.find(case_id);
  if(it==by_case_.end())throw Error("v2 discovery cache miss before commit");
  return it->second;
 }
 std::size_t size()const noexcept{return by_case_.size();}
};
} // namespace ps
