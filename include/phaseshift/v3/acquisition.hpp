#pragma once
// V3-05 empirical cost ranking. No distribution-free coverage or significance
// claim. Costs are complete committed pair measurement-wall and algorithm-region
// CPU, not full process-tree/abandoned-attempt cost.
#include "phaseshift/v3/types.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace ps::v3 {
struct PairMeasuredCost {
 std::int64_t wall_ns=0;
 std::optional<std::int64_t> worker_cpu_ns;
 bool operator==(const PairMeasuredCost&)const=default;
};
struct CostEstimateV3 {
 std::int64_t wall=0,wall_upper=0,cpu=0,cpu_upper=0,pooled_wall=0;
 std::size_t training=0,heldout=0,covered=0,failed=0;
 bool cpu_known=false,enabled=false;
 std::string audit()const{
  return "wall="+std::to_string(wall)+";upper="+std::to_string(wall_upper)+
   ";cpu="+(cpu_known?std::to_string(cpu):"unknown")+
   ";cpu_upper="+(cpu_known?std::to_string(cpu_upper):"unknown")+
   ";pooled_wall="+std::to_string(pooled_wall)+
   ";train="+std::to_string(training)+";heldout="+std::to_string(heldout)+
   ";covered="+std::to_string(covered)+";failed="+std::to_string(failed)+
   ";enabled="+(enabled?"1":"0");
 }
 double penalty()const noexcept{
  if(!enabled||wall<=0||pooled_wall<=0)return 1.;
  return std::sqrt(std::clamp(static_cast<double>(wall)/
                             static_cast<double>(pooled_wall),0.25,4.));
 }
};
class EmpiricalCostModelV3 {
 struct Entry {std::size_t context;PairMeasuredCost cost;bool valid,heldout;};
 std::vector<Entry> entries_;
 std::size_t heldout_=0,covered_=0,failed_=0;
 static std::int64_t median(std::vector<std::int64_t> v){
  if(v.empty())return 0;
  std::sort(v.begin(),v.end());
  return v[(v.size()-1)/2]; // lower median; exact integral arithmetic
 }
 static std::int64_t upper(const std::vector<std::int64_t>& v){
  return v.empty()?0:*std::max_element(v.begin(),v.end());
 }
public:
 CostEstimateV3 estimate(std::size_t context)const{
  std::vector<std::int64_t> wall,local_wall,cpu,local_cpu;
  for(const auto& e:entries_){
   if(!e.valid||e.heldout)continue;
   wall.push_back(e.cost.wall_ns);
   if(e.context==context)local_wall.push_back(e.cost.wall_ns);
   if(e.cost.worker_cpu_ns){
    cpu.push_back(*e.cost.worker_cpu_ns);
    if(e.context==context)local_cpu.push_back(*e.cost.worker_cpu_ns);
   }
  }
  CostEstimateV3 out;
  out.training=wall.size();out.heldout=heldout_;out.covered=covered_;
  out.failed=failed_;out.pooled_wall=median(wall);
  // Cold contexts pool. Local medians only after three independent pairs.
  out.wall=local_wall.size()>=3?median(local_wall):out.pooled_wall;
  out.wall_upper=upper(wall); // exploratory empirical envelope, not a bound
  out.cpu_known=!cpu.empty();
  out.cpu=local_cpu.size()>=3?median(local_cpu):median(cpu);
  out.cpu_upper=upper(cpu);
  // Fixed, small-sample eligibility rule, deliberately pessimistic for failures
  // and distribution shift. This is not statistical calibration certification.
  out.enabled=out.training>=6&&heldout_>=2&&covered_*5>=heldout_*4&&failed_==0;
  return out;
 }
 void add(std::size_t context,Cohort cohort,const PairMeasuredCost& cost,bool valid){
  if(cohort!=Cohort::Discovery)throw Error("v3 cost model rejects confirmation");
  if(entries_.size()>=100||cost.wall_ns<=0||
     (cost.worker_cpu_ns&&*cost.worker_cpu_ns<0))
   throw Error("v3 invalid or over-cap committed pair cost");
  const bool heldout=(entries_.size()+1)%4==0; // frozen prequential split
  if(heldout){
   const auto forecast=estimate(context);
   ++heldout_;
   if(valid&&forecast.wall_upper>0&&cost.wall_ns<=forecast.wall_upper)++covered_;
  }
  if(!valid)++failed_;
  entries_.push_back({context,cost,valid,heldout});
 }
 std::size_t size()const noexcept{return entries_.size();}
 std::string canonical_state()const{
  std::string s="cost.v3.1:"+std::to_string(heldout_)+":"+
   std::to_string(covered_)+":"+std::to_string(failed_)+";";
  for(const auto& e:entries_){
   s+=std::to_string(e.context)+":"+std::to_string(e.cost.wall_ns)+":"+
      (e.cost.worker_cpu_ns?std::to_string(*e.cost.worker_cpu_ns):"unknown")+":"+
      (e.valid?"1":"0")+":"+(e.heldout?"1":"0")+";";
  }
  return s;
 }
};
enum class AcquisitionAblationV3 {Full,NoCost,NoUncertainty,NoReplicate,NoDiversity,NoInteraction};
struct AcquisitionComponentsV3 {
 double contrast=0.,uncertainty=0.,diversity=0.,interaction=0.,status=0.;
 bool replication=false;
};
inline double acquisition_score_v3(const AcquisitionComponentsV3& x,
  const CostEstimateV3& cost,AcquisitionAblationV3 ablation=AcquisitionAblationV3::Full){
 const auto bounded=[](double v){
  if(!std::isfinite(v)||v<0.)throw Error("v3 invalid acquisition component");
  return std::min(1.,v);
 };
 const auto c=bounded(x.contrast),u=bounded(x.uncertainty),d=bounded(x.diversity),
            i=bounded(x.interaction),s=bounded(x.status);
 if(x.replication&&ablation==AcquisitionAblationV3::NoReplicate)return 0.;
 const double value=0.30*c+0.25*(ablation==AcquisitionAblationV3::NoUncertainty?0.:u)+
   0.25*(ablation==AcquisitionAblationV3::NoDiversity?0.:d)+
   0.10*(ablation==AcquisitionAblationV3::NoInteraction?0.:i)+0.10*s;
 return value/(ablation==AcquisitionAblationV3::NoCost?1.:cost.penalty());
}
inline std::string acquisition_audit_v3(const AcquisitionComponentsV3& x,const CostEstimateV3& cost){
 // Store exact IEEE bits for ranking scalars; not locale-dependent decimal text.
 auto bits=[](double v){return std::to_string(std::bit_cast<std::uint64_t>(v));};
 return cost.audit()+";contrast_bits="+bits(x.contrast)+
  ";uncertainty_bits="+bits(x.uncertainty)+";diversity_bits="+bits(x.diversity)+
  ";interaction_bits="+bits(x.interaction)+";status_bits="+bits(x.status)+
  ";rank_bits="+bits(acquisition_score_v3(x,cost));
}
} // namespace ps::v3
