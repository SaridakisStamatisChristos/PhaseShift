#include "phaseshift/interaction_v2.hpp"
#include "phaseshift/statistics.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
namespace ps {
InteractionEvidenceV2 screen_factorial_v2(
 const std::array<std::string,2>& a,const std::array<std::string,2>& b,
 const std::vector<FactorialCellV2>& observations,
 std::size_t min_cohorts,double min_effect){
 if(a[0].empty()||a[1].empty()||b[0].empty()||b[1].empty()||
    a[0]==a[1]||b[0]==b[1]||min_cohorts<3||min_cohorts>10000||
    !std::isfinite(min_effect)||min_effect<=0||observations.size()>100000)
  throw Error("invalid v2 factorial design or evidence bounds");
 using Row=std::optional<FactorialCellV2>;
 std::map<std::uint64_t,std::array<Row,4>> cohorts;
 for(const auto& row:observations){
  const auto ai=row.first==a[0]?0:row.first==a[1]?1:-1;
  const auto bi=row.second==b[0]?0:row.second==b[1]?1:-1;
  if(ai<0||bi<0)throw Error("v2 observation outside declared nominal factorial design");
  if(row.status.empty()||(row.status=="ok"&&(!row.log_metric||!std::isfinite(*row.log_metric)))||
     (row.status!="ok"&&row.log_metric))
   throw Error("v2 factorial metric/status contract invalid");
  auto& slot=cohorts[row.replicate_seed][static_cast<std::size_t>(ai*2+bi)];
  if(slot)throw Error("duplicate v2 factorial seed/cell evidence");
  slot=row;
 }
 InteractionEvidenceV2 result;
 std::vector<double> contrasts;
 std::size_t conditional_status=0,ambiguous=0;
 std::string common_failure;
 for(const auto& [seed,cells]:cohorts){
  (void)seed;
  if(std::any_of(cells.begin(),cells.end(),[](const Row& cell){return !cell.has_value();})){
   ++result.incomplete_cohorts;
   continue;
  }
  ++result.complete_cohorts;
  const bool all_ok=std::all_of(cells.begin(),cells.end(),[](const Row& cell){return cell->status=="ok";});
  if(all_ok){
   const auto did=(*cells[3]->log_metric-*cells[2]->log_metric)-
                  (*cells[1]->log_metric-*cells[0]->log_metric);
   if(!std::isfinite(did))throw Error("v2 factorial contrast is not finite");
   contrasts.push_back(did);
  }else{
   const auto& failure=cells[3]->status;
   const bool supported=failure=="wrong_answer"||failure=="crashed"||failure=="signaled"||
                        failure=="timeout"||failure=="out_of_memory";
   if(supported&&cells[0]->status=="ok"&&cells[1]->status=="ok"&&cells[2]->status=="ok"&&
      (common_failure.empty()||common_failure==failure)){
    common_failure=failure;++conditional_status;
   }else ++ambiguous;
  }
 }
 if(result.incomplete_cohorts||result.complete_cohorts<min_cohorts)return result;
 if(conditional_status==result.complete_cohorts&&ambiguous==0){
  result.kind="CONDITIONAL_STATUS_CANDIDATE";
  result.failure_status=common_failure;
  return result;
 }
 if(contrasts.size()!=result.complete_cohorts||ambiguous||conditional_status)return result;
 result.median_contrast=sample_median(contrasts);
 if(std::abs(result.median_contrast)<min_effect){
  result.kind="NO_INTERACTION_CANDIDATE";return result;
 }
 std::size_t same_sign=0;
 for(const auto contrast:contrasts)
  if((result.median_contrast>0&&contrast>0)||(result.median_contrast<0&&contrast<0))++same_sign;
 if(same_sign*5<contrasts.size()*4){
  result.kind="UNSTABLE_OR_AMBIGUOUS";return result;
 }
 result.kind="CONDITIONAL_PERFORMANCE_CANDIDATE";
 return result;
}

FixedFactorialHoldoutV2 fixed_factorial_holdout_v2(
 const std::array<std::string,2>& a,const std::array<std::string,2>& b,
 const std::vector<FactorialCellV2>& holdout,
 const std::vector<std::uint64_t>& discovery_seeds,
 std::size_t frozen_n,int direction,double threshold,double alpha){
 if(a[0].empty()||a[1].empty()||a[0]==a[1]||
    b[0].empty()||b[1].empty()||b[0]==b[1]||
    frozen_n<3||frozen_n>256||(direction!=1&&direction!=-1)||
    !std::isfinite(threshold)||threshold<=0||
    !std::isfinite(alpha)||alpha<=0||alpha>0.25||
    holdout.size()>frozen_n*4)
  throw Error("invalid frozen v2 factorial holdout design");
 const std::set<std::uint64_t> used(discovery_seeds.begin(),discovery_seeds.end());
 if(used.size()!=discovery_seeds.size())
  throw Error("duplicate v2 discovery seed in holdout manifest");
 using Cell=std::optional<FactorialCellV2>;
 std::map<std::uint64_t,std::array<Cell,4>> cohorts;
 for(const auto& row:holdout){
  if(used.count(row.replicate_seed))
   throw Error("v2 factorial holdout overlaps discovery seed stream");
  const auto ai=row.first==a[0]?0:row.first==a[1]?1:-1;
  const auto bi=row.second==b[0]?0:row.second==b[1]?1:-1;
  if(ai<0||bi<0||row.status.empty()||
     (row.status=="ok"&&(!row.log_metric||!std::isfinite(*row.log_metric)))||
     (row.status!="ok"&&row.log_metric))
   throw Error("invalid v2 factorial holdout cell");
  auto& slot=cohorts[row.replicate_seed][static_cast<std::size_t>(ai*2+bi)];
  if(slot)throw Error("duplicate v2 factorial holdout cell");
  slot=row;
 }
 FixedFactorialHoldoutV2 result;
 if(cohorts.size()!=frozen_n||holdout.size()!=4*frozen_n)return result;
 std::vector<double> effects;
 for(const auto& [seed,cells]:cohorts){
  (void)seed;
  for(const auto& cell:cells)if(!cell)return result;
  for(const auto& cell:cells)if(cell->status!="ok"){
   result.state="INVALID_OR_CENSORED_HOLDOUT";
   return result;
  }
  const auto did=(*cells[3]->log_metric-*cells[2]->log_metric)-
                 (*cells[1]->log_metric-*cells[0]->log_metric);
  if(!std::isfinite(did))throw Error("nonfinite v2 holdout interaction contrast");
  effects.push_back(static_cast<double>(direction)*did);
  if(static_cast<double>(direction)*did>0)++result.consistent_sign_cohorts;
 }
 result.tested_cohorts=frozen_n;
 result.median_signed_effect=sample_median(effects);
 // Exact Binomial(n,1/2) upper tail: valid only for independent,
 // sign-symmetric, fixed-size cohort contrasts. Ties count against signal.
 long double term=std::ldexp(1.L,-static_cast<int>(frozen_n));
 long double p=0;
 for(std::size_t k=0;k<=frozen_n;++k){
  if(k>=result.consistent_sign_cohorts)p+=term;
  if(k<frozen_n)
   term*=static_cast<long double>(frozen_n-k)/
         static_cast<long double>(k+1);
 }
 result.unadjusted_one_sided_p=std::min(1.,static_cast<double>(p));
 result.supports_predeclared_effect=
   result.median_signed_effect>=threshold&&
   result.unadjusted_one_sided_p<=alpha;
 result.state=result.supports_predeclared_effect?
   "FROZEN_HOLDOUT_SUPPORT_REQUIRES_CAMPAIGN_SEAL":
   "FROZEN_HOLDOUT_NO_SUPPORT";
 return result;
}
}
