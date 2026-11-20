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
namespace ps {
namespace fs=std::filesystem;
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
 config.put("max_cases",static_cast<std::int64_t>(total));config.put("max_case_bytes",static_cast<std::int64_t>(cfg.max_bytes));
 config.put("warmup",static_cast<std::int64_t>(1));config.put("sample_ms",static_cast<std::int64_t>(cfg.sample_ms));
 config.put("rss_mb",static_cast<std::int64_t>(cfg.rss_mb));config.put("total_wall_seconds",static_cast<std::int64_t>(cfg.total_wall_s));
 config.put("base_params",case_params(cfg.base));
 Json axes;auto* arr=json_object_new_array();for(auto n:schedule)json_object_array_add(arr,json_object_new_int(n));json_object_object_add(axes.p,axis.c_str(),arr);config.put("axes",std::move(axes));
 fs::create_directories(out);Store store(out+"/run.sqlite");store.experiment(config.str(),environment());
 std::ostringstream csv;csv<<"n,seed,algorithm,case_id,status,ns_per_operation\n";
 std::map<std::string,std::map<int,std::vector<double>>> measurements;
 const auto clock_start=std::chrono::steady_clock::now();
 for(auto n:schedule)for(int i=0;i<seed_count;i++){
  if(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-clock_start).count()>=cfg.total_wall_s)throw Error("scaling budget exceeded (partial raw evidence retained)");
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
  const auto obs=measure(c,names,3,1,cfg.sample_ms,cfg.rss_mb);
  std::map<std::string,std::vector<double>> case_values;
  for(const auto& o:obs){store.add_observation(c,o,true);
   if(o.status=="ok"&&o.ns>0&&o.batch>0)case_values[o.algorithm].push_back(static_cast<double>(o.ns)/static_cast<double>(o.batch));
  }
  for(const auto& algorithm:names){
   const auto it=case_values.find(algorithm);
   const auto good=it!=case_values.end()&&!it->second.empty();
   const auto value=good?sample_median(it->second):0.0;
   if(good)measurements[algorithm][n].push_back(value);
   csv<<n<<','<<c.seed<<','<<algorithm<<','<<c.id<<','<<(good?"ok":"invalid")<<','<<value<<'\n';
  }
 }
 write_file(out+"/scaling_samples.csv",csv.str());
 Json result;result.put("study","independent held-out size fingerprint");result.put("source_run",fs::path(source).filename().string());result.put("seed_count_per_size",static_cast<std::int64_t>(seed_count));
 result.put("environment",Json::parse(store.environment()));
 auto* models=json_object_new_object();
 for(const auto& [algorithm,by_size]:measurements){
  std::vector<std::pair<double,double>> data;
  for(const auto& [n,times]:by_size)if(times.size()==static_cast<std::size_t>(seed_count))data.emplace_back(static_cast<double>(n),sample_median(times));
  Json alg;alg.put("status",data.size()>=6?"EMPIRICAL_FIT":"INSUFFICIENT_VALID_SIZES");
  auto* points=json_object_new_array();for(const auto& [n,y]:data){Json point;point.put("n",n);point.put("median_ns_per_operation",y);json_object_array_add(points,point.p);point.p=nullptr;}
  json_object_object_add(alg.p,"sizes",points);
  if(data.size()>=6){auto* fits=json_object_new_array();for(const auto& fit:fit_growth_models(data)){Json m;m.put("model",fit.model);m.put("cross_validated_mean_abs_log_error",fit.log_error);m.put("fitted_scale",fit.factor);json_object_array_add(fits,m.p);m.p=nullptr;}json_object_object_add(alg.p,"growth_models",fits);}
  json_object_object_add(models,algorithm.c_str(),alg.p);alg.p=nullptr;
 }
 json_object_object_add(result.p,"algorithms",models);
 result.put("limitation","Empirical shape compatibility on eight independently selected n values, not asymptotic proof; CPU/algorithm/system-specific. Other input parameters held fixed or scaled explicitly.");
 write_file(out+"/scaling.json",result.str());
 std::cout<<"Scaling study: "<<out<<" sizes="<<schedule.size()<<" seeds_per_size="<<seed_count<<"\\n";
}
}
