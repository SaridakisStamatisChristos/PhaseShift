#pragma once
// Discovery-only, committed evidence snapshots; never a mutable holdout view.
#include "phaseshift/v3/types.hpp"
#include <bit>
#include <map>
#include <string>
#include <cstdint>

namespace ps::v3 {
class DiscoveryEvidenceSnapshot {
 std::map<CandidateKey,ObservationSummary> committed_;
 std::uint64_t source_row_watermark_=0;
 std::size_t capacity_;
 static void append_int(std::string& s,std::uint64_t value) {
  for(unsigned shift=0;shift<64;shift+=8)
   s.push_back(static_cast<char>((value>>shift)&255U));
 }
 static void append_string(std::string& s,const std::string& value) {
  append_int(s,static_cast<std::uint64_t>(value.size()));
  s+=value;
 }
 static void append_metric(std::string& s,std::optional<double> value) {
  s.push_back(value?'Y':'N');
  if(value) {
   const auto canonical=*value==0.0?0.0:*value;
   append_int(s,std::bit_cast<std::uint64_t>(canonical));
  }
 }
public:
 explicit DiscoveryEvidenceSnapshot(std::size_t capacity):capacity_(capacity) {
  if(!capacity || capacity>20000)
   throw Error("v3 bounded evidence snapshot capacity invalid");
 }
 void append_committed(const ObservationSummary& row,Cohort cohort,
                       std::uint64_t committed_watermark) {
  if(cohort!=Cohort::Discovery)
   throw Error("v3 sealed confirmation cannot enter discovery snapshot");
  validate_observation(row);
  if(committed_watermark<=source_row_watermark_ ||
     committed_.size()>=capacity_ || committed_.count(row.key))
   throw Error("v3 stale/duplicate committed discovery evidence");
  committed_.emplace(row.key,row);
  source_row_watermark_=committed_watermark;
 }
 const ObservationSummary* find(const CandidateKey& key)const noexcept {
  const auto it=committed_.find(key);
  return it==committed_.end()?nullptr:&it->second;
 }
 std::uint64_t source_watermark()const noexcept{return source_row_watermark_;}
 std::size_t size()const noexcept{return committed_.size();}
 std::string canonical_digest()const {
  // Length-prefix every field to avoid ambiguity; fixed little endian u64.
  std::string bytes="phaseshift.v3.discovery.snapshot.1";
  append_int(bytes,source_row_watermark_);
  append_int(bytes,static_cast<std::uint64_t>(committed_.size()));
  for(const auto& [key,row]:committed_) {
   append_int(bytes,key.structure.generator_version);
   append_int(bytes,static_cast<std::uint64_t>(key.structure.coordinates.size()));
   for(const auto& [axis,value]:key.structure.coordinates) {
    append_string(bytes,axis);append_string(bytes,value);
   }
   append_int(bytes,key.replicate.seed);
   append_int(bytes,key.replicate.replicate_index);
   append_string(bytes,row.status);
   append_metric(bytes,row.paired_log_ratio);
   append_metric(bytes,row.robust_dispersion);
   append_int(bytes,row.measured_child_cpu_ns);
   append_int(bytes,row.parent_cpu_ns);
   append_int(bytes,row.total_wall_ns);
   append_string(bytes,row.provenance_digest);
  }
  return ps::sha256(bytes);
 }
};
} // namespace ps::v3
