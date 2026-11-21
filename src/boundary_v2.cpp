#include "phaseshift/boundary_v2.hpp"
#include "phaseshift/core.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
namespace ps {
MultiChangeFitV2 fit_multi_change_v2(std::span<const double> values,
 std::size_t min_segment_size,std::size_t max_changes,double penalty){
 const auto n=values.size();
 if(n==0||n>4096||min_segment_size==0||min_segment_size>n||max_changes>32||
    !std::isfinite(penalty)||penalty<0)throw Error("invalid v2 changepoint fit bounds");
 for(const double value:values)if(!std::isfinite(value))throw Error("nonfinite v2 changepoint observation");
 const auto segments=std::min(max_changes+1,n/min_segment_size);
 std::vector<long double> sum(n+1,0),squares(n+1,0);
 const auto anchor=static_cast<long double>(values.front());
 for(std::size_t i=0;i<n;++i){
  const auto centered=static_cast<long double>(values[i])-anchor;
  sum[i+1]=sum[i]+centered;
  squares[i+1]=squares[i]+centered*centered;
  if(!std::isfinite(sum[i+1])||!std::isfinite(squares[i+1]))throw Error("changepoint numeric range overflow");
 }
 const auto cost=[&](std::size_t left,std::size_t right){
  const auto count=static_cast<long double>(right-left);
  const auto s=sum[right]-sum[left];
  const auto ss=squares[right]-squares[left];
  return std::max(0.0L,ss-s*s/count);
 };
 const auto inf=std::numeric_limits<long double>::infinity();
 std::vector<std::vector<long double>> dp(segments+1,std::vector<long double>(n+1,inf));
 std::vector<std::vector<std::size_t>> back(segments+1,std::vector<std::size_t>(n+1,0));
 dp[0][0]=0;
 for(std::size_t k=1;k<=segments;++k){
  for(std::size_t right=k*min_segment_size;right<=n;++right){
   for(std::size_t left=(k-1)*min_segment_size;left+min_segment_size<=right;++left){
    if(!std::isfinite(dp[k-1][left]))continue;
    const auto candidate=dp[k-1][left]+cost(left,right);
    if(candidate<dp[k][right]){dp[k][right]=candidate;back[k][right]=left;}
   }
  }
 }
 std::size_t best_k=1;auto best=dp[1][n];
 for(std::size_t k=2;k<=segments;++k){
  const auto candidate=dp[k][n]+static_cast<long double>(k-1)*static_cast<long double>(penalty);
  if(candidate<best){best=candidate;best_k=k;}
 }
 if(!std::isfinite(best)||!std::isfinite(dp[best_k][n])||
    best>static_cast<long double>(std::numeric_limits<double>::max())||
    dp[best_k][n]>static_cast<long double>(std::numeric_limits<double>::max()))
  throw Error("changepoint score cannot be represented");
 MultiChangeFitV2 fit;fit.penalized_sse=static_cast<double>(best);
 fit.residual_sse=static_cast<double>(dp[best_k][n]);
 std::size_t right=n;
 for(std::size_t k=best_k;k>0;--k){
  const auto left=back[k][right];
  const auto mean=anchor+(sum[right]-sum[left])/static_cast<long double>(right-left);
  if(!std::isfinite(mean)||std::abs(mean)>static_cast<long double>(std::numeric_limits<double>::max()))
   throw Error("changepoint mean cannot be represented");
  fit.segments.push_back({left,right,static_cast<double>(mean)});
  if(left)fit.changes.push_back(left);
  right=left;
 }
 std::reverse(fit.segments.begin(),fit.segments.end());
 std::reverse(fit.changes.begin(),fit.changes.end());
 return fit;
}

MultiChangeFitV2 fit_multi_change_pelt_v2(std::span<const double> y,
 std::size_t minimum,std::size_t max_changes,double penalty){
 const std::size_t n=y.size();
 if(n==0||n>4096||minimum==0||minimum>n||max_changes>32||
    !std::isfinite(penalty)||penalty<0)throw Error("invalid PELT v2 bounds");
 for(const auto v:y)if(!std::isfinite(v))throw Error("nonfinite PELT v2 observation");
 if(max_changes==0)return fit_multi_change_v2(y,minimum,0,penalty);
 const auto anchor=static_cast<long double>(y.front());
 std::vector<long double> sum(n+1,0),sum2(n+1,0);
 for(std::size_t i=0;i<n;++i){
  const auto value=static_cast<long double>(y[i])-anchor;
  sum[i+1]=sum[i]+value;sum2[i+1]=sum2[i]+value*value;
  if(!std::isfinite(sum[i+1])||!std::isfinite(sum2[i+1]))
   throw Error("PELT v2 accumulation overflow");
 }
 const auto loss=[&](std::size_t left,std::size_t right){
  const auto length=static_cast<long double>(right-left);
  const auto total=sum[right]-sum[left];
  return std::max(0.0L,sum2[right]-sum2[left]-total*total/length);
 };
 const auto infinite=std::numeric_limits<long double>::infinity();
 std::vector<long double> optimal(n+1,infinite);
 std::vector<std::size_t> previous(n+1,0),prunable_from(n+1,n+1),candidates;
 optimal[0]=-static_cast<long double>(penalty);
 for(std::size_t right=minimum;right<=n;++right){
  const auto newly_eligible=right-minimum;
  if(std::isfinite(optimal[newly_eligible]))candidates.push_back(newly_eligible);
  candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[&](auto index){
   return prunable_from[index]<=right;
  }),candidates.end());
  auto best=infinite;std::size_t best_left=n+1;
  for(const auto left:candidates){
   const auto candidate=optimal[left]+loss(left,right)+static_cast<long double>(penalty);
   if(candidate<best){best=candidate;best_left=left;}
  }
  if(best_left==n+1||!std::isfinite(best))throw Error("PELT v2 has no feasible partition");
  optimal[right]=best;previous[right]=best_left;
  if(right+minimum<=n){
   const auto tolerance=1.e-13L*std::max(1.0L,std::abs(best));
   for(const auto left:candidates){
    // If a cut at right dominates 'left', its replacement is feasible
    // only at right+minimum. Earlier pruning would be incorrect.
    if(optimal[left]+loss(left,right)>best+tolerance)
     prunable_from[left]=std::min(prunable_from[left],right+minimum);
   }
  }
 }
 std::vector<std::size_t> boundaries;
 for(std::size_t end=n;end>0;){
  const auto start=previous[end];
  if(start>=end||end-start<minimum)throw Error("invalid pruned PELT segment");
  if(start)boundaries.push_back(start);
  end=start;
 }
 if(boundaries.size()>max_changes)
  return fit_multi_change_v2(y,minimum,max_changes,penalty);
 std::reverse(boundaries.begin(),boundaries.end());
 MultiChangeFitV2 fit;fit.changes=boundaries;
 long double residual=0;
 std::size_t begin=0;
 for(std::size_t i=0;i<=boundaries.size();++i){
  const auto end=i<boundaries.size()?boundaries[i]:n;
  const auto mean=anchor+(sum[end]-sum[begin])/static_cast<long double>(end-begin);
  if(!std::isfinite(mean)||std::abs(mean)>static_cast<long double>(std::numeric_limits<double>::max()))
   throw Error("PELT v2 mean overflow");
  fit.segments.push_back({begin,end,static_cast<double>(mean)});
  residual+=loss(begin,end);
  begin=end;
 }
 const auto objective=residual+static_cast<long double>(boundaries.size())*static_cast<long double>(penalty);
 if(!std::isfinite(objective)||objective>static_cast<long double>(std::numeric_limits<double>::max()))
  throw Error("PELT v2 score overflow");
 fit.residual_sse=static_cast<double>(residual);
 fit.penalized_sse=static_cast<double>(objective);
 return fit;
}
}

namespace ps {
// Robust local-shape diagnostic; screens raw exploratory segmentation only.
// Three-point medians reject a one-bin impulse. A locally prominent step
// distinguishes abrupt jumps from a broad, approximately smooth trend.
// Positive screening is NOT statistical confirmation: frozen holdout remains
// mandatory before a hypothesis may be labelled CONFIRMED.
std::optional<PersistentJumpEvidenceV2> screen_persistent_jump_v2(
 std::span<const double> values,std::size_t cut,double minimum_log_effect){
 if(!std::isfinite(minimum_log_effect)||minimum_log_effect<=0)
  throw Error("invalid v2 persistent-jump effect threshold");
 if(cut==0||cut>=values.size())
  throw Error("invalid v2 persistent-jump cut index");
 for(const double y:values)
  if(!std::isfinite(y))throw Error("nonfinite v2 persistent-jump observation");
 if(cut<3||values.size()-cut<3)return std::nullopt;
 const auto median_three=[](double a,double b,double c){
  std::array<double,3> data{a,b,c};
  std::sort(data.begin(),data.end());
  return data[1];
 };
 const double before=median_three(values[cut-3],values[cut-2],values[cut-1]);
 const double after=median_three(values[cut],values[cut+1],values[cut+2]);
 const double persistence=after-before;
 const double edge=values[cut]-values[cut-1];
 if(!std::isfinite(persistence)||!std::isfinite(edge))
  throw Error("persistent-jump difference cannot be represented");
 std::vector<double> background;
 background.reserve(values.size()-2);
 for(std::size_t i=1;i<values.size();++i)if(i!=cut){
  const double delta=std::abs(values[i]-values[i-1]);
  if(!std::isfinite(delta))throw Error("persistent-jump gradient overflow");
  background.push_back(delta);
 }
 std::sort(background.begin(),background.end());
 // The upper quartile is deliberately more conservative than the median:
 // an isolated large noise gradient must not define a structural step.
 // Use integer arithmetic bounded by N<=4096 in the fit that calls us.
 const double noise_floor=background[(3*(background.size()-1)+3)/4];
 if(!std::isfinite(noise_floor))throw Error("persistent-jump background overflow");
 const double magnitude=std::abs(persistence);
 // Strict inequalities reject a continuous trend with no discrete jump.
 if(!(persistence*edge>0.)||!(magnitude>=minimum_log_effect)||
    !(magnitude>=0.5*std::abs(edge))||
    !(std::abs(edge)>=minimum_log_effect)||
    !(std::abs(edge)>3.*noise_floor))
  return std::nullopt;
 return PersistentJumpEvidenceV2{magnitude,std::abs(edge),noise_floor};
}
} // namespace ps
