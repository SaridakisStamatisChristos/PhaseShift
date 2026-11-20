#include "phaseshift/boundary_v2.hpp"
#include "phaseshift/core.hpp"
#include <algorithm>
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
}
