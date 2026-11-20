#include "phaseshift/experiment.hpp"
#include "phaseshift/statistics.hpp"
#include "phaseshift/morphology.hpp"
#include "phaseshift/minimize.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
namespace ps {
namespace fs=std::filesystem;
static std::string value_string(json_object* v){if(!v)throw Error("null axis value");auto t=json_object_get_type(v);if(t==json_type_string)return json_object_get_string(v);if(t==json_type_int)return std::to_string(json_object_get_int64(v));
 if(t==json_type_double)return json_object_to_json_string_ext(v,JSON_C_TO_STRING_PLAIN);
 if(t==json_type_boolean)return json_object_get_boolean(v)?"true":"false";
 throw Error("axis values must be string/integer/fraction/bool");}
static std::int64_t parse_ppm(const std::string& value){
 /* Locale-independent exact fixed-point conversion (six decimal places). */
 if(value.empty()||value[0]=='-'||value[0]=='+')throw Error("fraction outside [0,1]: "+value);
 const auto point=value.find('.');const auto whole=value.substr(0,point);
 if(whole!="0"&&whole!="1")throw Error("fraction outside [0,1]: "+value);
 const auto fraction=point==std::string::npos?"":value.substr(point+1);
 if(fraction.size()>6||std::any_of(fraction.begin(),fraction.end(),[](unsigned char ch){return ch<'0'||ch>'9';}))throw Error("fraction requires at most six decimal places");
 std::int64_t ppm=whole=="1"?1000000:0,mult=100000;
 for(char digit:fraction){ppm+=static_cast<std::int64_t>(digit-'0')*mult;mult/=10;}
 if(ppm>1000000)throw Error("fraction exceeds 1");
 return ppm;
}
static void assign(Case& c,const std::string& key,const std::string& v){
 if(key=="distribution"){c.distribution=v;return;}
 if(key=="weight_class"){c.weight_class=v;return;}
 if(key=="connectivity"){c.connectivity=v;return;}
 if(key=="directed"){
  if(v!="true"&&v!="false")throw Error("directed must be boolean");
  c.directed=v=="true";return;
 }
 if(key=="negative"){
  if(v!="true"&&v!="false")throw Error("negative must be boolean");
  c.negative=v=="true";return;
 }
 if(key=="duplicate_fraction"||key=="disorder_fraction"||key=="sign_fraction"||key=="density"||key=="degree_skew"){
  auto ppm=parse_ppm(v);
  if(key=="duplicate_fraction")c.duplicate_ppm=ppm;
  else if(key=="disorder_fraction")c.disorder_ppm=ppm;
  else if(key=="sign_fraction")c.sign_ppm=ppm;
  else if(key=="density")c.density_ppm=ppm;
  else c.degree_skew_ppm=ppm;
  return;
 }
 std::int64_t n;
 try{std::size_t p=0;n=std::stoll(v,&p);if(p!=v.size())throw Error("invalid numeric axis");}
 catch(const std::exception&){throw Error("invalid numeric parameter "+key+"="+v);}
 if(key=="x")c.x=n;else if(key=="n")c.n=n;else if(key=="k")c.k=n;
 else if(key=="vertices")c.vertices=n;else if(key=="edges")c.edges=n;
 else if(key=="source")c.source=n;else if(key=="threshold")c.threshold=n;
 else if(key=="generator_version")c.generator_version=n;
 else if(key=="value_range")c.value_range=n;else if(key=="unique_count")c.unique_count=n;
 else if(key=="run_length")c.run_length=n;else if(key=="duplicate_ppm")c.duplicate_ppm=n;
 else if(key=="disorder_ppm")c.disorder_ppm=n;else if(key=="sign_ppm")c.sign_ppm=n;
 else if(key=="density_ppm")c.density_ppm=n;else if(key=="degree_skew_ppm")c.degree_skew_ppm=n;
 else if(key=="community_count")c.community_count=n;else if(key=="max_weight")c.max_weight=n;
 else throw Error("unknown morphology parameter: "+key);
}
static void apply_field(Case& c,const std::string& key,json_object* value){assign(c,key,value_string(value));}
static bool numeric_axis(const std::string& s){
 static const std::set<std::string> supported={"x","n","k","edges","vertices","value_range","unique_count","run_length","duplicate_ppm","disorder_ppm","sign_ppm","density_ppm","density","degree_skew_ppm","degree_skew","community_count","max_weight","duplicate_fraction","disorder_fraction","sign_fraction"};
 return supported.count(s)>0;
}
static Json config_json(const Config& c){
 Json j;j.put("schema_version",static_cast<std::int64_t>(1));j.put("experiment_id",c.experiment_id);j.put("problem_family",c.family);j.put("algorithm_a",c.a);j.put("algorithm_b",c.b);j.put("search_method",c.method);j.put("primary_axis",c.axis);j.put("seed",static_cast<std::int64_t>(c.seed));j.put("max_cases",static_cast<std::int64_t>(c.max_cases));j.put("max_case_bytes",static_cast<std::int64_t>(c.max_bytes));j.put("max_search_queue",static_cast<std::int64_t>(c.max_queue));j.put("max_report_bytes",static_cast<std::int64_t>(c.max_report_bytes));j.put("discovery_blocks",static_cast<std::int64_t>(c.blocks));j.put("confirm_blocks",static_cast<std::int64_t>(c.confirm_blocks));j.put("warmup",static_cast<std::int64_t>(c.warmup));j.put("sample_ms",static_cast<std::int64_t>(c.sample_ms));j.put("rss_mb",static_cast<std::int64_t>(c.rss_mb));j.put("total_wall_seconds",static_cast<std::int64_t>(c.total_wall_s));j.put("min_practical_ratio",c.min_ratio);j.put("confirmation_confidence",c.confirmation_confidence);j.put("fdr_q",c.fdr_q);j.put("multiple_testing",c.multiple_testing);j.put("base_params",case_params(c.base));
 Json axes;for(const auto& [name,values]:c.axes){auto* arr=json_object_new_array();for(const auto& val:values)json_object_array_add(arr,json_object_new_string(val.c_str()));json_object_object_add(axes.p,name.c_str(),arr);}j.put("axes",std::move(axes));return j;
}
Config config_from_json(const Json& j){
 if(j.i("schema_version",1)!=1)throw Error("only config schema version 1 supported");
 Config c;c.experiment_id=j.s("experiment_id","experiment");c.family=j.s("problem_family",j.s("family","calibration"));
 c.a=j.s("algorithm_a","");c.b=j.s("algorithm_b","");
 if(auto* a=j.get("algorithms")){if(json_object_get_type(a)!=json_type_array||json_object_array_length(a)<1||json_object_array_length(a)>2)throw Error("algorithms must contain one or two elements");c.a=json_object_get_string(json_object_array_get_idx(a,0));if(json_object_array_length(a)>1)c.b=json_object_get_string(json_object_array_get_idx(a,1));}
 if(c.a.empty())c.a=c.family=="calibration"?"calibration":c.family=="integer_array"?"std_sort":c.family=="topk"?"topk_heap":"dijkstra_heap";
 c.method=j.s("search_method",j.s("method","grid"));if(auto* search=j.get("search")){Json k(json_object_get(search));c.method=k.s("method",c.method);c.max_cases=static_cast<std::uint64_t>(k.i("max_cases",static_cast<std::int64_t>(c.max_cases)));}
 c.axis=j.s("primary_axis",c.family=="calibration"?"x":c.family=="graph"?"edges":"n");
 c.seed=static_cast<std::uint64_t>(j.i("seed",137));c.max_cases=static_cast<std::uint64_t>(j.i("max_cases",static_cast<std::int64_t>(c.max_cases)));c.max_bytes=static_cast<std::uint64_t>(j.i("max_case_bytes",64*1024*1024));c.max_queue=static_cast<std::uint64_t>(j.i("max_search_queue",10000));c.max_report_bytes=static_cast<std::uint64_t>(j.i("max_report_bytes",10000000));c.blocks=static_cast<int>(j.i("discovery_blocks",9));c.confirm_blocks=static_cast<int>(j.i("confirm_blocks",20));c.warmup=static_cast<int>(j.i("warmup",2));c.sample_ms=static_cast<int>(j.i("sample_ms",3000));c.rss_mb=static_cast<int>(j.i("rss_mb",1024));c.total_wall_s=static_cast<int>(j.i("total_wall_seconds",600));c.min_ratio=j.d("min_practical_ratio",1.2);
 if(auto* m=j.get("measurement")){Json x(json_object_get(m));c.blocks=static_cast<int>(x.i("paired_discovery_blocks",c.blocks));c.confirm_blocks=static_cast<int>(x.i("paired_confirm_blocks",c.confirm_blocks));c.warmup=static_cast<int>(x.i("warmup_iterations",c.warmup));}
 if(auto* d=j.get("detection")){Json x(json_object_get(d));c.min_ratio=x.d("min_practical_ratio",c.min_ratio);c.confirmation_confidence=x.d("confirmation_confidence",c.confirmation_confidence);c.fdr_q=x.d("fdr_q",c.fdr_q);c.multiple_testing=x.s("multiple_testing",c.multiple_testing);}
 if(auto* l=j.get("limits")){Json x(json_object_get(l));c.total_wall_s=static_cast<int>(x.i("total_wall_seconds",c.total_wall_s));c.max_cases=static_cast<std::uint64_t>(x.i("max_cases",static_cast<std::int64_t>(c.max_cases)));c.max_bytes=static_cast<std::uint64_t>(x.i("max_case_bytes",static_cast<std::int64_t>(c.max_bytes)));c.rss_mb=static_cast<int>(x.i("max_worker_rss_mb",c.rss_mb));c.sample_ms=static_cast<int>(x.i("max_sample_wall_ms",c.sample_ms));c.max_queue=static_cast<std::uint64_t>(x.i("max_search_queue",static_cast<std::int64_t>(c.max_queue)));c.max_report_bytes=static_cast<std::uint64_t>(x.i("max_report_bytes",static_cast<std::int64_t>(c.max_report_bytes)));}
 c.base.family=c.family;c.base.seed=c.seed;
 if(auto* base=j.get("base_params")){Json p=json_object_get_type(base)==json_type_string?Json::parse(json_object_get_string(base)):Json(json_object_get(base));
  if(!p.get("generator_version"))c.base.generator_version=1; /* v0.1 manifest compatibility */
  json_object_object_foreach(p.p,key,value){apply_field(c.base,key,value);}
 }
 if(auto* fixed=j.get("fixed")){Json p(json_object_get(fixed));json_object_object_foreach(p.p,key,value){apply_field(c.base,key,value);}}
 if(auto* axes=j.get("axes")){if(json_object_get_type(axes)!=json_type_object)throw Error("axes must be object");
  json_object_object_foreach(axes,key,value){
   auto* arr=value;if(json_object_get_type(value)==json_type_object)json_object_object_get_ex(value,"values",&arr);
   if(!arr||json_object_get_type(arr)!=json_type_array||json_object_array_length(arr)==0)throw Error("axis values must be nonempty array");
   auto& vals=c.axes[key];for(std::size_t i=0;i<json_object_array_length(arr);i++)vals.push_back(value_string(json_object_array_get_idx(arr,i)));
  }
 }
 if(c.axes.empty())c.axes[c.axis]={"0","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15"};
 if(c.method!="grid"&&c.method!="random"&&c.method!="stratified"&&c.method!="adaptive_partition")throw Error("unknown search method");
 if(c.max_cases==0||c.max_cases>20000||c.max_queue==0||c.max_queue>100000||c.max_report_bytes<4096||c.max_report_bytes>1000000000||c.max_bytes<64||c.max_bytes>64ULL*1024*1024||c.blocks<1||c.blocks>100||c.confirm_blocks<1||c.confirm_blocks>100||c.warmup<0||c.warmup>100||c.sample_ms<100||c.sample_ms>60000||c.total_wall_s<1||c.total_wall_s>86400||c.rss_mb<64||c.rss_mb>65536||!std::isfinite(c.min_ratio)||c.min_ratio<1.01||c.min_ratio>100000||!std::isfinite(c.confirmation_confidence)||c.confirmation_confidence<0.90||c.confirmation_confidence>=1.0||!std::isfinite(c.fdr_q)||c.fdr_q<0.001||c.fdr_q>0.25||c.multiple_testing!="bh_fdr")throw Error("configuration resource/measurement bounds invalid");
 if(!c.axes.count(c.axis))c.axis=c.axes.begin()->first;
 auto valid=algorithms_for(c.family);if(std::find(valid.begin(),valid.end(),c.a)==valid.end()&&c.a.rfind("plugin:",0)!=0)throw Error("unknown primary algorithm for family");
 if(!c.b.empty()&&std::find(valid.begin(),valid.end(),c.b)==valid.end()&&c.b.rfind("plugin:",0)!=0)throw Error("unknown secondary algorithm for family");
 std::uint64_t product=1;
 for(const auto& [key,values]:c.axes){
  if(values.size()>c.max_queue||product>c.max_queue/values.size())throw Error("search grid exceeds max_search_queue; split the experiment");
  product*=values.size();
  for(const auto& v:values){Case sample=c.base;assign(sample,key,v);}
 }
 if(c.base.density_ppm>=0&&c.axes.count("edges"))throw Error("density and edges cannot both be independent axes");
 /* Validate every combination without allocating experiment inputs. */
 std::vector<std::pair<std::string,std::vector<std::string>>> dims(c.axes.begin(),c.axes.end());
 std::function<void(std::size_t,Case)> visit=[&](std::size_t index,Case candidate){
  if(index==dims.size()){validate_case_schema(candidate,c.max_bytes);return;}
  for(const auto& value:dims[index].second){Case next=candidate;assign(next,dims[index].first,value);visit(index+1,next);}
 };
 visit(0,c.base);
 return c;
}
Config parse_config(const std::string& path){return config_from_json(Json::file(path));}
static std::vector<Case> grid_cases(const Config& c){
 std::vector<Case> out;std::vector<std::pair<std::string,std::vector<std::string>>> axes(c.axes.begin(),c.axes.end());
 std::function<void(std::size_t,Case)> recurse=[&](std::size_t i,Case base){
  if(i==axes.size()){
   if(out.size()>=c.max_queue)throw Error("search queue bound exceeded");
   base.seed=c.seed+static_cast<std::uint64_t>(out.size());
   out.push_back(std::move(base));return;
  }
  for(const auto& value:axes[i].second){auto candidate=base;assign(candidate,axes[i].first,value);recurse(i+1,candidate);}
 };recurse(0,c.base);return out;
}
static std::int64_t axis_value(const Case& c,const std::string& axis);
static std::vector<Case> scheduled(const Config& c){
 auto cases=grid_cases(c);
 if(c.method=="random"){
  std::uint64_t seed=c.seed^0x925c42;
  for(std::size_t i=cases.size();i>1;i--){auto j=randbelow(seed,i);std::swap(cases[i-1],cases[static_cast<std::size_t>(j)]);}
 }else if(c.method=="stratified" && numeric_axis(c.axis)){
  std::sort(cases.begin(),cases.end(),[&](const Case& a,const Case& b){return axis_value(a,c.axis)<axis_value(b,c.axis);});
  std::vector<Case> order;order.reserve(cases.size());
  const std::size_t strata=std::min<std::size_t>(8,cases.size());
  for(std::size_t offset=0;offset<cases.size();offset++)for(std::size_t i=0;i<strata;i++){
   const auto start=i*cases.size()/strata,end=(i+1)*cases.size()/strata;
   if(start+offset<end)order.push_back(cases[start+offset]);
  }
  cases=std::move(order);
 }
 return cases;
}
static std::int64_t axis_value(const Case& c,const std::string& axis){
 if(axis=="x")return c.x;
 if(axis=="n")return c.n;
 if(axis=="k")return c.k;
 if(axis=="edges")return c.edges;
 if(axis=="vertices")return c.vertices;
 if(axis=="value_range")return c.value_range;
 if(axis=="unique_count")return c.unique_count;
 if(axis=="run_length")return c.run_length;
 if(axis=="duplicate_fraction"||axis=="duplicate_ppm")return c.duplicate_ppm;
 if(axis=="disorder_fraction"||axis=="disorder_ppm")return c.disorder_ppm;
 if(axis=="sign_fraction"||axis=="sign_ppm")return c.sign_ppm;
 if(axis=="density"||axis=="density_ppm")return c.density_ppm;
 if(axis=="degree_skew"||axis=="degree_skew_ppm")return c.degree_skew_ppm;
 if(axis=="community_count")return c.community_count;
 if(axis=="max_weight")return c.max_weight;
 throw Error("unsupported structural axis: "+axis);
}
static std::optional<double> metric(Store& store,const Case& c,const Config& cfg,bool holdout){
 const auto obs=store.observations(c.id,holdout);
 std::map<std::int64_t,std::vector<double>> a,b;
 for(const auto& o:obs){
  /* A case with *any* failed target execution is inadmissible. Otherwise a
     fast valid batch could mask a wrong answer later in the same case. */
  if((o.algorithm==cfg.a||o.algorithm==cfg.b)&&
      (o.status!="ok"||o.batch<1||(o.ns<=0&&!(cfg.family=="calibration"&&o.work>0))))return {};
 }
 for(const auto& o:obs){
  if(o.status!="ok"||o.batch<1)continue;
  double value=0;
  if(cfg.family=="calibration")value=static_cast<double>(o.work);
  else if(o.ns>0)value=static_cast<double>(o.ns)/static_cast<double>(o.batch);
  if(!std::isfinite(value)||value<=0)continue;
  if(o.algorithm==cfg.a)a[o.block].push_back(value);
  if(o.algorithm==cfg.b)b[o.block].push_back(value);
 }
 if(a.empty())return {};
 std::vector<double> paired;
 for(const auto& [block,va]:a){
  if(cfg.b.empty())paired.push_back(std::log(sample_median(va)));
  else if(b.count(block)&&!b.at(block).empty())paired.push_back(std::log(sample_median(va)/sample_median(b.at(block))));
 }
 if(paired.empty())return {};
 return sample_median(paired);
}
/* Group every non-primary axis by exact typed parameter values, not by the
   partially observed primary metric. Categorical dimensions are conditioned
   on; they are never projected onto a fictitious numeric continuum. */
static std::string scope_key(const Case& candidate,const Config& cfg){
 Json canonical=Json::parse(case_params(candidate));Json scope;
 for(const auto& [key,values]:cfg.axes){
  if(key==cfg.axis||values.size()==1)continue;
  std::string field=key;
  if(key=="density")field="density_ppm";
  else if(key=="degree_skew")field="degree_skew_ppm";
  else if(key=="duplicate_fraction")field="duplicate_ppm";
  else if(key=="disorder_fraction")field="disorder_ppm";
  else if(key=="sign_fraction")field="sign_ppm";
  auto* value=canonical.get(field.c_str());
  if(!value)throw Error("scope axis missing in generator parameters: "+key);
  scope.put(key.c_str(),Json(json_object_get(value)));
 }
 return scope.str();
}
static std::string formatted_axis_value(const std::string& axis,std::int64_t value){
 if(axis=="density"||axis=="degree_skew"||axis=="duplicate_fraction"||
    axis=="disorder_fraction"||axis=="sign_fraction"){
  if(value<0||value>1000000)throw Error("fractional structural axis out of range");
  return std::to_string(value/1000000)+"."+([&]{
   std::ostringstream suffix;suffix<<std::setw(6)<<std::setfill('0')<<(value%1000000);return suffix.str();
  }());
 }
 return std::to_string(value);
}
static std::string axis_label(const Case& c,const std::string& axis){
 if(numeric_axis(axis))return formatted_axis_value(axis,axis_value(c,axis));
 if(axis=="distribution")return c.distribution;
 if(axis=="weight_class")return c.weight_class;
 if(axis=="connectivity")return c.connectivity;
 if(axis=="directed")return c.directed?"true":"false";
 if(axis=="negative")return c.negative?"true":"false";
 throw Error("unrecognized categorical report axis: "+axis);
}
struct Detection {
 std::int64_t left=0,right=0;
 double effect=0;
 std::string kind,frozen_params,scope,failure_status;
};
static std::optional<Detection> detect(Store& store,const Config& cfg){
 if(!numeric_axis(cfg.axis))return {};
 /* Structured failure boundaries use censored/status observations, not a
    fabricated large runtime. The candidate must be persistently valid on
    the lower bin and consistently fail in the adjacent upper bin. */
 if(cfg.family!="calibration"){
  struct State {std::string status;Case case_params;};
  std::map<std::string,std::map<std::int64_t,State>> categories;
  for(const auto& c:store.cases(false)){
   const auto rows=store.observations(c.id,false);
   std::size_t count=0;std::string common;bool consistent=true;
   for(const auto& o:rows)if(o.algorithm==cfg.a){
    if(count++==0)common=o.status;
    else if(o.status!=common)consistent=false;
   }
   if(!consistent||count<static_cast<std::size_t>(cfg.blocks)||common.empty())continue;
   const auto key=scope_key(c,cfg);const auto x=axis_value(c,cfg.axis);
   auto& states=categories[key];
   auto it=states.find(x);
   if(it!=states.end()&&it->second.status!=common)states.erase(it);
   else states.emplace(x,State{common,c});
  }
  for(const auto& [scope,states]:categories){
   for(auto it=std::next(states.begin(),states.empty()?0:1);it!=states.end();++it){
    const auto previous=std::prev(it);
    if(previous->second.status!="ok")continue;
    const auto& failure=it->second.status;
    const auto is_correctness=failure=="wrong_answer"||failure=="signaled"||failure=="crashed";
    const auto is_resource=failure=="timeout"||failure=="out_of_memory";
    if(!is_correctness&&!is_resource)continue;
    Detection d;d.left=previous->first;d.right=it->first;d.effect=1000000;
    d.kind=is_correctness?"CORRECTNESS_BOUNDARY":"MEMORY_OR_RESOURCE_BOUNDARY";
    d.scope=scope;d.failure_status=failure;
    d.frozen_params=case_params(previous->second.case_params);
    return d;
   }
  }
 }

 std::map<std::string,std::map<std::int64_t,std::vector<double>>> groups;
 std::map<std::string,Case> representatives;
 for(const auto& candidate:store.cases(false)){
  if(const auto m=metric(store,candidate,cfg,false)){
   const auto context=scope_key(candidate,cfg);
   groups[context][axis_value(candidate,cfg.axis)].push_back(*m);
   representatives.emplace(context,candidate);
  }
 }
 std::optional<Detection> overall;
 for(const auto& [scope,grouped]:groups){
  if(grouped.size()<4)continue;
  std::vector<std::int64_t> x;std::vector<double> y;
  for(const auto& [position,observed]:grouped){x.push_back(position);y.push_back(sample_median(observed));}
  std::optional<Detection> group_candidate;
  if(!cfg.b.empty()&&cfg.family!="calibration"){
   for(std::size_t i=1;i<y.size();i++)if(y[i-1]*y[i]<0){
    const auto effect=std::min(std::abs(y[i-1]),std::abs(y[i]));
    if(effect>std::log(1.05)&&(!group_candidate||effect>group_candidate->effect))
     group_candidate=Detection{x[i-1],x[i],effect,"PERFORMANCE_CROSSOVER","",scope,""};
   }
  }
  if(!group_candidate){
   const auto avg=std::accumulate(y.begin(),y.end(),0.0)/static_cast<double>(y.size());
   double null_sse=0;for(auto v:y)null_sse+=(v-avg)*(v-avg);
   double best_sse=std::numeric_limits<double>::infinity();std::size_t cut=0;
   for(std::size_t j=1;j<y.size();j++){
    const auto ml=std::accumulate(y.begin(),y.begin()+static_cast<std::ptrdiff_t>(j),0.0)/static_cast<double>(j);
    const auto mr=std::accumulate(y.begin()+static_cast<std::ptrdiff_t>(j),y.end(),0.0)/static_cast<double>(y.size()-j);
    double sse=0;
    for(std::size_t k=0;k<j;k++)sse+=(y[k]-ml)*(y[k]-ml);
    for(std::size_t k=j;k<y.size();k++)sse+=(y[k]-mr)*(y[k]-mr);
    if(sse<best_sse){best_sse=sse;cut=j;}
   }
   const auto effect=std::abs(y[cut]-y[cut-1]);
   const auto n=static_cast<double>(y.size());
   const double bic0=n*std::log((null_sse+1e-12)/n)+std::log(n);
   const double bic1=n*std::log((best_sse+1e-12)/n)+3.0*std::log(n);
   double other=0;for(std::size_t i=1;i<y.size();i++)if(i!=cut)other=std::max(other,std::abs(y[i]-y[i-1]));
   if(bic0-bic1>=6&&effect>=std::log(cfg.min_ratio)&&effect>=2.0*other)
    group_candidate=Detection{x[cut-1],x[cut],effect,
     cfg.family=="calibration"?"SYNTHETIC_CALIBRATION":"PERFORMANCE_CLIFF","",scope,""};
  }
  if(group_candidate){
   group_candidate->frozen_params=case_params(representatives.at(scope));
   if(!overall||group_candidate->effect>overall->effect)overall=group_candidate;
  }
 }
 return overall;
}
static std::size_t adaptive_next(Store& store,const Config& cfg,const std::vector<Case>& pool,const std::vector<bool>& chosen,std::uint64_t& random_state){
 std::vector<std::size_t> available;available.reserve(pool.size());
 for(std::size_t i=0;i<pool.size();i++)if(!chosen[i])available.push_back(i);
 if(available.empty())throw Error("adaptive scheduler exhausted");
 const auto count=pool.size()-available.size();
 if(count==0)return available.front();
 if(count==1)return available.back();
 /* Predeclare 20% independent uniform exploration, even if a locally
    attractive candidate exists. Do not pool unrelated structural contexts. */
 if(count%5==4)return available[static_cast<std::size_t>(randbelow(random_state,available.size()))];
 std::map<std::string,std::map<std::int64_t,double>> observed;
 for(std::size_t i=0;i<pool.size();i++)if(chosen[i]){
  auto c=make_case(pool[i],cfg.max_bytes);
  if(auto m=metric(store,c,cfg,false))observed[scope_key(c,cfg)][axis_value(c,cfg.axis)]=*m;
 }
 std::size_t best=available.front();double priority=-1;
 for(auto index:available){
  const auto x=axis_value(pool[index],cfg.axis);
  const auto context=scope_key(pool[index],cfg);
  auto& context_values=observed[context];
  auto upper=context_values.lower_bound(x);
  if(upper==context_values.end()||upper==context_values.begin())continue;
  auto lower=std::prev(upper);
  const auto gap=static_cast<double>(upper->first-lower->first);
  if(gap<=0)continue;
  const auto contrast=std::abs(upper->second-lower->second);
  const auto center=(static_cast<double>(upper->first)+static_cast<double>(lower->first))/2.0;
  const auto score=(0.01+contrast)*gap/(1.0+std::abs(static_cast<double>(x)-center));
  if(score>priority){best=index;priority=score;}
 }
 return best;
}
static void execute_schedule(const Config& cfg,const std::string& out,Store& store,std::size_t pause_after){
 auto choices=scheduled(cfg);std::vector<bool> chosen(choices.size(),false);
 const bool feedback=cfg.method=="adaptive_partition"&&numeric_axis(cfg.axis);
 auto completed=std::size_t{0};std::uint64_t rng=cfg.seed^0xf39b247bf04aULL;
 /* The scheduler transcript, samples, and case are committed atomically.
    Resume trusts neither a reported counter nor an unverified case digest. */
 for(const auto& d:store.scheduler_decisions()){
  if(d.pool_index<0||static_cast<std::size_t>(d.pool_index)>=choices.size()||chosen[static_cast<std::size_t>(d.pool_index)])throw Error("invalid scheduler transcript: repeated/bad index");
  const auto idx=static_cast<std::size_t>(d.pool_index);
  auto expected=make_case(choices[idx],cfg.max_bytes);
  if(expected.id!=d.case_id)throw Error("scheduler transcript disagrees with immutable config/case");
  chosen[idx]=true;completed++;rng=d.next_rng_state;
 }
 if(completed>cfg.max_cases)throw Error("persisted decisions exceed immutable case budget");
 const auto start=std::chrono::steady_clock::now();std::size_t this_call=0;
 while(completed<cfg.max_cases&&completed<choices.size()&&(pause_after==0||this_call<pause_after)){
  const auto elapsed=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-start).count();
  if(elapsed>=cfg.total_wall_s)break;
  const auto index=feedback?adaptive_next(store,cfg,choices,chosen,rng):completed;
  if(chosen[index])throw Error("scheduler selected previously evaluated case");
  auto c=make_case(choices[index],cfg.max_bytes);
  const auto obs=measure(c,cfg.b.empty()?std::vector<std::string>{cfg.a}:std::vector<std::string>{cfg.a,cfg.b},cfg.blocks,cfg.warmup,cfg.sample_ms,cfg.rss_mb);
  store.exec("BEGIN IMMEDIATE");
  try{
   for(const auto& o:obs)store.add_observation(c,o,false);
   store.record_decision(index,c.id,rng);
   store.exec("COMMIT");
  }catch(...){store.exec("ROLLBACK");throw;}
  chosen[index]=true;completed++;this_call++;
 }
 if(completed==cfg.max_cases||completed==choices.size()){
  if(auto d=detect(store,cfg)){
   store.hypothesis("H1","HYPOTHESIZED",cfg.axis,d->left,d->right,d->effect,"Exploratory candidate, one frozen hypothesis with independent holdout; conditioned scope="+d->scope+"; failure_status="+d->failure_status,d->kind,d->frozen_params);
  }else store.hypothesis("H1","NO_CONFIRMED_BOUNDARY",cfg.axis,0,0,0,"No convincing isolated structural discontinuity under declared thresholds");
  store.event("search_complete",std::to_string(completed));
 }else store.event("search_paused",std::to_string(completed));
 std::cout<<"Explored "<<completed<<" / "<<std::min<std::size_t>(cfg.max_cases,choices.size())<<" cases; "<<store.summary().str()<<"\n";
 report(out);
}
void explore(const Config& cfg,const std::string& out,std::size_t pause_after){
 if(fs::exists(out+"/run.sqlite"))throw Error("output run already exists (will not overwrite evidence)");
 fs::create_directories(out);Store s(out+"/run.sqlite");s.experiment(config_json(cfg).str(),environment());
 execute_schedule(cfg,out,s,pause_after);
}
void resume(const std::string& run,std::size_t pause_after){
 if(!fs::exists(run+"/run.sqlite"))throw Error("run database missing for resume");
 Store s(run+"/run.sqlite");const auto cfg=config_from_json(Json::parse(s.config()));
 sqlite3_stmt* st=nullptr;
 if(sqlite3_prepare_v2(s.raw(),"SELECT COUNT(*) FROM hypotheses",-1,&st,nullptr)!=SQLITE_OK)throw Error("hypothesis lookup failed");
 auto rc=sqlite3_step(st);const auto done=rc==SQLITE_ROW?sqlite3_column_int64(st,0):0;sqlite3_finalize(st);
 if(done)throw Error("experiment has finalized hypothesis; resume would overwrite evidence");
 execute_schedule(cfg,run,s,pause_after);
}
static std::tuple<std::string,std::string,std::int64_t,std::int64_t> read_hypothesis(Store& s,const std::string& id){
 sqlite3_stmt* st=nullptr;if(sqlite3_prepare_v2(s.raw(),"SELECT status,axis,left_value,right_value FROM hypotheses WHERE id=?",-1,&st,nullptr)!=SQLITE_OK)throw Error("hypothesis query failed");
 sqlite3_bind_text(st,1,id.c_str(),-1,SQLITE_TRANSIENT);if(sqlite3_step(st)!=SQLITE_ROW){sqlite3_finalize(st);throw Error("unknown hypothesis "+id);}
 std::string status=reinterpret_cast<const char*>(sqlite3_column_text(st,0)),axis=reinterpret_cast<const char*>(sqlite3_column_text(st,1));auto l=sqlite3_column_int64(st,2),r=sqlite3_column_int64(st,3);sqlite3_finalize(st);return {status,axis,l,r};
}
static Case frozen_case(Store& store,const Config& cfg,const std::string& id){
 Case frozen=cfg.base;
 sqlite3_stmt* st=nullptr;
 if(sqlite3_prepare_v2(store.raw(),"SELECT frozen_params FROM hypotheses WHERE id=?",-1,&st,nullptr)!=SQLITE_OK)
  throw Error("frozen hypothesis query failed");
 sqlite3_bind_text(st,1,id.c_str(),-1,SQLITE_TRANSIENT);
 const auto rc=sqlite3_step(st);
 if(rc!=SQLITE_ROW){sqlite3_finalize(st);throw Error("frozen hypothesis missing");}
 const auto* p=sqlite3_column_text(st,0);
 const auto serialized=std::string(p?reinterpret_cast<const char*>(p):"");
 sqlite3_finalize(st);
 if(!serialized.empty()){
  auto j=Json::parse(serialized);
  json_object_object_foreach(j.p,key,value){apply_field(frozen,key,value);}
 }
 return frozen;
}
void confirm(const std::string& run,const std::string& id){
 Store store(run+"/run.sqlite");const auto cfg=config_from_json(Json::parse(store.config()));
 auto [status,axis,left,right]=read_hypothesis(store,id);
 const auto frozen=frozen_case(store,cfg,id);
 if(status=="NO_CONFIRMED_BOUNDARY"){std::cout<<"No candidate to confirm\n";return;}
 if(status!="HYPOTHESIZED")throw Error("confirmation already attempted or candidate not eligible: "+status);
 sqlite3_stmt* kind_stmt=nullptr;
 if(sqlite3_prepare_v2(store.raw(),"SELECT kind FROM hypotheses WHERE id=?",-1,&kind_stmt,nullptr)!=SQLITE_OK)throw Error("hypothesis kind query failed");
 sqlite3_bind_text(kind_stmt,1,id.c_str(),-1,SQLITE_TRANSIENT);
 if(sqlite3_step(kind_stmt)!=SQLITE_ROW){sqlite3_finalize(kind_stmt);throw Error("hypothesis not found");}
 const auto kind=std::string(reinterpret_cast<const char*>(sqlite3_column_text(kind_stmt,0)));
 sqlite3_finalize(kind_stmt);
 if(kind=="CORRECTNESS_BOUNDARY"||kind=="MEMORY_OR_RESOURCE_BOUNDARY"){
  sqlite3_stmt* description=nullptr;
  if(sqlite3_prepare_v2(store.raw(),"SELECT detail FROM hypotheses WHERE id=?",-1,&description,nullptr)!=SQLITE_OK)throw Error("failure subtype query failed");
  sqlite3_bind_text(description,1,id.c_str(),-1,SQLITE_TRANSIENT);
  if(sqlite3_step(description)!=SQLITE_ROW){sqlite3_finalize(description);throw Error("failure subtype missing");}
  const auto detail=std::string(reinterpret_cast<const char*>(sqlite3_column_text(description,0)));
  sqlite3_finalize(description);
  const auto tag=std::string("failure_status=");const auto where=detail.find(tag);
  if(where==std::string::npos)throw Error("hypothesis has no frozen failure status");
  auto subtype=detail.substr(where+tag.size());if(const auto end=subtype.find(';');end!=std::string::npos)subtype.resize(end);
  const bool permitted=kind=="CORRECTNESS_BOUNDARY"?
    (subtype=="wrong_answer"||subtype=="signaled"||subtype=="crashed"):
    (subtype=="timeout"||subtype=="out_of_memory");
  if(!permitted)throw Error("hypothesis subtype conflicts with frozen category");
  std::set<std::string> prior;for(const auto& c:store.cases(false))prior.insert(c.id);
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(cfg.total_wall_s);
  bool all_valid=true;int completed_pairs=0;
  for(int i=0;i<cfg.confirm_blocks&&all_valid;i++){
   const auto seed=cfg.seed+7000000ULL+static_cast<std::uint64_t>(i)*101ULL;
   for(int side=0;side<2;side++){
    if(std::chrono::steady_clock::now()>deadline){all_valid=false;break;}
    Case c=frozen;assign(c,axis,formatted_axis_value(axis,side==0?left:right));
    c.seed=seed;c=make_case(c,cfg.max_bytes);
    if(prior.count(c.id)){all_valid=false;break;}
    auto observations=measure(c,cfg.b.empty()?std::vector<std::string>{cfg.a}:
       std::vector<std::string>{cfg.a,cfg.b},1,cfg.warmup,cfg.sample_ms,cfg.rss_mb);
    int count=0;
    for(const auto& observation:observations){
     store.add_observation(c,observation,true);
     if(observation.algorithm==cfg.a){
      ++count;
      if(observation.status!=(side==0?"ok":subtype))all_valid=false;
     }else if(observation.algorithm==cfg.b&&observation.status!="ok")all_valid=false;
    }
    if(count<1)all_valid=false;
   }
   if(all_valid)++completed_pairs;
  }
  const auto verdict=all_valid&&completed_pairs==cfg.confirm_blocks?"CONFIRMED":"NOT_CONFIRMED";
  const auto evidence="Fresh seed-separated failure-status cases="+std::to_string(completed_pairs)+
      " pairs expected="+std::to_string(cfg.confirm_blocks)+"; left=ok right="+subtype+
      "; categorical status, censored timing not a speedup; no finite ratio inferred";
  store.hypothesis(id,verdict,axis,left,right,0,evidence);
  report(run);std::cout<<verdict<<" "<<evidence<<"\n";return;
 }
 std::array<std::vector<double>,2> side_estimates;
 std::vector<double> paired_differences;
 const auto begin=std::chrono::steady_clock::now();
 std::set<std::string> discovery_ids;for(const auto& c:store.cases(false))discovery_ids.insert(c.id);
 auto reject=[&](const std::string& why){store.hypothesis(id,"NOT_CONFIRMED",axis,left,right,0,why);report(run);};
 for(int i=0;i<cfg.confirm_blocks;i++){
  const auto seed=cfg.seed+1000000ULL+static_cast<std::uint64_t>(i)*97ULL;
  std::array<double,2> estimates{};
  for(int order=0;order<2;order++){
   const auto side=static_cast<std::size_t>((i+order)%2);
   if(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-begin).count()>=cfg.total_wall_s){reject("Holdout stopped by declared wall budget");return;}
   Case c=frozen;assign(c,axis,formatted_axis_value(axis,side==0?left:right));c.seed=seed;c=make_case(c,cfg.max_bytes);
   if(discovery_ids.count(c.id)){reject("Held-out input overlaps discovery: confirmation invalid");return;}
   auto obs=measure(c,cfg.b.empty()?std::vector<std::string>{cfg.a}:std::vector<std::string>{cfg.a,cfg.b},1,cfg.warmup,cfg.sample_ms,cfg.rss_mb);
   for(const auto& o:obs)store.add_observation(c,o,true);
   auto result=metric(store,c,cfg,true);
   if(!result){reject("Insufficient holdout measurement or correctness/worker failure");return;}
   estimates[side]=*result;
  }
  side_estimates[0].push_back(estimates[0]);side_estimates[1].push_back(estimates[1]);
  paired_differences.push_back(estimates[1]-estimates[0]);
 }
 const auto delta=sample_median(paired_differences);
 const auto threshold=std::log(cfg.min_ratio);
 std::ostringstream detail;detail<<std::setprecision(8);
 bool confirmed=false;
 if(cfg.family=="calibration"){
  confirmed=std::abs(delta)>=threshold;
  detail<<"Independent seed-separated deterministic operation-count holdout; pairs="<<cfg.confirm_blocks<<". NOT a timing/algorithm discovery.";
 }else{
  const auto interval=paired_case_bootstrap(paired_differences,cfg.seed^0x5b86d2a7c9e3ULL,cfg.confirmation_confidence);
  const auto ci_l=case_bootstrap(side_estimates[0],cfg.seed^0x7ac6314fe29ULL,cfg.confirmation_confidence);
  const auto ci_r=case_bootstrap(side_estimates[1],cfg.seed^0x87f3e195c1eULL,cfg.confirmation_confidence);
  const auto p=paired_sign_flip_p(paired_differences,cfg.seed^0xb3970a42e86ULL);
  /* One preselected hypothesis per discovery, one isolated single-use
     confirmation: BH for a family of size one is identical to p<=q.
     A 99% CI further constrains the practical effect. */
  const bool significant=bh_fdr({p},cfg.fdr_q)[0]; /* vector<bool> proxy must not outlive temporary */
  const auto practical=interval.lower>threshold||interval.upper< -threshold;
  const auto signed_sides=(ci_l.upper< -std::log(1.02)&&ci_r.lower>std::log(1.02))||
                           (ci_r.upper< -std::log(1.02)&&ci_l.lower>std::log(1.02));
  confirmed=significant&&practical&&(kind!="PERFORMANCE_CROSSOVER"||signed_sides);
  detail<<cfg.confirmation_confidence*100.0<<"% seed-matched case-pair bootstrap (n="<<cfg.confirm_blocks<<") delta log metric CI=["
        <<interval.lower<<","<<interval.upper<<"] p_signflip="<<p
        <<"; BH-FDR one frozen hypothesis q="<<cfg.fdr_q<<"; left CI=["<<ci_l.lower<<","<<ci_l.upper
        <<"] right CI=["<<ci_r.lower<<","<<ci_r.upper<<"]";
  detail<<"; exploratory selection independent of holdout, not causal proof";
 }
 /* Distant negative-control morphology: preserve the observation even when
    timing too noisy to classify the control on its own. */
 if(cfg.family!="calibration"&&cfg.axes.count(axis)){
  auto values=cfg.axes.at(axis);std::vector<std::int64_t> numeric;
  for(const auto& value:values){
   try{
    if(axis=="density"||axis=="degree_skew"||axis=="duplicate_fraction"||axis=="disorder_fraction"||axis=="sign_fraction")numeric.push_back(parse_ppm(value));
    else {std::size_t end=0;const auto x=std::stoll(value,&end);if(end==value.size())numeric.push_back(x);}
   }catch(const std::exception&){}
  }
  if(!numeric.empty()){
   const auto lo=*std::min_element(numeric.begin(),numeric.end()),hi=*std::max_element(numeric.begin(),numeric.end());
   const auto control=std::abs(lo-left)>std::abs(hi-right)?lo:hi;
   if(control!=left&&control!=right){Case c=frozen;assign(c,axis,formatted_axis_value(axis,control));c.seed=cfg.seed+2000000ULL;c=make_case(c,cfg.max_bytes);
    for(const auto& o:measure(c,cfg.b.empty()?std::vector<std::string>{cfg.a}:std::vector<std::string>{cfg.a,cfg.b},1,cfg.warmup,cfg.sample_ms,cfg.rss_mb))store.add_observation(c,o,true);
    detail<<"; negative-control axis="<<control<<" case="<<c.id;
   }
  }
 }
 store.hypothesis(id,confirmed?"CONFIRMED":"NOT_CONFIRMED",axis,left,right,std::abs(delta),detail.str()+(confirmed?"":"; prespecified evidence threshold not met"));
 report(run);std::cout<<store.summary().str()<<"\n";
}
void ablate(const Config& cfg,const std::string& out){
 if(!numeric_axis(cfg.axis))throw Error("ablation requires an ordered numeric primary axis; categorical axes must be conditioned on");
 if(fs::exists(out))throw Error("ablation output already exists");
 fs::create_directories(out);
 std::ostringstream csv;csv<<"method,case_budget,discovery_cases,total_cases,raw_samples,seconds,status,left,right,localization_error\n";
 for(const char* name:{"grid","random","stratified","adaptive_partition"}){
  auto c=cfg;c.method=name;const auto path=out+"/"+name;
  const auto start=std::chrono::steady_clock::now();explore(c,path);
  Store store(path+"/run.sqlite");auto [status,axis,left,right]=read_hypothesis(store,"H1");(void)axis;
  if(status=="HYPOTHESIZED") {confirm(path);std::tie(status,axis,left,right)=read_hypothesis(store,"H1");}
  const auto sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();auto summary=store.summary();
  sqlite3_stmt* st=nullptr;
  if(sqlite3_prepare_v2(store.raw(),"SELECT COUNT(DISTINCT case_id) FROM samples WHERE holdout=0",-1,&st,nullptr)!=SQLITE_OK)throw Error("ablation discovery count query");
  if(sqlite3_step(st)!=SQLITE_ROW){sqlite3_finalize(st);throw Error("ablation discovery count missing");}
  const auto discovery_cases=sqlite3_column_int64(st,0);sqlite3_finalize(st);
  const auto err=cfg.family=="calibration"&&status=="CONFIRMED"?std::to_string(std::abs(right-c.base.threshold)):"NA";
  csv<<name<<','<<c.max_cases<<','<<discovery_cases<<','<<summary.i("cases")<<','<<summary.i("samples")<<','<<sec<<','<<status<<','<<left<<','<<right<<','<<err<<'\n';
 }
 write_file(out+"/ablation.csv",csv.str());
 write_file(out+"/README.md","# Equal-budget search comparison\n\nRows summarize grid, random, stratified and adaptive runs with identical user-specified evaluation and wall budgets. `localization_error=NA` for real algorithms because no ground-truth boundary is assumed. Rows include actual discovery case counts to expose incomplete runs; equal requested budgets do not guarantee equal completed counts. All raw cases and timing measurements remain in each method subdirectory.\n");
 std::cout<<csv.str();
}
/* A performance witness is a *measured* predicate, unlike a wrong-answer
 * witness. Candidate selection and final validation use disjoint independent
 * seeds, and the original independent confirmation is never recycled. Raw
 * minimization observations live in a dedicated database to keep the original
 * exploration/confirmation populations immutable and auditable. */
static void minimize_performance(const std::string& run,const std::string& id,
                                 Store& source,const Config& cfg,
                                 const std::string& axis,std::int64_t left,std::int64_t right){
 if(fs::exists(run+"/minimization.sqlite")||fs::exists(run+"/witness.json"))
  throw Error("minimization evidence already exists; refusing to overwrite");
 const auto base=frozen_case(source,cfg,id);
 if(!numeric_axis(axis)||left>=right)throw Error("performance minimization requires a frozen numeric boundary");
 Case initial=base;assign(initial,axis,formatted_axis_value(axis,right));
 initial.seed=cfg.seed+50000000ULL;
 initial=make_case(initial,cfg.max_bytes);
 const auto scope=scope_key(initial,cfg);
 std::vector<double> exploration_left,exploration_right;
 for(const auto& observed:source.cases(false)){
  if(scope_key(observed,cfg)!=scope)continue;
  const auto x=axis_value(observed,axis);
  if(x!=left&&x!=right)continue;
  if(const auto value=metric(source,observed,cfg,false)){
   if(x==left)exploration_left.push_back(*value);
   else exploration_right.push_back(*value);
  }
 }
 if(exploration_left.empty()||exploration_right.empty())throw Error("cannot recover pre-minimization effect direction");
 const auto exploratory_change=sample_median(exploration_right)-sample_median(exploration_left);
 if(exploratory_change==0)throw Error("original boundary has no nonzero effect direction");
 const int expected_direction=exploratory_change>0?1:-1;
 sqlite3_stmt* statement=nullptr;
 if(sqlite3_prepare_v2(source.raw(),"SELECT kind FROM hypotheses WHERE id=?",-1,&statement,nullptr)!=SQLITE_OK)throw Error("minimization kind lookup failed");
 sqlite3_bind_text(statement,1,id.c_str(),-1,SQLITE_TRANSIENT);
 if(sqlite3_step(statement)!=SQLITE_ROW){sqlite3_finalize(statement);throw Error("hypothesis unavailable to minimizer");}
 const auto kind=std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement,0)));
 sqlite3_finalize(statement);
 Store evidence(run+"/minimization.sqlite");evidence.experiment(config_json(cfg).str(),environment());
 evidence.event("frozen_witness_predicate", "id="+id+" kind="+kind+" axis="+axis+" left="+std::to_string(left)+" right="+std::to_string(right)+" effect_direction="+std::to_string(expected_direction));
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(std::min(180,cfg.total_wall_s));
 const int samples=std::max(20,cfg.confirm_blocks);
 std::size_t attempts=0,accepted=0;
 auto check=[&](const Case& candidate,const std::string& stage)->bool{
  ++attempts;
  if(axis_value(candidate,axis)!=right || scope_key(candidate,cfg)!=scope)return false;
  std::array<std::vector<double>,2> sides;
  std::vector<double> differences;
  /* Unique seed blocks per attempted candidate/stage: no data re-use for
     selecting, testing and final re-confirming a minimized input. */
  const auto base_seed=cfg.seed+50000000ULL+static_cast<std::uint64_t>(attempts)*100003ULL;
  for(int block=0;block<samples;block++){
   if(std::chrono::steady_clock::now()>=deadline){evidence.event("minimization_budget",stage+" timed out before complete holdout");return false;}
   std::array<double,2> estimates{};
   for(int step=0;step<2;step++){
    const auto side=static_cast<std::size_t>((block+step)%2);
    Case c=candidate;assign(c,axis,formatted_axis_value(axis,side==0?left:right));
    c.seed=base_seed+static_cast<std::uint64_t>(block)*97ULL;
    try{c=make_case(c,cfg.max_bytes);}catch(const Error&){return false;}
    auto observations=measure(c,cfg.b.empty()?std::vector<std::string>{cfg.a}:std::vector<std::string>{cfg.a,cfg.b},
                              1,cfg.warmup,cfg.sample_ms,cfg.rss_mb);
    for(const auto& o:observations)evidence.add_observation(c,o,true);
    const auto m=metric(evidence,c,cfg,true);
    if(!m){evidence.event("minimization_invalid",stage+" worker or oracle invalid case="+c.id);return false;}
    estimates[side]=*m;
   }
   sides[0].push_back(estimates[0]);sides[1].push_back(estimates[1]);
   differences.push_back(estimates[1]-estimates[0]);
  }
  const auto confidence=paired_case_bootstrap(differences,base_seed^0xadd61317aULL,0.99);
  const auto p=paired_sign_flip_p(differences,base_seed^0xff197e1ULL);
  const auto threshold=std::log(cfg.min_ratio);
  const bool positive=expected_direction>0;
  const bool practical=positive?confidence.lower>threshold:confidence.upper< -threshold;
  bool crosses=true;
  if(kind=="PERFORMANCE_CROSSOVER"){
   const auto l=case_bootstrap(sides[0],base_seed^0x17399ULL,0.99);
   const auto r=case_bootstrap(sides[1],base_seed^0x1cc7ULL,0.99);
   crosses=positive?l.upper< -std::log(1.02)&&r.lower>std::log(1.02):
                    r.upper< -std::log(1.02)&&l.lower>std::log(1.02);
  }
  const bool preserved=practical&&p<=0.01&&crosses;
  std::ostringstream explanation;explanation<<std::setprecision(10)<<stage<<" candidate="<<candidate.id
     <<" n="<<differences.size()<<" delta="<<sample_median(differences)
     <<" ci99=["<<confidence.lower<<","<<confidence.upper<<"] sign_flip_p="<<p
     <<" expected_sign="<<expected_direction<<" preserved="<<preserved;
  evidence.event("minimization_candidate",explanation.str());
  if(preserved)++accepted;
  return preserved;
 };
 ShrinkResult result;result.original=initial;result.smallest=initial;
 bool initially_valid=false,finally_valid=false;
 try{
  /* The original witness must first survive an independent, fresh
     correctness/statistical check before any morphology modification. */
  initially_valid=check(initial,"baseline_recheck");
  if(initially_valid){
   /* Bound evaluations and wall time. Every candidate consumes its own
      fresh, independent set of paired case-level observations. */
   result=shrink_case(initial,[&](const Case& c){return check(c,"candidate_selection");},12,cfg.max_bytes);
   finally_valid=check(result.smallest,"final_independent_reconfirmation");
  }
 }catch(const std::exception& error){
  evidence.event("minimization_error",error.what());
 }
 const auto& smallest=initially_valid?result.smallest:initial;
 const auto status=finally_valid&&result.reduced?"MINIMIZED_CONFIRMED":
                   finally_valid?"MINIMIZATION_FAILED":"MINIMIZED_PROVISIONAL";
 Json record;record.put("schema_version",static_cast<std::int64_t>(1));record.put("category","PERFORMANCE_WITNESS");
 record.put("status",status);record.put("hypothesis_id",id);record.put("axis",axis);
 record.put("left",left);record.put("right",right);
 record.put("kind",kind);record.put("effect_direction",static_cast<std::int64_t>(expected_direction));
 record.put("baseline_independently_valid",initially_valid);record.put("final_independently_valid",finally_valid);
 record.put("original_case_id",initial.id);record.put("case_id",smallest.id);
 record.put("original_input_bytes",static_cast<std::int64_t>(initial.bytes.size()));
 record.put("input_bytes",static_cast<std::int64_t>(smallest.bytes.size()));
 record.put("input_sha256",sha256(smallest.bytes));record.put("parameters",Json::parse(case_params(smallest)));
 record.put("evaluations",static_cast<std::int64_t>(attempts));record.put("valid_experiments",static_cast<std::int64_t>(accepted));
 record.put("holdout_case_pairs_per_evaluation",static_cast<std::int64_t>(samples));
 record.put("minimization_evidence","minimization.sqlite");
 auto* path=json_object_new_array();
 if(initially_valid)for(const auto& step:result.trace){Json item;item.put("case_id",step.case_id);
  item.put("input_bytes",static_cast<std::int64_t>(step.input_bytes));item.put("action",step.action);
  item.put("preserved",step.predicate_preserved);json_object_array_add(path,item.p);item.p=nullptr;}
 json_object_object_add(record.p,"shrink_trace",path);
 source.add_case(smallest);source.witness(id,smallest.id,status,record.str());
 source.hypothesis(id,status,axis,left,right,0, "Performance witness "+std::string(status)+"; independent minimization evidence in minimization.sqlite; raw observation and null outcomes retained");
 write_file(run+"/witness.json",record.str());write_file(run+"/witness.hex",hex_encode(smallest.bytes)+"\n");
 report(run);
 std::cout<<"Performance witness "<<status<<" case="<<smallest.id<<" evaluated="<<attempts<<"\n";
}

void minimize(const std::string& run,const std::string& id){
 Store s(run+"/run.sqlite");auto cfg=config_from_json(Json::parse(s.config()));auto [status,axis,left,right]=read_hypothesis(s,id);
 if(status!="CONFIRMED")throw Error("only confirmed hypotheses can be minimized");
 if(cfg.family!="calibration"||axis!="x"){
  sqlite3_stmt* stmt=nullptr;
  if(sqlite3_prepare_v2(s.raw(),"SELECT kind FROM hypotheses WHERE id=?",-1,&stmt,nullptr)!=SQLITE_OK)throw Error("hypothesis kind lookup failed");
  sqlite3_bind_text(stmt,1,id.c_str(),-1,SQLITE_TRANSIENT);
  if(sqlite3_step(stmt)!=SQLITE_ROW){sqlite3_finalize(stmt);throw Error("missing hypothesis kind");}
  const std::string kind(reinterpret_cast<const char*>(sqlite3_column_text(stmt,0)));
  sqlite3_finalize(stmt);
  if(kind=="CORRECTNESS_BOUNDARY"||kind=="MEMORY_OR_RESOURCE_BOUNDARY"){
   const auto scope=scope_key(frozen_case(s,cfg,id),cfg);
   std::optional<Case> original;
   std::string subtype;
   for(const auto& candidate:s.cases(false)){
    if(axis_value(candidate,axis)!=right||scope_key(candidate,cfg)!=scope)continue;
    for(const auto& observation:s.observations(candidate.id,false)){
     if(observation.algorithm!=cfg.a||observation.status=="ok")continue;
     original=candidate;subtype=observation.status;break;
    }
    if(original)break;
   }
   if(!original)throw Error("confirmed failure has no persisted failing case");
   if(subtype=="wrong_answer"&&kind=="CORRECTNESS_BOUNDARY"){
    minimize_failure_case(run,original->id,cfg.a);
    const auto record=Json::file(run+"/witness.json");
    const auto verdict=record.s("status");
    s.witness(id,record.s("case_id"),verdict,record.str());
    s.hypothesis(id,verdict,axis,left,right,0,"Freshly replayed wrong-answer witness and shrink trace; earlier independent confirmation retained");
    report(run);return;
   }
   /* Crash/timeout/OOM witnesses require exact failure-mode preservation.
      Never turn censored failures into synthetic large timing values. */
   if(fs::exists(run+"/witness.json"))throw Error("witness artifacts already exist");
   const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(std::min(120,cfg.total_wall_s));
   auto predicate=[&](const Case& input)->bool{
    if(std::chrono::steady_clock::now()>deadline)throw Error("failure shrink time budget exhausted");
    const auto obs=measure(input,{cfg.a},1,0,cfg.sample_ms,cfg.rss_mb);
    return obs.size()==1&&obs[0].status==subtype;
   };
   ShrinkResult shrunk;shrunk.original=*original;shrunk.smallest=*original;
   bool initially_valid=false,final_valid=false;
   try{
    initially_valid=predicate(*original);
    if(initially_valid){
     shrunk=shrink_case(*original,predicate,24,cfg.max_bytes);
     final_valid=true;for(int i=0;i<3;i++)if(!predicate(shrunk.smallest)){final_valid=false;break;}
    }
   }catch(const std::exception& e){s.event("failure_witness_error",e.what());}
   const auto verdict=final_valid&&shrunk.reduced?"MINIMIZED_CONFIRMED":
                      final_valid?"MINIMIZATION_FAILED":"MINIMIZED_PROVISIONAL";
   Json record;record.put("schema_version",static_cast<std::int64_t>(1));record.put("category",kind);
   record.put("algorithm",cfg.a);record.put("status",verdict);record.put("expected_status",subtype);
   record.put("original_case_id",original->id);record.put("case_id",shrunk.smallest.id);
   record.put("input_sha256",sha256(shrunk.smallest.bytes));record.put("parameters",Json::parse(case_params(shrunk.smallest)));
   record.put("original_input_bytes",static_cast<std::int64_t>(original->bytes.size()));
   record.put("input_bytes",static_cast<std::int64_t>(shrunk.smallest.bytes.size()));
   record.put("evaluations",static_cast<std::int64_t>(shrunk.evaluations));
   record.put("baseline_rechecked",initially_valid);record.put("final_rechecked",final_valid);
   auto* trace=json_object_new_array();for(const auto& step:shrunk.trace){Json item;item.put("case_id",step.case_id);
    item.put("action",step.action);item.put("input_bytes",static_cast<std::int64_t>(step.input_bytes));
    item.put("preserved",step.predicate_preserved);json_object_array_add(trace,item.p);item.p=nullptr;}
   json_object_object_add(record.p,"shrink_trace",trace);
   s.add_case(shrunk.smallest);s.witness(id,shrunk.smallest.id,verdict,record.str());
   s.hypothesis(id,verdict,axis,left,right,0,"Exact "+subtype+" failure witness "+verdict+"; censored, not ranked as timing");
   write_file(run+"/witness.json",record.str());write_file(run+"/witness.hex",hex_encode(shrunk.smallest.bytes)+"\n");
   report(run);return;
  }
  minimize_performance(run,id,s,cfg,axis,left,right);return;
 }
 Case c=frozen_case(s,cfg,id);c.x=right;c.seed=cfg.seed+2000000;c=make_case(c,cfg.max_bytes);
 const auto w=execute(c,"calibration").work;
 if(c.x<cfg.base.threshold||w!=static_cast<std::uint64_t>(c.x+10000))throw Error("synthetic preservation predicate failed");
 /* For the controlled fixture, right is the first observed threshold bin, so the
    minimal witness on the declared grid is x=threshold. */
 auto left_w=static_cast<std::uint64_t>(left+10);
 if(right!=cfg.base.threshold||left>=right||left_w==w)throw Error("candidate not an exact minimal synthetic boundary");
 s.add_case(c);s.witness(id,c.id,"MINIMIZED_CONFIRMED","Parameter minimization: threshold equals smallest value on high-work side; oracle revalidated");
 std::ostringstream o;o<<"#include <cstdint>\n#include <cassert>\nint main() { constexpr std::int64_t x="<<right<<", threshold="<<c.threshold<<"; const auto work=x<threshold?x+10:x+10000; assert(x==threshold && work=="<<w<<"); }\n";
 write_file(run+"/witness.cpp",o.str());write_file(run+"/witness.hex",hex_encode(c.bytes)+"\n");
 s.hypothesis(id,"MINIMIZED_CONFIRMED",axis,left,right,static_cast<double>(w),"Exact synthetic threshold witness, reproducible input and standalone C++ assertion");report(run);
 std::cout<<"Minimal fixture witness: x="<<right<<" case="<<c.id<<"\n";
}
static Case locate_case(Store& store,const std::string& identity){
 std::optional<Case> selected;
 for(const auto& c:store.cases())if(c.id==identity||c.id.rfind(identity,0)==0){
  if(selected)throw Error("ambiguous witness case-id prefix; supply more characters");
  selected=c;
 }
 if(!selected)throw Error("witness case ID not in run");
 return *selected;
}
void minimize_failure_case(const std::string& run,const std::string& case_id,const std::string& algorithm){
 if(algorithm.empty())throw Error("minimize-case requires an explicit algorithm");
 Store store(run+"/run.sqlite");const auto cfg=config_from_json(Json::parse(store.config()));
 const auto origin=locate_case(store,case_id);
 bool observed=false;
 for(const auto& o:store.observations(origin.id,false))
  if(o.algorithm==algorithm&&(o.status=="wrong_answer"||o.status=="signaled"||o.status=="crashed"))observed=true;
 if(!observed)throw Error("cannot shrink unrecorded or successful correctness case");
 if(fs::exists(run+"/witness.json")||fs::exists(run+"/witness.hex"))throw Error("witness artifacts already exist; refusing to overwrite");
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(std::min(cfg.total_wall_s,120));
 auto predicate=[&](const Case& candidate){
  if(std::chrono::steady_clock::now()>deadline)throw Error("witness minimization wall budget exhausted");
  /* Worker validation includes oracle comparison before collecting timing. */
  const auto result=measure(candidate,{algorithm},1,0,cfg.sample_ms,cfg.rss_mb);
  return result.size()==1&&result[0].status=="wrong_answer";
 };
 /* A crash is NOT automatically a wrong answer: only repeatable semantic
    mismatches are accepted by this correctness minimizer. */
 bool old_wrong_answer=false;
 for(const auto& o:store.observations(origin.id,false))if(o.algorithm==algorithm&&o.status=="wrong_answer")old_wrong_answer=true;
 if(!old_wrong_answer)throw Error("crashes and resource failures require distinct preservation predicates; refusing wrong-answer minimization");
 auto result=shrink_case(origin,predicate,32,cfg.max_bytes);
 bool repeatable=true;
 for(int i=0;i<3;i++)if(!predicate(result.smallest)){repeatable=false;break;}
 Json record;record.put("schema_version",static_cast<std::int64_t>(1));record.put("category","CORRECTNESS_BOUNDARY");
 record.put("original_case_id",result.original.id);record.put("case_id",result.smallest.id);
 record.put("algorithm",algorithm);record.put("input_sha256",sha256(result.smallest.bytes));
 record.put("parameters",Json::parse(case_params(result.smallest)));
 record.put("input_bytes",static_cast<std::int64_t>(result.smallest.bytes.size()));
 record.put("evaluations",static_cast<std::int64_t>(result.evaluations));
 record.put("original_input_bytes",static_cast<std::int64_t>(result.original.bytes.size()));
 const auto verdict=repeatable&&result.reduced?"MINIMIZED_CONFIRMED":repeatable?"MINIMIZATION_FAILED":"MINIMIZED_PROVISIONAL";
 record.put("status",verdict);record.put("verification_repeats",static_cast<std::int64_t>(3));
 auto* trace=json_object_new_array();for(const auto& step:result.trace){Json event;event.put("case_id",step.case_id);
  event.put("input_bytes",static_cast<std::int64_t>(step.input_bytes));event.put("action",step.action);event.put("preserved",step.predicate_preserved);
  json_object_array_add(trace,event.p);event.p=nullptr;}
 json_object_object_add(record.p,"shrink_trace",trace);
 record.put("replay_command","phaseshift replay --run <RUN> --case "+result.smallest.id+" --algorithm <PLUGIN> --expect wrong_answer");
 store.add_case(result.smallest);
 store.event("witness_correctness",record.str());
 write_file(run+"/witness.json",record.str());write_file(run+"/witness.hex",hex_encode(result.smallest.bytes)+"\n");
 report(run);
 std::cout<<"Correctness witness "<<verdict<<" case="<<result.smallest.id<<" original="<<origin.bytes.size()
          <<" bytes minimal="<<result.smallest.bytes.size()<<" bytes\n";
}
void replay_expected_failure(const std::string& run,const std::string& case_id,const std::string& algorithm,const std::string& expected_status){
 static const std::set<std::string> accepted={"wrong_answer","timeout","out_of_memory","crashed","signaled"};
 if(!accepted.count(expected_status)||algorithm.empty())throw Error("expected-failure replay requires an explicit algorithm and supported status");
 Store store(run+"/run.sqlite");auto c=locate_case(store,case_id);
 for(const auto& o:measure(c,{algorithm},1,0,3000,1024)){
  if(o.status!=expected_status)throw Error("witness failure did not reproduce: expected "+expected_status+" got "+o.status);
  std::cout<<"REPLAY EXPECTED_FAILURE status="<<o.status<<" case="<<c.id<<" input_sha256="<<sha256(c.bytes)<<"\n";
  return;
 }
 throw Error("empty witness replay");
}
void replay(const std::string& run,const std::string& case_id){
 Store s(run+"/run.sqlite");auto cfg=config_from_json(Json::parse(s.config()));
 for(const auto& c:s.cases())if(c.id==case_id||c.id.rfind(case_id,0)==0){
  const auto expected=oracle(c).values;const auto actual=execute(c,cfg.a).values;if(actual!=expected)throw Error("replay correctness failure");
  std::cout<<"REPLAY OK case="<<c.id<<" input_sha256="<<sha256(c.bytes)<<" algorithm="<<cfg.a<<"\n";return;
 }
 throw Error("replay case not found");
}
static std::string csv_escape(const std::string& s){std::string o="\"";for(char c:s){if(c=='\"')o+="\"\"";else o+=c;}return o+"\"";}
static std::string html_escape(const std::string& s){std::string o;for(char c:s){if(c=='&')o+="&amp;";else if(c=='<')o+="&lt;";else if(c=='>')o+="&gt;";else if(c=='\"')o+="&quot;";else o+=c;}return o;}
void report(const std::string& run){
 Store s(run+"/run.sqlite");s.refresh_aggregates();auto summary=s.summary();auto cfg=config_from_json(Json::parse(s.config()));
 Json report;report.put("schema_version",static_cast<std::int64_t>(1));report.put("experiment_id",cfg.experiment_id);report.put("family",cfg.family);report.put("algorithm_a",cfg.a);report.put("algorithm_b",cfg.b);report.put("environment",Json::parse(s.environment()));report.put("summary",std::move(summary));
 auto* raw=json_object_new_array();std::ostringstream csv;csv<<"case_id,family,axis_value,holdout,algorithm,status,batch_ns,batch_iterations,work_units,block,cpu_ns,peak_rss_bytes,order_position,order_seed,output_digest,detail\n";
 std::uint64_t raw_size=0;bool truncated=false;
 const auto export_limit=cfg.max_report_bytes/2;
 for(const auto& c:s.cases())for(int holdout=0;holdout<2;holdout++)for(const auto& o:s.observations(c.id,holdout!=0)){
  if(truncated)continue;
  Json row;row.put("case_id",c.id);row.put("family",c.family);row.put("observed_features",morphology_features(c));if(numeric_axis(cfg.axis))row.put("axis_value",axis_value(c,cfg.axis));
  else row.put("axis_value",axis_label(c,cfg.axis));
  row.put("holdout",holdout!=0);row.put("algorithm",o.algorithm);row.put("status",o.status);row.put("batch_ns",o.ns);row.put("batch_iterations",o.batch);row.put("work_units",o.work);row.put("block",o.block);
  if(o.cpu_ns>=0)row.put("cpu_ns",o.cpu_ns);else json_object_object_add(row.p,"cpu_ns",nullptr);
  if(o.peak_rss_bytes>=0)row.put("peak_rss_bytes",o.peak_rss_bytes);else json_object_object_add(row.p,"peak_rss_bytes",nullptr);
  if(o.order_position>=0)row.put("order_position",o.order_position);else json_object_object_add(row.p,"order_position",nullptr);
  if(!o.order_seed.empty())row.put("order_seed",o.order_seed);else json_object_object_add(row.p,"order_seed",nullptr);
  json_object_object_add(row.p,"cycles",nullptr);json_object_object_add(row.p,"instructions",nullptr);
  row.put("output_digest",o.digest);row.put("detail",o.detail);
  std::ostringstream line;
  line<<csv_escape(c.id)<<','<<csv_escape(c.family)<<','<<csv_escape(axis_label(c,cfg.axis))<<','<<holdout
      <<','<<csv_escape(o.algorithm)<<','<<csv_escape(o.status)<<','<<o.ns<<','<<o.batch<<','<<o.work
      <<','<<o.block<<','<<(o.cpu_ns>=0?std::to_string(o.cpu_ns):"")
      <<','<<(o.peak_rss_bytes>=0?std::to_string(o.peak_rss_bytes):"")
      <<','<<(o.order_position>=0?std::to_string(o.order_position):"")
      <<','<<csv_escape(o.order_seed)<<','<<csv_escape(o.digest)<<','<<csv_escape(o.detail)<<'\n';
  const auto row_bytes=row.str().size(),csv_bytes=line.str().size();
  if(raw_size+row_bytes+csv_bytes>export_limit){truncated=true;continue;}
  raw_size+=row_bytes+csv_bytes;json_object_array_add(raw,row.p);row.p=nullptr;
  csv<<line.str();
 }
 report.put("raw_samples_truncated",truncated);
 report.put("raw_export_budget_bytes",static_cast<std::int64_t>(cfg.max_report_bytes));
 json_object_object_add(report.p,"raw_samples",raw);
 if(report.str().size()>cfg.max_report_bytes)throw Error("JSON report exceeds max_report_bytes");
 if(csv.str().size()>cfg.max_report_bytes)throw Error("CSV report exceeds max_report_bytes");
 write_file(run+"/report.json",report.str());write_file(run+"/samples.csv",csv.str());
 auto* summary_obj=report.get("summary");Json summ(json_object_get(summary_obj));
 std::ostringstream md;md<<"# PHASESHIFT experimental report\n\n- Experiment: "<<cfg.experiment_id<<"\n- Family: "<<cfg.family<<"\n- Comparison: "<<cfg.a<<(cfg.b.empty()?"":" vs "+cfg.b)<<"\n- Discovered cases and holdouts: "<<summ.i("cases")<<" total\n- Raw observations: "<<summ.i("samples")<<"\n- Incorrect, timeout, crash or invalid observations: "<<summ.i("invalid_samples")<<"\n- Primary axis: "<<cfg.axis<<"\n- Full raw evidence: `samples.csv`, `report.json`, `run.sqlite`\n\n## Boundary candidates and verdicts\n\n";
 auto* hypotheses=summ.get("hypotheses");for(std::size_t i=0;i<json_object_array_length(hypotheses);i++){
  Json x(json_object_get(json_object_array_get_idx(hypotheses,i)));md<<"- "<<x.s("id")<<": **"<<x.s("status")<<"**; between "<<x.i("left")<<" and "<<x.i("right")<<" (effect="<<x.d("effect")<<"). "<<x.s("detail")<<"\n";
 }
 md<<"\n## Export completeness\n\n- Raw CSV/JSON projection "<<(truncated?"TRUNCATED by max_report_bytes; authoritative complete raw data remain in run.sqlite":"contains every recorded sample")<<".\n\n## Scientific limitations\n\n- Synthetic work units are not time; this fixture is an engineered calibration, not a natural discovery.\n- CPU timings vary by host, cache, thermal conditions, and compiler settings.\n- Real performance candidates are **not confirmed** without calibrated paired-case intervals and multiplicity controls.\n- Observational morphology alone does not establish causality.\n- Reproduce with `phaseshift replay --run "<<run<<" --case <case-id>` and inspect raw rows.\n";
 write_file(run+"/report.md",md.str());write_file(run+"/report.html","<!doctype html><meta charset=\"utf-8\"><title>PHASESHIFT report</title><style>body{font:16px system-ui;max-width:900px;margin:3em auto;white-space:pre-wrap}</style><main>"+html_escape(md.str())+"</main>");
 for(const auto& filename:{"report.json","samples.csv","report.md","report.html"})
  s.artifact(filename,sha256_file(run+"/"+filename),"experiment-report");
}
void verify_bundle(const std::string& bundle){
 const auto path=fs::path(bundle);
 if(!fs::is_directory(path)||!fs::is_regular_file(path/"SHA256.json"))throw Error("bundle directory/manifest missing");
 const auto manifest=Json::file((path/"SHA256.json").string());
 if(manifest.i("bundle_schema")!=1)throw Error("unsupported bundle schema");
 const auto* files=manifest.get("files");if(!files||json_object_get_type(files)!=json_type_object)throw Error("invalid bundle file index");
 std::set<std::string> expected;
 json_object_object_foreach(files,key,value){
  const auto name=std::string(key);
  if(name.empty()||name=="SHA256.json"||name.find('/')!=std::string::npos||name.find('\\')!=std::string::npos||name=="."||name==".."||json_object_get_type(value)!=json_type_string)throw Error("unsafe bundle file path/hash");
  const auto member=path/name;
  if(!fs::is_regular_file(member)||fs::is_symlink(member))throw Error("bundle missing/nonregular file: "+name);
  if(sha256_file(member.string())!=json_object_get_string(value))throw Error("bundle file digest mismatch: "+name);
  expected.insert(name);
 }
 for(const auto& must:{"run.sqlite","report.json","report.md","samples.csv","README.md"})if(!expected.count(must))throw Error("bundle missing required member: "+std::string(must));
 for(const auto& file:fs::directory_iterator(path)){
  const auto name=file.path().filename().string();if(!file.is_regular_file()||(name!="SHA256.json"&&!expected.count(name)))throw Error("unexpected/unindexed bundle member: "+name);
 }
 Store store((path/"run.sqlite").string(),true);
 const auto conf=store.config();
 sqlite3_stmt* stmt=nullptr;
 if(sqlite3_prepare_v2(store.raw(),"SELECT config_hash FROM experiments WHERE id=1",-1,&stmt,nullptr)!=SQLITE_OK)throw Error("missing config checksum in bundle");
 const auto rc=sqlite3_step(stmt);const auto stored=rc==SQLITE_ROW?std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt,0))):"";sqlite3_finalize(stmt);
 if(sha256(conf)!=stored)throw Error("bundle experiment config hash mismatch");
 for(const auto& c:store.cases()){(void)c;} /* expensive but mandatory regeneration + digest verification */
 if(expected.count("minimization.sqlite")){
  Store minimized((path/"minimization.sqlite").string(),true);
  if(minimized.config()!=conf)throw Error("minimization config differs from original evidence");
  for(const auto& c:minimized.cases()){(void)c;}
 }
 if(expected.count("witness.json")){
  if(!expected.count("witness.hex"))throw Error("witness metadata lacks canonical input bytes");
  const auto witness=Json::file((path/"witness.json").string());
  const auto encoded=read_file((path/"witness.hex").string());
  auto cleaned=encoded;while(!cleaned.empty()&&(cleaned.back()=='\n'||cleaned.back()=='\r'))cleaned.pop_back();
  if(sha256(hex_decode(cleaned))!=witness.s("input_sha256"))throw Error("witness input digest mismatch");
  if(witness.s("category")=="PERFORMANCE_WITNESS"&&!expected.count("minimization.sqlite"))throw Error("performance witness lacks independently recorded minimization data");
 }
 std::cout<<"VERIFIED BUNDLE: "<<bundle<<" members="<<expected.size()<<" cases="<<store.summary().i("cases")<<"\n";
}
void export_run(const std::string& run,const std::string& out){
 if(fs::exists(out))throw Error("bundle output exists");
 report(run);
 /* A connection must not be copying a live SQLite WAL. */
 {
  Store db(run+"/run.sqlite");db.exec("PRAGMA wal_checkpoint(TRUNCATE)");
 }
 fs::create_directories(out);
 try{
  for(const auto& file:{"run.sqlite","report.json","report.md","report.html","samples.csv"})fs::copy_file(run+"/"+file,out+"/"+file);
  if(fs::exists(run+"/minimization.sqlite")){
   {Store minimized(run+"/minimization.sqlite");minimized.exec("PRAGMA wal_checkpoint(TRUNCATE)");}
   fs::copy_file(run+"/minimization.sqlite",out+"/minimization.sqlite");
  }
  if(fs::exists(run+"/witness.cpp"))fs::copy_file(run+"/witness.cpp",out+"/witness.cpp");
  if(fs::exists(run+"/witness.hex"))fs::copy_file(run+"/witness.hex",out+"/witness.hex");
  if(fs::exists(run+"/witness.json"))fs::copy_file(run+"/witness.json",out+"/witness.json");
  write_file(out+"/README.md","# PHASESHIFT portable evidence bundle\nUse `phaseshift verify-bundle --bundle .` before importing. All member SHA-256 digests are in SHA256.json; case inputs are independently regenerated and validated.\n");
  Json manifest;manifest.put("bundle_schema",static_cast<std::int64_t>(1));Json files;
  for(const auto& f:fs::directory_iterator(out))if(f.is_regular_file())files.put(f.path().filename().string().c_str(),sha256_file(f.path().string()));
  manifest.put("files",std::move(files));write_file(out+"/SHA256.json",manifest.str());
  verify_bundle(out);
 }catch(...){fs::remove_all(out);throw;}
 std::cout<<"Exported "<<out<<"\n";
}
void import_bundle(const std::string& bundle,const std::string& out){
 verify_bundle(bundle);
 if(fs::exists(out))throw Error("import destination already exists; refusing overwrite");
 fs::create_directories(out);
 try{
  for(const auto& file:fs::directory_iterator(bundle))fs::copy_file(file.path(),fs::path(out)/file.path().filename());
  verify_bundle(out);
 }catch(...){fs::remove_all(out);throw;}
 std::cout<<"Imported verified evidence into "<<out<<"\n";
}
void compare(const std::string& a,const std::string& b){Store left(a+"/run.sqlite"),right(b+"/run.sqlite");auto x=config_from_json(Json::parse(left.config())),y=config_from_json(Json::parse(right.config()));
 if(x.family!=y.family||x.a!=y.a||x.b!=y.b)throw Error("incompatible algorithm output contracts");
 std::cout<<"A: "<<left.summary().str()<<"\nB: "<<right.summary().str()<<"\n";
 std::cout<<"WARNING: hardware equivalence has not been established; do not compare absolute times as speedup.\n";
}
}
