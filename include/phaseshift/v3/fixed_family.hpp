#pragma once
// V3-08 fixed family inference. This does not establish the sampling
// assumptions: the caller must explicitly declare an independent sign model.
#include "phaseshift/v3/types.hpp"
#include "phaseshift/statistics.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
#include <string>
#include <vector>
namespace ps::v3 {
struct FrozenEdgeMemberV3 {
 std::size_t discovery_pair=0;
 int direction=0;
};
struct FixedFamilyMemberV3 {
 std::size_t valid=0,positive=0;
 double raw_p=1.,holm_p=1.;
 std::string state="INCOMPLETE";
};
struct FixedFamilyResultV3 {
 bool complete=false;
 std::vector<FixedFamilyMemberV3> members;
};
inline double exact_upper_sign_tail_v3(std::size_t n,std::size_t successes){
 if(n<1||n>100||successes>n)throw Error("v3 invalid exact sign count");
 // n<=100, long double binomial recurrence; no Monte Carlo or pseudocount.
 long double term=std::ldexp(1.L,-static_cast<int>(n)),sum=0.;
 for(std::size_t k=0;k<=n;++k){
  if(k>=successes)sum+=term;
  if(k<n)term*=static_cast<long double>(n-k)/static_cast<long double>(k+1);
 }
 return std::min(1.,static_cast<double>(sum));
}
inline FixedFamilyResultV3 fixed_family_v3(
 const std::vector<FrozenEdgeMemberV3>& members,
 const std::vector<std::vector<std::optional<double>>>& observations,
 std::size_t planned,double minimum,double alpha,bool sign_assumptions){
 if(members.empty()||members.size()>16||members.size()!=observations.size()||
    planned<20||planned>100||!std::isfinite(minimum)||minimum<=0.||
    !std::isfinite(alpha)||alpha<.001||alpha>.25)
  throw Error("v3 invalid fixed family contract");
 std::set<std::size_t> seen;
 FixedFamilyResultV3 out;out.complete=true;out.members.resize(members.size());
 for(std::size_t i=0;i<members.size();++i){
  const auto& m=members[i];const auto& rows=observations[i];
  if(!seen.insert(m.discovery_pair).second||(m.direction!=1&&m.direction!=-1)||
     rows.size()>planned)throw Error("v3 malformed fixed family member");
  auto& result=out.members[i];
  if(rows.size()!=planned)out.complete=false;
  for(const auto effect:rows)if(effect){
   if(!std::isfinite(*effect))throw Error("v3 nonfinite holdout effect");
   ++result.valid;
   // Ties count against the alternative. Under the declared null each
   // independent seed has P(direction*effect>minimum)<=1/2.
   if(static_cast<double>(m.direction)*(*effect)>minimum)++result.positive;
  }
 }
 // Never publish interim p-values or decisions for a partially filled family.
 if(!out.complete)return out;
 std::vector<double> p(members.size(),1.);
 for(std::size_t i=0;i<members.size();++i){
  auto& r=out.members[i];
  if(r.valid!=planned)r.state="ABSTAIN_CENSORED";
  else if(!sign_assumptions)r.state="ABSTAIN_ASSUMPTIONS";
  else{
   r.state="NOT_REJECTED";
   p[i]=exact_upper_sign_tail_v3(planned,r.positive);
   r.raw_p=p[i];
  }
 }
 const auto reject=holm_fwer(p,alpha);
 std::vector<std::size_t> indices(p.size());std::iota(indices.begin(),indices.end(),0U);
 std::stable_sort(indices.begin(),indices.end(),[&](auto a,auto b){return p[a]<p[b];});
 double running=0.;
 for(std::size_t rank=0;rank<indices.size();++rank){
  const auto i=indices[rank];
  running=std::max(running,p[i]*static_cast<double>(indices.size()-rank));
  auto& r=out.members[i];r.holm_p=std::min(1.,running);
  if(reject[i]&&r.state=="NOT_REJECTED")r.state="REJECTED_UNDER_DECLARED_MODEL";
 }
 return out;
}
} // namespace ps::v3
