#include "phaseshift/scaling.hpp"
#include "phaseshift/statistics.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <array>
#include <limits>
namespace ps {
namespace fs=std::filesystem;
static std::vector<int> scaling_holdout_sizes(const std::vector<std::pair<double,double>>& data,std::size_t cut){
 std::set<int> used;for(const auto& point:data)used.insert(static_cast<int>(point.first));
 std::vector<int> selected;selected.reserve(4);
 for(int side=0;side<2;side++){
  const auto begin=side==0?std::size_t{0}:cut;
  const auto end=side==0?cut:data.size();
  int count=0;
  for(std::size_t i=begin;i+1<end&&count<2;i++){
   const int lo=static_cast<int>(data[i].first),hi=static_cast<int>(data[i+1].first);
   for(int candidate=lo+1;candidate<hi;candidate++){
    if(!used.count(candidate)){
     selected.push_back(candidate);used.insert(candidate);count++;break;
    }
   }
  }
  if(count!=2)return {};
 }
 return selected;
}
static double model_value(int model,double n){
 switch(model){case 0:return 1;case 1:return std::log(n);case 2:return n;case 3:return n*std::log(n);case 4:return n*n;default:return n*n*n;}
}
std::vector<GrowthFit> fit_growth_models(const std::vector<std::pair<double,double>>& data){
 if(data.size()<6)throw Error("scaling fit requires six or more distinct n values");
 std::set<double> unique;
 for(const auto& [n,y]:data)if(!std::isfinite(n)||!std::isfinite(y)||n<2||y<=0||!unique.insert(n).second)throw Error("invalid/duplicate scaling x/y");
 static const char* names[]={"constant","log_n","n","n_log_n","n_squared","n_cubed"};
 std::vector<GrowthFit> fits;
 for(int model=0;model<6;model++){
  std::vector<double> ratios;
  for(const auto& [n,y]:data)ratios.push_back(std::log(y/model_value(model,n)));
  double error=0;
  for(std::size_t i=0;i<data.size();i++){
   std::vector<double> training;training.reserve(data.size()-1);
   for(std::size_t j=0;j<data.size();j++)if(i!=j)training.push_back(ratios[j]);
   error+=std::abs(ratios[i]-sample_median(training));
  }
  fits.push_back({names[model],error/static_cast<double>(data.size()),std::exp(sample_median(ratios))});
 }
 std::stable_sort(fits.begin(),fits.end(),[](const GrowthFit& a,const GrowthFit& b){return a.log_error<b.log_error;});
 return fits;
}
ScalingHoldoutVerdict evaluate_scaling_holdout(const std::vector<double>& d,double discovery_slope_difference,
                                                std::uint64_t seed,double confidence,double q){
 ScalingHoldoutVerdict result;result.independent_seed_pairs=d.size();
 if(d.size()<20)return result;
 if(!std::isfinite(discovery_slope_difference))throw Error("non-finite frozen scaling slope difference");
 const auto ci=paired_case_bootstrap(d,seed^0x19a7e2feULL,confidence);
 const auto p=paired_sign_flip_p(d,seed^0x7d6e9041ULL);
 result.slope_difference=ci.point;result.ci_lower=ci.lower;result.ci_upper=ci.upper;result.p_value=p;
 result.confirmed=p<=q && ((discovery_slope_difference>0&&ci.lower>0.6)||
                            (discovery_slope_difference<0&&ci.upper< -0.6));
 return result;
}
static std::pair<double,double> linear_fit(const std::vector<std::pair<double,double>>& points,std::size_t begin,std::size_t end){
 const auto count=static_cast<double>(end-begin);
 double mx=0.0,my=0.0;
 for(std::size_t i=begin;i<end;i++){mx+=std::log(points[i].first);my+=std::log(points[i].second);}
 mx/=count;my/=count;
 double cov=0.0,var=0.0;
 for(std::size_t i=begin;i<end;i++){
  const auto dx=std::log(points[i].first)-mx;
  cov+=dx*(std::log(points[i].second)-my);var+=dx*dx;
 }
 if(var<=0)throw Error("degenerate scaling design");
 const auto slope=cov/var;
 return {slope,my-slope*mx};
}
static double residual_sse(const std::vector<std::pair<double,double>>& points,std::size_t begin,std::size_t end){
 const auto [slope,intercept]=linear_fit(points,begin,end);
 double sse=0.0;
 for(std::size_t i=begin;i<end;i++){
  const auto residual=std::log(points[i].second)-intercept-slope*std::log(points[i].first);
  sse+=residual*residual;
 }
 return sse;
}
ScalingBoundary detect_scaling_boundary(const std::vector<std::pair<double,double>>& data,double min_slope_shift){
 if(data.size()<8)throw Error("scaling-regime inference requires at least eight distinct sizes");
 if(!std::isfinite(min_slope_shift)||min_slope_shift<0.2||min_slope_shift>10)throw Error("invalid scaling shift threshold");
 for(std::size_t i=0;i<data.size();i++){
  if(!std::isfinite(data[i].first)||!std::isfinite(data[i].second)||data[i].first<2||data[i].second<=0||
     (i>0&&data[i].first<=data[i-1].first))throw Error("scaling inputs must be strictly ordered positive size/runtime pairs");
 }
 const auto total=data.size();const auto null_sse=residual_sse(data,0,total);
 const auto n=static_cast<double>(total);
 const auto bic_null=n*std::log(std::max(null_sse,1e-18)/n)+2.0*std::log(n);
 ScalingBoundary best;
 for(std::size_t split=3;split+3<=total;split++){
  const auto left=linear_fit(data,0,split),right=linear_fit(data,split,total);
  if(std::abs(right.first-left.first)<min_slope_shift)continue;
  const auto segmented_sse=residual_sse(data,0,split)+residual_sse(data,split,total);
  const auto bic_segmented=n*std::log(std::max(segmented_sse,1e-18)/n)+5.0*std::log(n);
  const auto gain=bic_null-bic_segmented;
  if(gain>=10.0&&(!best.candidate||gain>best.bic_gain)){
   best={true,split,left.first,right.first,gain,data[split-1].first,data[split].first};
  }
 }
 return best;
}
void scaling_study(const std::string& source,const std::string& out,int seed_count){
 if(seed_count<3||seed_count>20)throw Error("scaling requires 3..20 independent seeds per size");
 if(fs::exists(out))throw Error("scaling output already exists");
 Store base(source+"/run.sqlite");const auto cfg=config_from_json(Json::parse(base.config()));
 if(cfg.family!="integer_array"&&cfg.family!="topk"&&cfg.family!="graph")throw Error("scaling supports array, Top-K, and graph families");
 const std::string axis=cfg.family=="graph"?"vertices":"n";
 int max_n=cfg.family=="graph"?4096:50000;
 if(cfg.a=="insertion_sort"||cfg.b=="insertion_sort")max_n=std::min(max_n,4096);
 if(cfg.a=="quicksort_first_pivot"||cfg.b=="quicksort_first_pivot")max_n=std::min(max_n,20000);
 std::set<int> originally;for(const auto& v:cfg.axes){if(v.first!=axis)continue;for(const auto& x:v.second){
  try{std::size_t offset=0;auto n=std::stoi(x,&offset);if(offset==x.size()&&n>=2)originally.insert(n);}catch(const std::exception&){}
 }}
 std::set<int> sizes;
 auto consider=[&](int n){if(n>=2&&n<=max_n&&!originally.count(n))sizes.insert(n);};
 int current=std::max(4,originally.empty()?8:*originally.begin());
 for(int i=0;i<30&&sizes.size()<8;i++){
  consider(current);
  if(current>max_n/2)current+=std::max(1,(max_n-current)/4);
  else current=current*3/2+1;
 }
 if(sizes.size()<6)for(int n=2;n<=max_n&&sizes.size()<6;n+=std::max(1,n/3))consider(n);
 if(sizes.size()<6)throw Error("insufficient independent held-out sizes for scaling");
 std::vector<int> schedule(sizes.begin(),sizes.end());if(schedule.size()>8)schedule.resize(8);
 const auto total=static_cast<int>(schedule.size())*seed_count;
 Json config;config.put("schema_version",static_cast<std::int64_t>(1));config.put("experiment_id","scaling-"+cfg.experiment_id);
 config.put("problem_family",cfg.family);config.put("algorithm_a",cfg.a);config.put("algorithm_b",cfg.b);
 config.put("primary_axis",axis);config.put("seed",static_cast<std::int64_t>(cfg.seed));
 config.put("discovery_blocks",static_cast<std::int64_t>(3));config.put("confirm_blocks",static_cast<std::int64_t>(cfg.confirm_blocks));
 config.put("max_cases",static_cast<std::int64_t>(total+4*cfg.confirm_blocks));config.put("max_case_bytes",static_cast<std::int64_t>(cfg.max_bytes));
 config.put("warmup",static_cast<std::int64_t>(1));config.put("sample_ms",static_cast<std::int64_t>(cfg.sample_ms));
 config.put("rss_mb",static_cast<std::int64_t>(cfg.rss_mb));config.put("total_wall_seconds",static_cast<std::int64_t>(cfg.total_wall_s));
 config.put("base_params",case_params(cfg.base));
 Json axes;auto* arr=json_object_new_array();for(auto n:schedule)json_object_array_add(arr,json_object_new_int(n));json_object_object_add(axes.p,axis.c_str(),arr);config.put("axes",std::move(axes));
 fs::create_directories(out);Store store(out+"/run.sqlite");store.experiment(config.str(),environment());
 std::ostringstream csv;csv<<"n,seed,algorithm,case_id,status,ns_per_operation\n";
 std::map<std::string,std::map<int,std::vector<double>>> measurements;
 std::map<std::string,int> rejected_cases;
 const auto clock_start=std::chrono::steady_clock::now();
 const auto hard_deadline=clock_start+std::chrono::seconds(cfg.total_wall_s);
 for(auto n:schedule)for(int i=0;i<seed_count;i++){
  if(std::chrono::steady_clock::now()>=hard_deadline)throw Error("scaling budget exceeded (partial raw evidence retained)");
  Case c=cfg.base;
  c.seed=cfg.seed+10000000ULL+static_cast<std::uint64_t>(n)*131ULL+static_cast<std::uint64_t>(i)*7919ULL;
  if(cfg.family=="graph"){
   const auto v0=std::max<std::int64_t>(2,c.vertices);
   const auto density=static_cast<double>(c.edges)/static_cast<double>(v0*(v0-1));
   c.vertices=n;c.edges=static_cast<std::int64_t>(std::min<double>(200000.0,std::floor(density*static_cast<double>(n*(n-1)))));
   c.source=std::min<std::int64_t>(c.source,n-1);
  }else{
   if(cfg.family=="topk")c.k=static_cast<std::int64_t>(std::round(static_cast<double>(c.k)*n/static_cast<double>(std::max<std::int64_t>(1,c.n))));
   c.n=n;
  }
  c=make_case(c,cfg.max_bytes);
  const auto names=cfg.b.empty()?std::vector<std::string>{cfg.a}:std::vector<std::string>{cfg.a,cfg.b};
  const auto obs=measure(c,names,3,1,cfg.sample_ms,cfg.rss_mb,cfg.minimum_batch_ms,cfg.isolation_mode=="per_sample",hard_deadline,cfg.measurement_protocol_version);
  // Timing evidence is all-or-nothing at the seed/case boundary: a wrong
  // answer in any required block invalidates every comparison from that case.
  // In A/B mode the ABBA order produces TWO samples per algorithm per block.
  std::map<std::string,std::vector<double>> case_values;
  std::map<std::string,std::map<int,int>> block_counts;
  std::set<std::pair<std::int64_t,std::int64_t>> positions;
  bool admissible=obs.size()==static_cast<std::size_t>(3*(names.size()==2?4:1));
  for(const auto& o:obs){
   store.add_observation(c,o,false);
   if(std::find(names.begin(),names.end(),o.algorithm)==names.end()||o.block<0||o.block>=3||
      o.status!="ok"||o.ns<=0||o.batch<=0||o.order_position<0||
      !positions.emplace(o.block,o.order_position).second)admissible=false;
   else{
    block_counts[o.algorithm][static_cast<int>(o.block)]++;
    case_values[o.algorithm].push_back(static_cast<double>(o.ns)/static_cast<double>(o.batch));
   }
  }
  for(const auto& algorithm:names){
   if(case_values[algorithm].size()!=static_cast<std::size_t>(3*(names.size()==2?2:1)))admissible=false;
   for(int block=0;block<3;block++)if(block_counts[algorithm][block]!=(names.size()==2?2:1))admissible=false;
  }
  for(const auto& algorithm:names){
   const auto value=admissible?sample_median(case_values.at(algorithm)):0.0;
   if(admissible)measurements[algorithm][n].push_back(value);
   else rejected_cases[algorithm]++;
   csv<<n<<','<<c.seed<<','<<algorithm<<','<<c.id<<','<<(admissible?"ok":"invalid")<<','<<value<<'\n';
  }
 }
 /* The primary algorithm is the sole hypothesis selected for confirmation.
    Other algorithms receive descriptive fits without multiple-hypothesis
    promotion. Candidate selection uses discovery sizes/seeds only. */
 std::vector<std::pair<double,double>> primary_points;
 for(const auto& [n,times]:measurements[cfg.a])if(times.size()==static_cast<std::size_t>(seed_count))
  primary_points.emplace_back(static_cast<double>(n),sample_median(times));
 ScalingBoundary candidate;
 if(primary_points.size()>=8)candidate=detect_scaling_boundary(primary_points);
 Json scaling_evidence;scaling_evidence.put("kind","SCALING_REGIME_CHANGE");
 scaling_evidence.put("hypothesis_selection","one primary algorithm frozen before new-size holdout");
 scaling_evidence.put("primary_algorithm",cfg.a);
 scaling_evidence.put("candidate",candidate.candidate);
 scaling_evidence.put("status",candidate.candidate?"HYPOTHESIZED":"NO_CONFIRMED_BOUNDARY");
 if(candidate.candidate){
  scaling_evidence.put("discovery_left_max_n",candidate.left_n);
  scaling_evidence.put("discovery_right_min_n",candidate.right_n);
  scaling_evidence.put("discovery_slope_left",candidate.slope_left);
  scaling_evidence.put("discovery_slope_right",candidate.slope_right);
  scaling_evidence.put("discovery_bic_gain",candidate.bic_gain);
  const auto holdout_sizes=scaling_holdout_sizes(primary_points,candidate.split);
  const auto low=static_cast<std::int64_t>(candidate.left_n),high=static_cast<std::int64_t>(candidate.right_n);
  const auto frozen=case_params(cfg.base);
  store.hypothesis("SCALE-H1","OBSERVED",axis,low,high,std::abs(candidate.slope_right-candidate.slope_left),
    "Exploratory segmented log-log fit; no causal or asymptotic proof","SCALING_REGIME_CHANGE",frozen);
  store.hypothesis("SCALE-H1","HYPOTHESIZED",axis,low,high,std::abs(candidate.slope_right-candidate.slope_left),
    "One candidate selected for independent, previously unmeasured sizes");
  if(holdout_sizes.size()!=4||cfg.confirm_blocks<20){
   scaling_evidence.put("status","NOT_CONFIRMED_INSUFFICIENT_HOLDOUT_DESIGN");
   store.hypothesis("SCALE-H1","NOT_CONFIRMED",axis,low,high,0,
     "Insufficient distinct unmeasured n values or <20 independent confirmation seeds");
  }else{
   store.hypothesis("SCALE-H1","TESTED",axis,low,high,0,
     "Frozen scaling candidate; independent unmeasured n and seed schedule begins");
   std::map<int,std::vector<double>> confirmed;
   const auto names=cfg.b.empty()?std::vector<std::string>{cfg.a}:std::vector<std::string>{cfg.a,cfg.b};
   bool samples_valid=true;std::set<std::string> discovery_ids;
   for(const auto& previous:store.cases(false))discovery_ids.insert(previous.id);
   for(int n:holdout_sizes)for(int i=0;i<cfg.confirm_blocks;i++){
    if(std::chrono::steady_clock::now()>=hard_deadline){
     samples_valid=false;break;
    }
    Case c=cfg.base;
    c.seed=cfg.seed+30000000ULL+static_cast<std::uint64_t>(n)*131ULL+static_cast<std::uint64_t>(i)*7919ULL;
    if(cfg.family=="graph"){
     const auto v0=std::max<std::int64_t>(2,c.vertices);
     const auto density=static_cast<double>(c.edges)/static_cast<double>(v0*(v0-1));
     c.vertices=n;c.edges=static_cast<std::int64_t>(std::min<double>(200000.0,std::floor(density*static_cast<double>(n*(n-1)))));
     c.source=std::min<std::int64_t>(c.source,n-1);
    }else{
     if(cfg.family=="topk")c.k=static_cast<std::int64_t>(std::round(static_cast<double>(c.k)*n/static_cast<double>(std::max<std::int64_t>(1,c.n))));
     c.n=n;
    }
    c=make_case(c,cfg.max_bytes);
    if(discovery_ids.count(c.id)){samples_valid=false;break;}
    const auto rows=measure(c,names,1,1,cfg.sample_ms,cfg.rss_mb,cfg.minimum_batch_ms,cfg.isolation_mode=="per_sample",hard_deadline,cfg.measurement_protocol_version);
    std::vector<double> timings;
    bool cohort_valid=rows.size()==(names.size()==2?4U:1U);
    std::map<std::string,int> counts;
    std::set<std::int64_t> positions;
    for(const auto& row:rows){
     store.add_observation(c,row,true);
     if(std::find(names.begin(),names.end(),row.algorithm)==names.end()||row.block!=0||
        row.status!="ok"||row.ns<=0||row.batch<=0||row.order_position<0||
        !positions.insert(row.order_position).second)cohort_valid=false;
     counts[row.algorithm]++;
     if(row.algorithm==cfg.a&&row.status=="ok"&&row.ns>0&&row.batch>0)
      timings.push_back(static_cast<double>(row.ns)/static_cast<double>(row.batch));
    }
    for(const auto& algorithm:names)if(counts[algorithm]!=(names.size()==2?2:1))cohort_valid=false;
    if(!cohort_valid){samples_valid=false;}
    else confirmed[n].push_back(sample_median(timings));
    csv<<n<<','<<c.seed<<','<<cfg.a<<','<<c.id<<','<<(cohort_valid?"holdout":"invalid_holdout")<<','
       <<(cohort_valid?sample_median(timings):0.0)<<'\n';
   }
   std::vector<double> seed_differences;
   if(samples_valid&&confirmed.size()==4){
    for(int i=0;i<cfg.confirm_blocks;i++){
     const auto left_n0=holdout_sizes[0],left_n1=holdout_sizes[1],right_n0=holdout_sizes[2],right_n1=holdout_sizes[3];
     const auto left_slope=std::log(confirmed[left_n1][static_cast<std::size_t>(i)]/confirmed[left_n0][static_cast<std::size_t>(i)])/
                           std::log(static_cast<double>(left_n1)/static_cast<double>(left_n0));
     const auto right_slope=std::log(confirmed[right_n1][static_cast<std::size_t>(i)]/confirmed[right_n0][static_cast<std::size_t>(i)])/
                            std::log(static_cast<double>(right_n1)/static_cast<double>(right_n0));
     seed_differences.push_back(right_slope-left_slope);
    }
   }
   bool pass=false;
   if(seed_differences.size()==static_cast<std::size_t>(cfg.confirm_blocks)){
    const auto validation=evaluate_scaling_holdout(seed_differences,candidate.slope_right-candidate.slope_left,
                                                 cfg.seed,cfg.confirmation_confidence,cfg.fdr_q);
    pass=validation.confirmed;
    scaling_evidence.put("holdout_slope_shift",validation.slope_difference);
    scaling_evidence.put("holdout_ci_lower",validation.ci_lower);
    scaling_evidence.put("holdout_ci_upper",validation.ci_upper);
    scaling_evidence.put("holdout_signflip_p",validation.p_value);
   }
   scaling_evidence.put("status",pass?"CONFIRMED":"NOT_CONFIRMED");
   scaling_evidence.put("holdout_seed_count",static_cast<std::int64_t>(seed_differences.size()));
   scaling_evidence.put("holdout_new_size_count",static_cast<std::int64_t>(holdout_sizes.size()));
   auto* used=json_object_new_array();for(const int n:holdout_sizes)json_object_array_add(used,json_object_new_int(n));
   json_object_object_add(scaling_evidence.p,"holdout_sizes",used);
   if(pass)store.seal_confirmation("SCALE-H1",cfg.a,cfg.confirm_blocks,true);
   store.hypothesis("SCALE-H1",pass?"CONFIRMED":"NOT_CONFIRMED",axis,low,high,
     std::abs(candidate.slope_right-candidate.slope_left),
     pass?"New-size, new-seed 99% case-paired slope contrast confirmed; empirical finite n only":
          "Independent new-size holdout failed or was interrupted; no confirmed regime change");
  }
 }
 write_file(out+"/scaling_samples.csv",csv.str());
 Json result;result.put("scaling_regime_evidence",std::move(scaling_evidence));result.put("study","independent held-out size fingerprint");result.put("source_run",fs::path(source).filename().string());result.put("seed_count_per_size",static_cast<std::int64_t>(seed_count));
 result.put("environment",Json::parse(store.environment()));
 auto* models=json_object_new_object();
 for(const auto& algorithm:(cfg.b.empty()?std::vector<std::string>{cfg.a}:std::vector<std::string>{cfg.a,cfg.b})){
  const auto& by_size=measurements[algorithm];
  std::vector<std::pair<double,double>> data;
  for(const auto& [n,times]:by_size)if(times.size()==static_cast<std::size_t>(seed_count))data.emplace_back(static_cast<double>(n),sample_median(times));
  Json alg;alg.put("status",data.size()>=6?"EMPIRICAL_FIT":"INSUFFICIENT_VALID_SIZES");
  alg.put("rejected_discovery_cases",static_cast<std::int64_t>(rejected_cases[algorithm]));
  alg.put("valid_discovery_sizes",static_cast<std::int64_t>(data.size()));
  auto* points=json_object_new_array();for(const auto& [n,y]:data){Json point;point.put("n",n);point.put("median_ns_per_operation",y);json_object_array_add(points,point.p);point.p=nullptr;}
  json_object_object_add(alg.p,"sizes",points);
  if(data.size()>=6){auto* fits=json_object_new_array();for(const auto& fit:fit_growth_models(data)){Json m;m.put("model",fit.model);m.put("cross_validated_mean_abs_log_error",fit.log_error);m.put("fitted_scale",fit.factor);json_object_array_add(fits,m.p);m.p=nullptr;}json_object_object_add(alg.p,"growth_models",fits);}
  json_object_object_add(models,algorithm.c_str(),alg.p);alg.p=nullptr;
 }
 json_object_object_add(result.p,"algorithms",models);
 result.put("limitation","Finite-size empirical fits only. Only one frozen primary algorithm can receive new-size and new-seed confirmation. This is not a proof of Big-O or causality. Other input parameters were fixed or explicitly scaled.");
 write_file(out+"/scaling.json",result.str());
 std::cout<<"Scaling study: "<<out<<" sizes="<<schedule.size()<<" seeds_per_size="<<seed_count<<"\\n";
}
}
