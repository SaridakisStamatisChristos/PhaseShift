#pragma once
// V3-07 discovery-only shape comparison. Integrated matched contrasts are
// descriptive finite-range profiles, not independent level observations.
#include "phaseshift/boundary_v2.hpp"
#include "phaseshift/v3/types.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ps::v3 {
struct ShapeCutV3 {
 std::size_t left=0,right=0;
 double signed_jump=0.;
};
struct ShapeFitV3 {
 std::string kind="UNSTABLE_OR_INCONCLUSIVE",rival="unavailable";
 // All six entries below use SSE + declared model penalty (log-unit squared).
 // Heterogeneity is reported separately, not a comparable likelihood.
 std::map<std::string,double> scores;
 std::vector<ShapeCutV3> cuts;
 double residual_variance_ratio=1.;
};
inline double shape_median_v3(std::vector<double> a){
 if(a.empty())throw Error("v3 empty shape median");
 std::sort(a.begin(),a.end());
 const auto m=a.size()/2;
 return a.size()%2?a[m]:a[m-1]*.5+a[m]*.5;
}
// Fit against actual ordered numeric coordinates (not category indices).
inline double line_loss_v3(std::span<const double> x,std::span<const double> y){
 const auto n=x.size();
 if(n<2||n!=y.size())throw Error("v3 invalid shape line segment");
 long double sx=0.,sy=0.;
 for(std::size_t i=0;i<n;++i){sx+=x[i];sy+=y[i];}
 sx/=static_cast<long double>(n);sy/=static_cast<long double>(n);
 long double xx=0.,xy=0.;
 for(std::size_t i=0;i<n;++i){
  const auto dx=static_cast<long double>(x[i])-sx;
  xx+=dx*dx;xy+=dx*(static_cast<long double>(y[i])-sy);
 }
 const auto slope=xx>0?xy/xx:0.;
 long double loss=0.;
 for(std::size_t i=0;i<n;++i){
  const auto e=static_cast<long double>(y[i])-sy-slope*(static_cast<long double>(x[i])-sx);
  loss+=e*e;
 }
 return static_cast<double>(loss);
}
inline ShapeFitV3 classify_shape_v3(std::span<const double> x,std::span<const double> y,
                                  double min_effect,bool allow_scaling=false){
 if(x.size()!=y.size()||y.size()>256||!std::isfinite(min_effect)||
    min_effect<1e-6||min_effect>100.)
  throw Error("v3 invalid shape domain/threshold");
 for(std::size_t i=0;i<y.size();++i)
  if(!std::isfinite(x[i])||!std::isfinite(y[i])||std::abs(x[i])>1e15||
     std::abs(y[i])>1e6||(i&&x[i]<=x[i-1]))
   throw Error("v3 shape needs finite strictly ordered numerical coordinates");
 ShapeFitV3 result;
 const auto n=y.size();
 if(n<6)return result;
 const double penalty=2.*min_effect*min_effect;
 const auto constant=fit_multi_change_v2(y,3,0,penalty);
 const auto step=fit_multi_change_v2(y,3,1,penalty);
 const auto multi=fit_multi_change_pelt_v2(y,3,8,penalty);
 result.scores["constant"]=constant.residual_sse;
 result.scores["smooth_linear"]=line_loss_v3(x,y)+penalty;
 result.scores["step"]=step.penalized_sse;
 result.scores["multi_step"]=multi.penalized_sse;
 // Piecewise linear rival allows separate intercept and slope in each half.
 double piece=std::numeric_limits<double>::infinity();
 for(std::size_t cut=3;cut+3<=n;++cut)
  piece=std::min(piece,line_loss_v3(x.first(cut),y.first(cut))+
                       line_loss_v3(x.subspan(cut),y.subspan(cut))+3.*penalty);
 result.scores["piecewise_linear"]=piece;
 // Exact finite single-impulse rival: fit constant after deleting each bin.
 double impulse=std::numeric_limits<double>::infinity();
 for(std::size_t skip=0;skip<n;++skip){
  std::vector<double> rest;
  for(std::size_t i=0;i<n;++i)if(i!=skip)rest.push_back(y[i]);
  impulse=std::min(impulse,fit_multi_change_v2(rest,1,0,penalty).residual_sse+2.*penalty);
 }
 result.scores["localized_outlier"]=impulse;
 // Variance diagnostic, conditional on the constant mean. This is not a
 // calibrated variance test nor compared against the penalized mean models.
 const auto mean=constant.segments.front().mean;
 double l=0.,r=0.;
 for(std::size_t i=0;i<n;++i){
  const auto e=y[i]-mean;
  (i<n/2?l:r)+=e*e;
 }
 l/=static_cast<double>(n/2);r/=static_cast<double>(n-n/2);
 result.residual_variance_ratio=(std::max(l,r)+min_effect*min_effect)/
                                (std::min(l,r)+min_effect*min_effect);
 // Conservative tie order prioritizes simpler rival shapes.
 const std::array<std::string,6> names={"constant","smooth_linear","localized_outlier",
                                       "piecewise_linear","step","multi_step"};
 result.rival=names.front();
 for(const auto& name:names)
  if(result.scores.at(name)<result.scores.at(result.rival)-1e-12)
   result.rival=name;
 const auto [low,high]=std::minmax_element(y.begin(),y.end());
 if(result.rival=="constant"&&*high-*low<min_effect){
  result.kind="NO_CONFIRMED_BOUNDARY";return result;
 }
 if(result.rival=="smooth_linear") {
  result.kind="SMOOTH_TRANSITION";return result;
 }
 if(result.rival=="piecewise_linear"){
  result.kind=allow_scaling?"SCALING_KNEE":"SMOOTH_TRANSITION";
  return result;
 }
 if(result.rival=="step"||result.rival=="multi_step"){
  const auto& fit=result.rival=="step"?step:multi;
  for(const auto cut:fit.changes){
   if(screen_persistent_jump_v2(y,cut,min_effect))
    result.cuts.push_back({cut-1,cut,y[cut]-y[cut-1]});
  }
  if(!result.cuts.empty()&&result.cuts.size()==fit.changes.size())
   result.kind=result.cuts.size()>1?"MULTI_CLIFF":"ABRUPT_CLIFF";
  else result.cuts.clear(); // abstain from a partially unsupported partition
 }
 return result;
}
struct MatchedBoundaryObservationV3 {
 std::vector<AxisValue> context;
 std::size_t left=0,right=0; // indices in declared ordered primary axis
 std::uint64_t seed=0;
 std::optional<double> left_metric,right_metric;
 std::string left_status,right_status;
};
struct BoundaryCandidateV3 {
 std::vector<AxisValue> context,other_context;
 std::string kind,detail;
 std::size_t left=0,right=0,support=0;
 std::optional<double> effect;
};
struct SliceShapeV3 {
 std::vector<AxisValue> context;
 std::size_t begin=0,end=0;
 ShapeFitV3 shape;
};
struct TaxonomyReportV3 {
 std::vector<BoundaryCandidateV3> candidates;
 std::vector<SliceShapeV3> profiles;
 std::size_t observations=0,missing_or_unsupported_edges=0;
};
// Distinguish repeated matched effects from endpoint-wise independent samples.
// Context equality is exact; no ordinal interpretation of nominal labels.
inline TaxonomyReportV3 classify_matched_v3(
 const std::vector<double>& axis,const std::vector<MatchedBoundaryObservationV3>& observations,
 double min_effect,bool relative_metric,Cohort cohort= Cohort::Discovery){
 if(cohort!=Cohort::Discovery)throw Error("v3 taxonomy rejects confirmation");
 if(axis.size()<2||axis.size()>4096||observations.size()>100||
    !std::isfinite(min_effect)||min_effect<1e-6||min_effect>100.)
  throw Error("v3 taxonomy bounds");
 for(std::size_t i=0;i<axis.size();++i)
  if(!std::isfinite(axis[i])||std::abs(axis[i])>1e15||(i&&axis[i]<=axis[i-1]))
   throw Error("v3 malformed taxonomy axis");
 using Context=std::vector<AxisValue>;
 using Edge=std::pair<std::size_t,std::size_t>;
 struct Group {std::vector<double> effects;std::vector<std::pair<double,double>> levels;
               std::map<std::pair<std::string,std::string>,std::size_t> statuses;
               std::size_t count=0;};
 std::map<Context,std::map<Edge,Group>> slices;
 std::set<std::uint64_t> seeds;
 TaxonomyReportV3 out;out.observations=observations.size();
 for(const auto& o:observations){
  if(o.left>=o.right||o.right>=axis.size()||!seeds.insert(o.seed).second||
     o.left_status.empty()||o.right_status.empty()||
     (o.left_status=="ok")!=o.left_metric.has_value()||
     (o.right_status=="ok")!=o.right_metric.has_value())
   throw Error("v3 malformed or duplicate matched observation");
  for(auto v:{o.left_metric,o.right_metric})
   if(v&&(!std::isfinite(*v)||std::abs(*v)>1e4))
    throw Error("v3 metric out of range");
  std::set<std::string> keys;
  for(const auto& [k,v]:o.context)
   if(k.empty()||v.empty()||!keys.insert(k).second)
    throw Error("v3 malformed conditional context");
  auto& g=slices[o.context][{o.left,o.right}];++g.count;
  ++g.statuses[{o.left_status,o.right_status}];
  if(o.left_metric&&o.right_metric){
   g.effects.push_back(*o.right_metric-*o.left_metric);
   g.levels.emplace_back(*o.left_metric,*o.right_metric);
  }
 }
 // A supported status contrast requires repeated identical outcomes.
 const auto failure=[](const std::string& s){
  return s=="wrong_answer"||s=="crashed"||s=="signaled"||
         s=="timeout"||s=="out_of_memory";
 };
 struct Stable {Context context;Edge edge;double effect;std::size_t support;};
 std::vector<Stable> stable;
 for(const auto& [context,edges]:slices){
  std::map<std::size_t,std::pair<double,std::size_t>> adjacent;
  for(const auto& [edge,g]:edges){
   if(g.statuses.size()==1&&g.count>=2){
    const auto& st=g.statuses.begin()->first;
    if((st.first=="ok"&&failure(st.second))||(st.second=="ok"&&failure(st.first)))
     out.candidates.push_back({context,{},"STATUS_FRONTIER",st.first+" -> "+st.second,
                               edge.first,edge.second,g.count,std::nullopt});
   }
   if(g.effects.size()!=g.count||g.effects.size()<2){
    ++out.missing_or_unsupported_edges;continue;
   }
   const auto center=shape_median_v3(g.effects);
   std::vector<double> deviations;
   for(auto v:g.effects)deviations.push_back(std::abs(v-center));
   if(shape_median_v3(deviations)>min_effect*.5){
    ++out.missing_or_unsupported_edges;continue;
   }
   stable.push_back({context,edge,center,g.count});
   if(edge.second==edge.first+1)adjacent.emplace(edge.first,std::pair{center,g.count});
   else ++out.missing_or_unsupported_edges; // no fabricated missing intermediate edge
   if(relative_metric){
    std::size_t up=0,down=0;
    for(const auto& [l,r]:g.levels){
     if(l<=-min_effect&&r>=min_effect)++up;
     if(l>=min_effect&&r<=-min_effect)++down;
    }
    if(up==g.count||down==g.count)
     out.candidates.push_back({context,{},"CROSSOVER","matched sign reversal",
                              edge.first,edge.second,g.count,center});
   }
  }
  // Split into contiguous observed edge chains. Each chain has a separate
  // arbitrary zero intercept; never bridge unmeasured or censored edges.
  for(auto it=adjacent.begin();it!=adjacent.end();){
   const auto begin=it->first;
   auto end=begin;
   std::vector<double> x{axis[begin]},y{0.};
   std::size_t support=std::numeric_limits<std::size_t>::max();
   while(it!=adjacent.end()&&it->first==end){
    y.push_back(y.back()+it->second.first);
    x.push_back(axis[end+1]);
    support=std::min(support,it->second.second);
    ++it;++end;
   }
   const auto fit=classify_shape_v3(x,y,min_effect,false);
   out.profiles.push_back({context,begin,end,fit});
   if(!fit.cuts.empty()){
    for(const auto& cut:fit.cuts)
     out.candidates.push_back({context,{},fit.kind,"descriptive matched-edge profile",
                              begin+cut.left,begin+cut.right,support,cut.signed_jump});
   }else if(fit.kind!="UNSTABLE_OR_INCONCLUSIVE"&&fit.kind!="NO_CONFIRMED_BOUNDARY"){
    out.candidates.push_back({context,{},fit.kind,"descriptive profile rival",
                             begin,end,support,std::nullopt});
   }
  }
 }
 // Conditional nomination only: differences of matched edge medians across
 // contexts differing in ONE explicit label; seeds are not shared across
 // contexts in v12-v15, so no factorial/cohort significance is implied.
 for(std::size_t i=0;i<stable.size();++i)for(std::size_t j=i+1;j<stable.size();++j){
  const auto& a=stable[i];const auto& b=stable[j];
  if(a.edge!=b.edge||a.context.size()!=b.context.size())continue;
  std::size_t changed=0;
  bool names=true;
  for(std::size_t k=0;k<a.context.size();++k){
   names=names&&a.context[k].first==b.context[k].first;
   changed+=a.context[k].second!=b.context[k].second?1U:0U;
  }
  if(names&&changed==1&&std::abs(b.effect-a.effect)>=min_effect&&
     (std::abs(a.effect)>=min_effect||std::abs(b.effect)>=min_effect))
   out.candidates.push_back({a.context,b.context,"CONDITIONAL_INTERACTION",
        "unpaired-across-context difference of matched medians; nomination only",
        a.edge.first,a.edge.second,std::min(a.support,b.support),b.effect-a.effect});
 }
 return out;
}
} // namespace ps::v3
