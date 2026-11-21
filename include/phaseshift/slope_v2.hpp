#pragma once
#include "phaseshift/core.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace ps {
struct SlopeSegmentV2 {
 std::size_t begin=0,end=0;
 double slope=0;
};
struct MultiSlopeFitV2 {
 std::vector<std::size_t> changes;
 std::vector<SlopeSegmentV2> segments;
 double residual_sse=0,penalized_sse=0;
};
// Independent exploratory piecewise-linear segmentation over the ACTUAL
// ordered numeric structural coordinates (not ordinal sample ranks).
// O(K*N^2) time, O(K*N) memory; N<=512, K<=8, min segment >=3.
// Score is penalized least-squares, not a significance p-value or causal proof.
inline MultiSlopeFitV2 fit_multi_slope_v2(std::span<const std::int64_t> x,
 std::span<const double> y,std::size_t minimum,std::size_t max_changes,double penalty){
 const auto n=y.size();
 if(n<3||n>512||x.size()!=n||minimum<3||minimum>n||
    max_changes>8||!std::isfinite(penalty)||penalty<0)
  throw Error("invalid v2 slope fit bounds");
 for(std::size_t i=0;i<n;++i){
  if(!std::isfinite(y[i])||(i&&x[i]<=x[i-1]))
   throw Error("nonfinite observation or unordered v2 slope axis");
 }
 const auto x_anchor=static_cast<long double>(x[0]);
 const auto y_anchor=static_cast<long double>(y[0]);
 std::vector<long double> sx(n+1),sy(n+1),sxx(n+1),syy(n+1),sxy(n+1);
 for(std::size_t i=0;i<n;++i){
  const auto vx=static_cast<long double>(x[i])-x_anchor;
  const auto vy=static_cast<long double>(y[i])-y_anchor;
  sx[i+1]=sx[i]+vx;sy[i+1]=sy[i]+vy;
  sxx[i+1]=sxx[i]+vx*vx;syy[i+1]=syy[i]+vy*vy;
  sxy[i+1]=sxy[i]+vx*vy;
  if(!std::isfinite(sx[i+1])||!std::isfinite(sy[i+1])||
     !std::isfinite(sxx[i+1])||!std::isfinite(syy[i+1])||
     !std::isfinite(sxy[i+1]))
   throw Error("v2 slope numeric accumulation overflow");
 }
 const auto moments=[&](std::size_t a,std::size_t b){
  const auto m=static_cast<long double>(b-a);
  const auto dx=sx[b]-sx[a],dy=sy[b]-sy[a];
  const auto xx=std::max(0.0L,(sxx[b]-sxx[a])-dx*dx/m);
  const auto yy=std::max(0.0L,(syy[b]-syy[a])-dy*dy/m);
  const auto xy=(sxy[b]-sxy[a])-dx*dy/m;
  if(xx<=0||!std::isfinite(xx)||!std::isfinite(yy)||!std::isfinite(xy))
   throw Error("degenerate v2 slope variance");
  return std::pair<long double,long double>{xy/xx,
   std::max(0.0L,yy-xy*xy/xx)};
 };
 const auto k_max=std::min(max_changes+1,n/minimum);
 const auto inf=std::numeric_limits<long double>::infinity();
 std::vector<std::vector<long double>> dp(k_max+1,std::vector<long double>(n+1,inf));
 std::vector<std::vector<std::size_t>> back(k_max+1,std::vector<std::size_t>(n+1));
 dp[0][0]=0;
 for(std::size_t k=1;k<=k_max;++k){
  for(std::size_t b=k*minimum;b<=n;++b){
   for(std::size_t a=(k-1)*minimum;a+minimum<=b;++a){
    if(!std::isfinite(dp[k-1][a]))continue;
    const auto score=dp[k-1][a]+moments(a,b).second;
    if(score<dp[k][b]){dp[k][b]=score;back[k][b]=a;}
   }
  }
 }
 std::size_t best_k=1;
 long double best=dp[1][n];
 for(std::size_t k=2;k<=k_max;++k){
  const auto candidate=dp[k][n]+static_cast<long double>(k-1)*penalty;
  if(candidate<best){best=candidate;best_k=k;}
 }
 if(!std::isfinite(best)||best>std::numeric_limits<double>::max()||
    dp[best_k][n]>std::numeric_limits<double>::max())
  throw Error("v2 slope objective out of range");
 MultiSlopeFitV2 fit;
 fit.residual_sse=static_cast<double>(dp[best_k][n]);
 fit.penalized_sse=static_cast<double>(best);
 std::size_t b=n;
 for(std::size_t k=best_k;k>0;--k){
  const auto a=back[k][b];
  const auto slope=moments(a,b).first;
  if(!std::isfinite(slope)||std::abs(slope)>std::numeric_limits<double>::max())
   throw Error("v2 slope estimate out of range");
  fit.segments.push_back({a,b,static_cast<double>(slope)});
  if(a)fit.changes.push_back(a);
  b=a;
 }
 std::reverse(fit.changes.begin(),fit.changes.end());
 std::reverse(fit.segments.begin(),fit.segments.end());
 return fit;
}
} // namespace ps
