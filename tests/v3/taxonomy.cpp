#include "phaseshift/v3/taxonomy.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>
#define CHECK(x) do{if(!(x))throw std::runtime_error("taxonomy falsifier: " #x);}while(false)
using namespace ps;
using namespace ps::v3;
template<class F>void rejects(F&& fn){
 bool rejected=false;try{fn();}catch(const Error&){rejected=true;}CHECK(rejected);
}
double direct_sse(const std::vector<double>& y,std::size_t a,std::size_t b){
 double mean=0.;for(std::size_t i=a;i<b;++i)mean+=y[i];
 mean/=static_cast<double>(b-a);double total=0.;
 for(std::size_t i=a;i<b;++i){double d=y[i]-mean;total+=d*d;}
 return total;
}
// Independent tiny exact oracle: enumerate all cut bitmasks, not DP/PELT.
double exhaustive(const std::vector<double>& y,std::size_t max_changes,double penalty){
 double best=std::numeric_limits<double>::infinity();
 for(unsigned mask=0;mask<(1U<<(y.size()-1));++mask){
  std::size_t begin=0,cuts=0;double cost=0.;bool legal=true;
  for(std::size_t k=1;k<y.size();++k)if(mask&(1U<<(k-1))){
   if(k-begin<3){legal=false;break;}
   cost+=direct_sse(y,begin,k)+penalty;begin=k;++cuts;
  }
  if(legal&&cuts<=max_changes&&y.size()-begin>=3)
   best=std::min(best,cost+direct_sse(y,begin,y.size()));
 }
 return best;
}
std::vector<MatchedBoundaryObservationV3> observations(const std::vector<double>& y,
 const std::string& label,std::uint64_t& seed){
 std::vector<MatchedBoundaryObservationV3> out;
 for(std::size_t i=0;i+1<y.size();++i)for(int r=0;r<2;++r)
  out.push_back({{{"mode",label}},i,i+1,seed++,y[i],y[i+1],"ok","ok"});
 return out;
}
int main(){
 std::vector<double> x(15);std::iota(x.begin(),x.end(),0.);
 const std::vector<double> y{0,0,0,0,0,2,2,2,2,2,-1,-1,-1,-1,-1};
 const auto multi=classify_shape_v3(x,y,.2);
 CHECK(multi.kind=="MULTI_CLIFF"&&multi.cuts.size()==2);
 CHECK(multi.cuts[0].left==4&&multi.cuts[0].right==5);
 CHECK(multi.cuts[1].left==9&&multi.cuts[1].right==10);
 CHECK(multi.cuts[0].signed_jump==2.&&multi.cuts[1].signed_jump==-3.);
 std::vector<double> step(15,0.);std::fill(step.begin()+6,step.end(),2.);
 CHECK(classify_shape_v3(x,step,.2).kind=="ABRUPT_CLIFF");
 const std::vector<double> constant(15,0.2);
 CHECK(classify_shape_v3(x,constant,.2).kind=="NO_CONFIRMED_BOUNDARY");
 auto smooth=x;for(auto& v:smooth)v*=.4;
 CHECK(classify_shape_v3(x,smooth,.2).kind=="SMOOTH_TRANSITION");
 auto irregular=x;for(auto& v:irregular)v=v*v+v;
 auto nonuniform=irregular;for(auto& v:nonuniform)v*=.03;
 CHECK(classify_shape_v3(irregular,nonuniform,.2).kind=="SMOOTH_TRANSITION");
 auto bent=x;for(auto& v:bent)v=v<7?.02*v:.14+.9*(v-7);
 CHECK(classify_shape_v3(x,bent,.2,true).kind=="SCALING_KNEE");
 auto impulse=constant;impulse[7]=12.;
 const auto outlier=classify_shape_v3(x,impulse,.2);
 CHECK(outlier.kind=="UNSTABLE_OR_INCONCLUSIVE"&&outlier.rival=="localized_outlier");
 CHECK(outlier.cuts.empty());
 CHECK(classify_shape_v3(std::span(x).first(5),std::span(y).first(5),.2).scores.empty());
 auto malformed=x;malformed[3]=malformed[2];
 rejects([&]{(void)classify_shape_v3(malformed,y,.2);});
 auto nonfinite=y;nonfinite[2]=std::numeric_limits<double>::infinity();
 rejects([&]{(void)classify_shape_v3(x,nonfinite,.2);});
 rejects([&]{(void)classify_shape_v3(x,y,0.);});
 // Exhaustive tiny binary profiles; independently compare both optimal
 // constant-segment rival objectives without assuming the preferred shape.
 std::vector<double> small_x(8);std::iota(small_x.begin(),small_x.end(),0.);
 for(unsigned value=0;value<256;++value){
  std::vector<double> small(8);
  for(std::size_t i=0;i<8;++i)small[i]=(value&(1U<<i))?1.:0.;
  const auto shape=classify_shape_v3(small_x,small,.2);
  CHECK(std::abs(shape.scores.at("step")-exhaustive(small,1,.08))<1e-10);
  CHECK(std::abs(shape.scores.at("multi_step")-exhaustive(small,8,.08))<1e-10);
 }
 std::uint64_t seed=1;
 auto raw=observations(y,"zeta",seed);
 const auto report=classify_matched_v3(x,raw,.2,false);
 CHECK(report.profiles.size()==1&&report.profiles[0].shape.kind=="MULTI_CLIFF");
 CHECK(report.candidates.size()==2&&report.candidates[0].left==4&&report.candidates[1].left==9);
 CHECK(report.candidates[0].support==2);
 // Remove both copies of an edge: no interpolation across missing bins.
 auto sparse=raw;
 sparse.erase(std::remove_if(sparse.begin(),sparse.end(),[](const auto& r){return r.left==7;}),sparse.end());
 const auto disconnected=classify_matched_v3(x,sparse,.2,false);
 CHECK(disconnected.profiles.size()==2);
 CHECK(disconnected.profiles[0].end==7&&disconnected.profiles[1].begin==8);
 auto bridge=raw.front();bridge.right=3;bridge.seed=seed++;
 auto other_bridge=bridge;other_bridge.seed=seed++;
 const auto no_bridge=classify_matched_v3(x,{bridge,other_bridge},.2,false);
 CHECK(no_bridge.profiles.empty()&&no_bridge.missing_or_unsupported_edges==1);
 // Nominal interaction: alternate context has opposing matched effects.
 auto opposite=y;for(auto& v:opposite)v=-v;
 auto second=observations(opposite,"alpha",seed);
 raw.insert(raw.end(),second.begin(),second.end());
 const auto conditional=classify_matched_v3(x,raw,.2,false);
 CHECK(conditional.profiles.size()==2);
 const auto interaction_count=std::count_if(conditional.candidates.begin(),conditional.candidates.end(),
  [](const auto& c){return c.kind=="CONDITIONAL_INTERACTION"&&c.context!=c.other_context;});
 CHECK(interaction_count==2);
 // Direct within-seed sign changes and separately censored status edges.
 std::vector<MatchedBoundaryObservationV3> status={
  {{{"mode","beta"}},0,1,seed++,-.5,.7,"ok","ok"},
  {{{"mode","beta"}},0,1,seed++,-.6,.8,"ok","ok"},
  {{{"mode","beta"}},1,2,seed++,0.,std::nullopt,"ok","timeout"},
  {{{"mode","beta"}},1,2,seed++,0.,std::nullopt,"ok","timeout"}
 };
 const auto screens=classify_matched_v3(x,status,.2,true);
 CHECK(screens.candidates.size()==2);
 CHECK(screens.candidates[0].kind=="CROSSOVER"&&screens.candidates[0].effect.has_value());
 CHECK(screens.candidates[1].kind=="STATUS_FRONTIER"&&!screens.candidates[1].effect);
 auto one=status;one.pop_back();
 CHECK(classify_matched_v3(x,one,.2,true).candidates.size()==1);
 auto mixed=status;mixed.back().right_status="crashed";
 CHECK(classify_matched_v3(x,mixed,.2,true).candidates.size()==1);
 auto noisy=status;noisy[1].left_metric=0.9;noisy[1].right_metric=-.9;
 CHECK(classify_matched_v3(x,noisy,.2,true).candidates.size()==1);
 auto duplicate=status;duplicate.back().seed=duplicate.front().seed;
 rejects([&]{(void)classify_matched_v3(x,duplicate,.2,true);});
 auto failed_metric=status;failed_metric.back().right_metric=1.;
 rejects([&]{(void)classify_matched_v3(x,failed_metric,.2,true);});
 rejects([&]{(void)classify_matched_v3(x,status,.2,true,Cohort::Confirmation);});
 CHECK(classify_matched_v3(x,{},.2,false).candidates.empty());
 std::cout<<"PASS v3 discovery shape taxonomy: exact partition oracle, double cliff, smooth, impulse, sparse, nominal, status, crossover\n";
}
