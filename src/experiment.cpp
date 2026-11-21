#include "phaseshift/experiment.hpp"
#include "phaseshift/campaign_v2.hpp"
#include "phaseshift/confirmation_seeds_v2.hpp"
#include "phaseshift/statistics.hpp"
#include "phaseshift/morphology.hpp"
#include "phaseshift/minimize.hpp"
#include "phaseshift/witness_paths_v2.hpp"
#include "phaseshift/reproducer.hpp"
#include "phaseshift/lazy_search.hpp"
#include "phaseshift/stratified_v2.hpp"
#include "phaseshift/scheduler_v2.hpp"
#include "phaseshift/hybrid_v2.hpp"
#include "phaseshift/interaction_v2.hpp"
#include "phaseshift/boundary_v2.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <memory>
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
static std::int64_t axis_value(const Case& c,const std::string& axis);
static Json config_json(const Config& c){
 Json j;j.put("schema_version",static_cast<std::int64_t>(1));j.put("experiment_id",c.experiment_id);j.put("problem_family",c.family);j.put("algorithm_a",c.a);j.put("algorithm_b",c.b);j.put("search_method",c.method);j.put("primary_axis",c.axis);j.put("seed",static_cast<std::int64_t>(c.seed));j.put("max_cases",static_cast<std::int64_t>(c.max_cases));j.put("max_case_bytes",static_cast<std::int64_t>(c.max_bytes));j.put("max_search_queue",static_cast<std::int64_t>(c.max_queue));j.put("max_report_bytes",static_cast<std::int64_t>(c.max_report_bytes));j.put("discovery_blocks",static_cast<std::int64_t>(c.blocks));j.put("confirm_blocks",static_cast<std::int64_t>(c.confirm_blocks));j.put("warmup",static_cast<std::int64_t>(c.warmup));j.put("sample_ms",static_cast<std::int64_t>(c.sample_ms));j.put("rss_mb",static_cast<std::int64_t>(c.rss_mb));j.put("total_wall_seconds",static_cast<std::int64_t>(c.total_wall_s));j.put("min_practical_ratio",c.min_ratio);j.put("confirmation_confidence",c.confirmation_confidence);j.put("fdr_q",c.fdr_q);j.put("multiple_testing",c.multiple_testing);j.put("exploration_fraction",c.exploration_fraction);j.put("minimum_batch_ms",static_cast<std::int64_t>(c.minimum_batch_ms));j.put("isolation_mode",c.isolation_mode);if(c.policy_version=="v2"){
   j.put("policy_version",c.policy_version);
   if(c.replicate_count>1||!c.admissible_subspaces.empty()){
     Json search;
     if(c.replicate_count>1)search.put("replicates",static_cast<std::int64_t>(c.replicate_count));
     if(!c.admissible_subspaces.empty()){
      auto* array=json_object_new_array();
      for(const auto& scope:c.admissible_subspaces){
       Json entry;
       for(const auto& [axis,allowed]:scope){
        auto* values=json_object_new_array();
        for(const auto& v:allowed)json_object_array_add(values,json_object_new_string(v.c_str()));
        json_object_object_add(entry.p,axis.c_str(),values);
       }
       json_object_array_add(array,json_object_get(entry.p));
      }
      json_object_object_add(search.p,"admissible_subspaces",array);
     }
     j.put("search",std::move(search));
    }
   if(c.detector_version!="boundary_v1"){Json detection;detection.put("detector_version",c.detector_version);j.put("detection",std::move(detection));}
   if(c.max_sampled_cpu_ns){Json limits;limits.put("max_sampled_cpu_ns",static_cast<std::int64_t>(c.max_sampled_cpu_ns));j.put("limits",std::move(limits));}
   if(c.measurement_protocol_version!="legacy_retained_outputs_v1"){
    Json m;m.put("protocol_version",c.measurement_protocol_version);
    j.put("measurement",std::move(m));
   }
  }
  j.put("base_params",case_params(c.base));
 Json axes;for(const auto& [name,values]:c.axes){auto* arr=json_object_new_array();for(const auto& val:values)json_object_array_add(arr,json_object_new_string(val.c_str()));json_object_object_add(axes.p,name.c_str(),arr);}j.put("axes",std::move(axes));return j;
}
/* Config is an experimental contract: reject unsupported keys and coercions,
   rather than silently measuring a workload different from the stated one. */
static void validate_configuration_shape(const Json& j){
 if(!j.p||json_object_get_type(j.p)!=json_type_object)throw Error("configuration root must be an object");
 const auto check=[](json_object* object,const std::string& scope,
                     const std::set<std::string>& integers,
                     const std::set<std::string>& reals,
                     const std::set<std::string>& strings,
                     const std::set<std::string>& structures){
  if(!object||json_object_get_type(object)!=json_type_object)throw Error(scope+" must be an object");
  json_object_object_foreach(object,key,value){
   const std::string name(key),field=scope+"."+name;
   if(!value)throw Error(field+" cannot be null");
   const auto kind=json_object_get_type(value);
   if(integers.count(name)){
    if(kind!=json_type_int)throw Error(field+" must be an integer");
    const auto n=json_object_get_int64(value);
    if(n<0 && (name=="seed"||name=="max_cases"||name=="max_case_bytes"||name=="max_worker_rss_mb"||name=="max_sample_wall_ms"||name=="max_report_bytes"||name=="max_search_queue"||name=="total_wall_seconds"||name=="max_sampled_cpu_ns"||name=="schema_version"))throw Error(field+" cannot be negative");
    /* The runtime's integer-valued measurement/budget fields are signed 32-bit. */
    if(name!="seed"&&name!="max_case_bytes"&&name!="max_search_queue"&&name!="max_report_bytes"&&name!="max_cases"&&name!="max_sampled_cpu_ns"&&n>2147483647LL)throw Error(field+" is too large");
   }else if(reals.count(name)){
    if(kind!=json_type_int&&kind!=json_type_double)throw Error(field+" must be a finite number");
    if(!std::isfinite(json_object_get_double(value)))throw Error(field+" must be finite");
   }else if(strings.count(name)){
    if(kind!=json_type_string)throw Error(field+" must be a string");
   }else if(structures.count(name)){
    if(kind!=json_type_object&&!(name=="base_params"&&kind==json_type_string)&&
       !(scope=="search"&&name=="admissible_subspaces"&&kind==json_type_array))
      throw Error(field+" must be an object or admissible subspace array");
   }else throw Error("unknown configuration parameter: "+field);
  }
 };
 static const std::set<std::string> integer_top={"schema_version","seed","max_cases","max_case_bytes","max_search_queue","max_report_bytes","discovery_blocks","confirm_blocks","warmup","sample_ms","rss_mb","total_wall_seconds","minimum_batch_ms"};
 static const std::set<std::string> real_top={"min_practical_ratio","confirmation_confidence","fdr_q","exploration_fraction"};
 static const std::set<std::string> text_top={"experiment_id","problem_family","family","objective","algorithm_a","algorithm_b","search_method","method","primary_axis","isolation_mode","multiple_testing","policy_version"};
 static const std::set<std::string> obj_top={"search","measurement","detection","limits","fixed","base_params","axes"};
 /* The algorithm list is validated separately because it is an array. */
 json_object* algorithms=nullptr;
 if(json_object_object_get_ex(j.p,"algorithms",&algorithms)){
  if(!algorithms||json_object_get_type(algorithms)!=json_type_array)throw Error("algorithms must be an array");
  const auto length=json_object_array_length(algorithms);
  if(length<1||length>2)throw Error("algorithms must contain one or two elements");
  for(std::size_t i=0;i<length;i++){
   auto* entry=json_object_array_get_idx(algorithms,i);
   if(!entry||json_object_get_type(entry)!=json_type_string||!json_object_get_string(entry)||!json_object_get_string(entry)[0])
    throw Error("algorithms["+std::to_string(i)+"] must be a nonempty string");
  }
 }
 /* The object-key validator is intentionally applied to a filtered object;
    algorithms has the separate array contract above. */
 Json top;
 json_object_object_foreach(j.p,key,value){
  if(std::string(key)!="algorithms")json_object_object_add(top.p,key,json_object_get(value));
 }
 check(top.p,"config",integer_top,real_top,text_top,obj_top);
 if(auto* v=j.get("objective");v&&std::string(json_object_get_string(v))!="discover_structural_performance_boundaries")
  throw Error("unsupported objective");
 if(auto* v=j.get("search"))check(v,"search",{"max_cases","replicates"},{"exploration_fraction"},{"method","policy_version"},{"admissible_subspaces"});
 if(auto* v=j.get("measurement"))check(v,"measurement",{"paired_discovery_blocks","paired_confirm_blocks","warmup_iterations","minimum_batch_ms"},{},{"primary_metric","isolation_mode","protocol_version"},{});
 if(auto* v=j.get("detection"))check(v,"detection",{},{"min_practical_ratio","confirmation_confidence","fdr_q"},{"multiple_testing","detector_version"},{});
 if(auto* v=j.get("limits"))check(v,"limits",{"total_wall_seconds","max_cases","max_case_bytes","max_worker_rss_mb","max_sample_wall_ms","max_report_bytes","max_search_queue","max_sampled_cpu_ns"},{},{},{});
 if(auto* v=j.get("fixed"))if(json_object_get_type(v)!=json_type_object)throw Error("fixed must be an object");
 if(auto* v=j.get("base_params")){
  if(json_object_get_type(v)==json_type_string){auto parsed=Json::parse(json_object_get_string(v));if(!parsed.p||json_object_get_type(parsed.p)!=json_type_object)throw Error("base_params serialized value must be object");}
 }
 if(auto* v=j.get("axes")){
  if(json_object_get_type(v)!=json_type_object)throw Error("axes must be an object");
  json_object_object_foreach(v,axis_name,axis_value){
   if(!axis_value)throw Error(std::string("axes.")+axis_name+" cannot be null");
   json_object* axis_values=axis_value;
   if(json_object_get_type(axis_value)==json_type_object){
    json_object_object_foreach(axis_value,member_key,axis_v){
     if(std::string(member_key)!="values")throw Error(std::string("unknown axes.")+axis_name+" field: "+member_key);
     (void)axis_v;
    }
    if(!json_object_object_get_ex(axis_value,"values",&axis_values))throw Error(std::string("axes.")+axis_name+" missing axis_values");
   }
   if(!axis_values||json_object_get_type(axis_values)!=json_type_array||json_object_array_length(axis_values)==0)
    throw Error(std::string("axes.")+axis_name+" axis_values must be a nonempty array");
   for(std::size_t index=0;index<json_object_array_length(axis_values);index++){
    const auto* member=json_object_array_get_idx(axis_values,index);
    if(!member||json_object_get_type(member)==json_type_object||json_object_get_type(member)==json_type_array)
     throw Error(std::string("axes.")+axis_name+" contains invalid axis_value");
    if(json_object_get_type(member)==json_type_double&&!std::isfinite(json_object_get_double(member)))
     throw Error(std::string("axes.")+axis_name+" contains nonfinite axis_value");
   }
  }
 }
}
Config config_from_json(const Json& j){
 validate_configuration_shape(j);
 if(j.i("schema_version",1)!=1)throw Error("only config schema version 1 supported");
 auto require_field_type=[](const Json& object,const char* name,bool numeric){
  const auto* v=object.get(name);if(!v)return;
  const auto type=json_object_get_type(v);
  if(numeric?(type!=json_type_int&&type!=json_type_double):type!=json_type_string)
   throw Error(std::string("wrong type for configuration parameter: ")+name);
 };
 require_field_type(j,"exploration_fraction",true);
 require_field_type(j,"isolation_mode",false);
 if(const auto* v=j.get("minimum_batch_ms");v&&json_object_get_type(v)!=json_type_int)
  throw Error("minimum_batch_ms must be integral");
 Config c;c.policy_version=j.s("policy_version","v1");c.experiment_id=j.s("experiment_id","experiment");c.family=j.s("problem_family",j.s("family","calibration"));
 c.a=j.s("algorithm_a","");c.b=j.s("algorithm_b","");
 if(auto* a=j.get("algorithms")){if(json_object_get_type(a)!=json_type_array||json_object_array_length(a)<1||json_object_array_length(a)>2)throw Error("algorithms must contain one or two elements");c.a=json_object_get_string(json_object_array_get_idx(a,0));if(json_object_array_length(a)>1)c.b=json_object_get_string(json_object_array_get_idx(a,1));}
 if(c.a.empty())c.a=c.family=="calibration"?"calibration":c.family=="integer_array"?"std_sort":c.family=="topk"?"topk_heap":"dijkstra_heap";
 c.method=j.s("search_method",j.s("method","grid"));if(auto* search=j.get("search")){Json k(json_object_get(search));require_field_type(k,"exploration_fraction",true);c.method=k.s("method",c.method);c.policy_version=k.s("policy_version",c.policy_version);
  const auto replicates=k.i("replicates",1);
  if(replicates<1||replicates>32)throw Error("v2 replicate count must be 1..32");
  c.replicate_count=static_cast<int>(replicates);
  c.max_cases=static_cast<std::uint64_t>(k.i("max_cases",static_cast<std::int64_t>(c.max_cases)));c.exploration_fraction=k.d("exploration_fraction",c.exploration_fraction);}
 c.axis=j.s("primary_axis",c.family=="calibration"?"x":c.family=="graph"?"edges":"n");
 c.seed=static_cast<std::uint64_t>(j.i("seed",137));c.max_cases=static_cast<std::uint64_t>(j.i("max_cases",static_cast<std::int64_t>(c.max_cases)));c.max_bytes=static_cast<std::uint64_t>(j.i("max_case_bytes",64*1024*1024));c.max_queue=static_cast<std::uint64_t>(j.i("max_search_queue",10000));c.max_report_bytes=static_cast<std::uint64_t>(j.i("max_report_bytes",10000000));c.blocks=static_cast<int>(j.i("discovery_blocks",9));c.confirm_blocks=static_cast<int>(j.i("confirm_blocks",20));c.warmup=static_cast<int>(j.i("warmup",2));c.sample_ms=static_cast<int>(j.i("sample_ms",3000));c.rss_mb=static_cast<int>(j.i("rss_mb",1024));c.total_wall_s=static_cast<int>(j.i("total_wall_seconds",600));c.min_ratio=j.d("min_practical_ratio",1.2);c.exploration_fraction=j.d("exploration_fraction",c.exploration_fraction);c.minimum_batch_ms=static_cast<int>(j.i("minimum_batch_ms",c.minimum_batch_ms));c.isolation_mode=j.s("isolation_mode",c.isolation_mode);
 if(auto* m=j.get("measurement")){Json x(json_object_get(m));require_field_type(x,"isolation_mode",false);
  if(json_object_get_type(m)!=json_type_object)throw Error("measurement must be an object");
  static const std::set<std::string> accepted={"paired_discovery_blocks","paired_confirm_blocks","warmup_iterations","minimum_batch_ms","primary_metric","isolation_mode","protocol_version"};
  json_object_object_foreach(m,key,value){(void)value;if(!accepted.count(key))throw Error(std::string("unsupported measurement key: ")+key);}
  if(x.get("primary_metric")){
   if(json_object_get_type(x.get("primary_metric"))!=json_type_string||x.s("primary_metric")!="wall_ns_per_operation")
    throw Error("unsupported measurement.primary_metric; only wall_ns_per_operation is implemented");
  }
  if(const auto* v=x.get("minimum_batch_ms");v&&json_object_get_type(v)!=json_type_int)
   throw Error("minimum_batch_ms must be integral");
  c.blocks=static_cast<int>(x.i("paired_discovery_blocks",c.blocks));c.confirm_blocks=static_cast<int>(x.i("paired_confirm_blocks",c.confirm_blocks));c.warmup=static_cast<int>(x.i("warmup_iterations",c.warmup));c.minimum_batch_ms=static_cast<int>(x.i("minimum_batch_ms",c.minimum_batch_ms));c.isolation_mode=x.s("isolation_mode",c.isolation_mode);
  c.measurement_protocol_version=x.s("protocol_version",c.measurement_protocol_version);}
 if(auto* d=j.get("detection")){Json x(json_object_get(d));c.min_ratio=x.d("min_practical_ratio",c.min_ratio);c.confirmation_confidence=x.d("confirmation_confidence",c.confirmation_confidence);c.fdr_q=x.d("fdr_q",c.fdr_q);c.multiple_testing=x.s("multiple_testing",c.multiple_testing);c.detector_version=x.s("detector_version",c.detector_version);}
 if(auto* l=j.get("limits")){Json x(json_object_get(l));c.total_wall_s=static_cast<int>(x.i("total_wall_seconds",c.total_wall_s));c.max_cases=static_cast<std::uint64_t>(x.i("max_cases",static_cast<std::int64_t>(c.max_cases)));c.max_bytes=static_cast<std::uint64_t>(x.i("max_case_bytes",static_cast<std::int64_t>(c.max_bytes)));c.rss_mb=static_cast<int>(x.i("max_worker_rss_mb",c.rss_mb));c.sample_ms=static_cast<int>(x.i("max_sample_wall_ms",c.sample_ms));c.max_queue=static_cast<std::uint64_t>(x.i("max_search_queue",static_cast<std::int64_t>(c.max_queue)));c.max_report_bytes=static_cast<std::uint64_t>(x.i("max_report_bytes",static_cast<std::int64_t>(c.max_report_bytes)));c.max_sampled_cpu_ns=static_cast<std::uint64_t>(x.i("max_sampled_cpu_ns",0));}
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
 if(auto* search=j.get("search")){
  json_object* scopes=nullptr;
  if(json_object_object_get_ex(search,"admissible_subspaces",&scopes)){
   if(!scopes||json_object_get_type(scopes)!=json_type_array||
      json_object_array_length(scopes)==0||json_object_array_length(scopes)>256)
    throw Error("search.admissible_subspaces must contain 1..256 objects");
   for(std::size_t i=0;i<json_object_array_length(scopes);++i){
    auto* item=json_object_array_get_idx(scopes,i);
    if(!item||json_object_get_type(item)!=json_type_object||json_object_object_length(item)==0)
     throw Error("admissible subspace must be a nonempty object");
    std::map<std::string,std::vector<std::string>> scope;
    json_object_object_foreach(item,key,labels){
     if(!labels||json_object_get_type(labels)!=json_type_array||
        json_object_array_length(labels)==0)
      throw Error(std::string("admissible label set must be nonempty: ")+key);
     auto& values=scope[key];
     for(std::size_t k=0;k<json_object_array_length(labels);++k){
      auto* value=json_object_array_get_idx(labels,k);
      if(!value||json_object_get_type(value)==json_type_object||
         json_object_get_type(value)==json_type_array||
         (json_object_get_type(value)==json_type_double&&!std::isfinite(json_object_get_double(value))))
       throw Error(std::string("invalid admissible label: ")+key);
      values.push_back(value_string(value));
     }
    }
    c.admissible_subspaces.push_back(std::move(scope));
   }
  }
 }
 
 if(c.policy_version=="v1"){
  if(auto* detector=j.get("detection")){
   Json policy(json_object_get(detector));
   if(policy.get("detector_version"))throw Error("v1 does not accept detection.detector_version");
  }
  if(auto* nested=j.get("search")){
   Json search(json_object_get(nested));
   if(search.get("replicates"))throw Error("v1 does not accept search.replicates");
   if(search.get("admissible_subspaces"))throw Error("v1 does not accept search.admissible_subspaces");
  }
  if(auto* l=j.get("limits")){Json limits(json_object_get(l));if(limits.get("max_sampled_cpu_ns"))throw Error("v1 does not accept max_sampled_cpu_ns");}
  if(auto* m=j.get("measurement")){Json legacy(json_object_get(m));
   if(legacy.get("protocol_version"))throw Error("v1 does not accept measurement.protocol_version");
  }
  if(c.method!="grid"&&c.method!="random"&&c.method!="stratified"&&c.method!="adaptive_partition")throw Error("unknown search method");
 }else if(c.policy_version=="v2"){
  if(c.method!="uniform_v2"&&c.method!="hierarchical_boundary_v2"&&c.method!="stratified_v2"&&c.method!="hybrid_mixed_v2")throw Error("unsupported v2 search method");
  if(c.measurement_protocol_version!="legacy_retained_outputs_v1"&&
     c.measurement_protocol_version!="calibrated_batch_v2")
   throw Error("unsupported v2 measurement protocol; kernel_only_v2 is unavailable");
  if(c.detector_version!="boundary_v1"&&c.detector_version!="multi_change_v2"&&c.detector_version!="multi_change_pelt_v2")
   throw Error("unsupported v2 detector version");
  if((c.detector_version=="multi_change_v2"||c.detector_version=="multi_change_pelt_v2")&&!numeric_axis(c.axis))
   throw Error("multi_change_v2 requires an ordered numeric primary axis");
  if(c.max_cases>c.max_queue)throw Error("v2 candidate budget exceeds memory bound");
  if(c.seed>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())-static_cast<std::uint64_t>(c.replicate_count-1))
   throw Error("v2 replicate seeds overflow signed persistence");
  if(c.method=="hierarchical_boundary_v2"&&(c.axes.size()!=1||!numeric_axis(c.axis)||c.replicate_count!=1||!c.admissible_subspaces.empty()))
   throw Error("hierarchical_boundary_v2 currently requires one ordered numeric axis");
  if((c.method=="hierarchical_boundary_v2"||c.method=="hybrid_mixed_v2")&&numeric_axis(c.axis)){
   std::optional<std::int64_t> previous;
   for(const auto& label:c.axes.at(c.axis)){
    Case sample=c.base;assign(sample,c.axis,label);
    const auto current=axis_value(sample,c.axis);
    if(previous&&current<=*previous)throw Error("hierarchical_boundary_v2 requires strictly increasing axis values");
    previous=current;
   }
  }
 }else throw Error("unsupported search.policy_version");
 if(c.max_cases==0||c.max_cases>20000||c.max_queue==0||c.max_queue>100000||c.max_report_bytes<4096||c.max_report_bytes>1000000000||c.max_bytes<64||c.max_bytes>64ULL*1024*1024||c.blocks<1||c.blocks>100||c.confirm_blocks<1||c.confirm_blocks>100||c.warmup<0||c.warmup>100||c.sample_ms<100||c.sample_ms>60000||c.total_wall_s<1||c.total_wall_s>86400||c.rss_mb<64||c.rss_mb>65536||!std::isfinite(c.min_ratio)||c.min_ratio<1.01||c.min_ratio>100000||!std::isfinite(c.confirmation_confidence)||c.confirmation_confidence<0.90||c.confirmation_confidence>=1.0||!std::isfinite(c.fdr_q)||c.fdr_q<0.001||c.fdr_q>0.25||c.multiple_testing!="bh_fdr"||!std::isfinite(c.exploration_fraction)||c.exploration_fraction<0.0||c.exploration_fraction>1.0||c.minimum_batch_ms<1||c.minimum_batch_ms>100|| (c.isolation_mode!="per_case"&&c.isolation_mode!="per_sample"))throw Error("configuration resource/measurement bounds invalid");
 if(!c.axes.count(c.axis)){
  if(j.get("primary_axis"))throw Error("explicit primary_axis is not declared in axes: "+c.axis);
  c.axis=c.axes.begin()->first; /* Only omitted axes may use a deterministic inferred default. */
 }
 if(c.family!="integer_array"&&(c.a.rfind("plugin:",0)==0||c.b.rfind("plugin:",0)==0))
  throw Error("native plugin ABI v1 supports integer_array only");
 auto valid=algorithms_for(c.family);if(std::find(valid.begin(),valid.end(),c.a)==valid.end()&&c.a.rfind("plugin:",0)!=0)throw Error("unknown primary algorithm for family");
 if(!c.b.empty()&&std::find(valid.begin(),valid.end(),c.b)==valid.end()&&c.b.rfind("plugin:",0)!=0)throw Error("unknown secondary algorithm for family");
 std::uint64_t product=1;
 for(const auto& [key,values]:c.axes){
  if(c.policy_version=="v1"){
   if(values.size()>c.max_queue||product>c.max_queue/values.size())throw Error("search grid exceeds max_search_queue; split the experiment");
   product*=values.size();
  }
  std::set<std::string> unique_values;
  for(const auto& v:values){
   if(!unique_values.insert(v).second)throw Error("duplicate axis value in "+key);
   Case sample=c.base;assign(sample,key,v);
  }
 }
 if(c.base.density_ppm>=0&&c.axes.count("edges"))throw Error("density and edges cannot both be independent axes");
 /* Validate every combination without allocating experiment inputs. */
 std::vector<std::pair<std::string,std::vector<std::string>>> dims(c.axes.begin(),c.axes.end());
 std::function<void(std::size_t,Case)> visit=[&](std::size_t index,Case candidate){
  if(index==dims.size()){validate_case_schema(candidate,c.max_bytes);return;}
  for(const auto& value:dims[index].second){Case next=candidate;assign(next,dims[index].first,value);visit(index+1,next);}
 };
 if(c.policy_version=="v1")visit(0,c.base);
 else (void)AdmissibleSearchSpace(std::vector<LazySearchSpace::Axis>(c.axes.begin(),c.axes.end()),c.admissible_subspaces);
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
static std::string formatted_axis_value(const std::string& axis,std::int64_t value);
static std::int64_t axis_value(const Case& c,const std::string& axis);
static std::string axis_label(const Case& c,const std::string& axis);
static std::string categorical_value(const Config& cfg,std::int64_t index){
 const auto& labels=cfg.axes.at(cfg.axis);
 if(index<0||static_cast<std::size_t>(index)>=labels.size())throw Error("invalid frozen categorical axis index");
 return labels[static_cast<std::size_t>(index)];
}
static std::string frozen_axis_value(const Config& cfg,std::int64_t value){
 return numeric_axis(cfg.axis)?formatted_axis_value(cfg.axis,value):categorical_value(cfg,value);
}
static std::vector<Case> scheduled(const Config& c){
 auto cases=grid_cases(c);
 if(c.method=="random"){
  std::uint64_t seed=c.seed^0x925c42;
  for(std::size_t i=cases.size();i>1;i--){auto j=randbelow(seed,i);std::swap(cases[i-1],cases[static_cast<std::size_t>(j)]);}
 }else if(c.method=="stratified"){
  std::vector<std::vector<Case>> groups;
  if(numeric_axis(c.axis)){
   // Quantile strata over the ordered primary axis, with seeded *within*
   // stratum sampling; a deterministic sweep is not stratified random.
   std::stable_sort(cases.begin(),cases.end(),[&](const Case& a,const Case& b){return axis_value(a,c.axis)<axis_value(b,c.axis);});
   const auto strata=std::min<std::size_t>(8,cases.size());
   for(std::size_t i=0;i<strata;i++){
    const auto start=i*cases.size()/strata,end=(i+1)*cases.size()/strata;
    groups.emplace_back(cases.begin()+static_cast<std::ptrdiff_t>(start),cases.begin()+static_cast<std::ptrdiff_t>(end));
   }
  }else{
   // Categorical strata are exact labels, not ordered numeric bins.
   const auto& labels=c.axes.at(c.axis);
   groups.resize(labels.size());
   for(auto& item:cases){
    const auto label=axis_label(item,c.axis);
    const auto at=std::find(labels.begin(),labels.end(),label);
    if(at==labels.end())throw Error("categorical search case not in axis design");
    groups[static_cast<std::size_t>(std::distance(labels.begin(),at))].push_back(std::move(item));
   }
  }
  std::uint64_t rng=c.seed^0x853bc248f1dULL;
  for(auto& group:groups)for(std::size_t i=group.size();i>1;i--){
   const auto j=static_cast<std::size_t>(randbelow(rng,i));std::swap(group[i-1],group[j]);
  }
  // Shuffle stratum visitation too: a six-case budget should not always
  // favor the first category in a written JSON configuration.
  for(std::size_t i=groups.size();i>1;i--){
   const auto j=static_cast<std::size_t>(randbelow(rng,i));std::swap(groups[i-1],groups[j]);
  }
  std::vector<Case> order;order.reserve(cases.size());
  for(std::size_t round=0;order.size()<cases.size();round++)
   for(auto& group:groups)if(round<group.size())order.push_back(std::move(group[round]));
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
 if(!numeric_axis(cfg.axis)){
  // Unordered category-edge detection. Indices are *identifiers* in the
  // frozen design, not magnitudes, distances or interpolated thresholds.
  const auto& labels=cfg.axes.at(cfg.axis);
  std::map<std::string,std::map<std::int64_t,std::vector<double>>> measurements;
  struct Status {std::string value;bool consistent=true;};
  std::map<std::string,std::map<std::int64_t,Status>> failures;
  std::map<std::string,std::set<std::int64_t>> invalid_performance;
  std::map<std::string,Case> exemplars;
  for(const auto& c:store.cases(false)){
   const auto label=axis_label(c,cfg.axis);
   auto found=std::find(labels.begin(),labels.end(),label);
   if(found==labels.end())throw Error("persisted category not in frozen design");
   const auto pos=static_cast<std::int64_t>(std::distance(labels.begin(),found));
   const auto scope=scope_key(c,cfg);
   exemplars.emplace(scope,c);
   if(auto value=metric(store,c,cfg,false))measurements[scope][pos].push_back(*value);
   else invalid_performance[scope].insert(pos);
   if(cfg.family!="calibration"){
    const auto obs=store.observations(c.id,false);
    std::string status;bool consistent=true;std::size_t count=0;
    for(const auto& o:obs)if(o.algorithm==cfg.a){
     if(count++==0)status=o.status;
     else if(o.status!=status)consistent=false;
    }
    if(consistent&&count>=static_cast<std::size_t>(cfg.blocks)){
     auto [it,inserted]=failures[scope].emplace(pos,Status{status,true});
     if(!inserted&&it->second.value!=status)it->second.consistent=false;
    }else failures[scope][pos].consistent=false;
   }
  }
  std::optional<Detection> best;
  auto consider=[&](const std::string& scope,std::int64_t left,std::int64_t right,double effect,
                    const std::string& kind,const std::string& failure){
   if(!best||effect>best->effect){
    best=Detection{left,right,effect,kind,case_params(exemplars.at(scope)),scope,failure};
   }
  };
  for(const auto& [scope,groups]:failures){
   for(auto l=groups.begin();l!=groups.end();++l)for(auto r=std::next(l);r!=groups.end();++r){
    const auto detect_status=[&](auto ok,auto bad){
     const auto& failure=bad->second.value;
     const bool correctness=failure=="wrong_answer"||failure=="signaled"||failure=="crashed";
     const bool resource=failure=="timeout"||failure=="out_of_memory";
     if(ok->second.consistent&&bad->second.consistent&&ok->second.value=="ok"&&(correctness||resource))
      consider(scope,ok->first,bad->first,1000000.0,
       correctness?"CORRECTNESS_BOUNDARY":"MEMORY_OR_RESOURCE_BOUNDARY",failure);
    };
    detect_status(l,r);detect_status(r,l);
   }
  }
  for(const auto& [scope,groups]:measurements){
   for(auto l=groups.begin();l!=groups.end();++l)for(auto r=std::next(l);r!=groups.end();++r){
    if(invalid_performance[scope].count(l->first)||invalid_performance[scope].count(r->first))continue;
    const auto a=sample_median(l->second),b=sample_median(r->second);
    const auto effect=std::abs(b-a);
    if(effect<std::log(cfg.min_ratio))continue;
    const bool crossover=!cfg.b.empty()&&cfg.family!="calibration"&&a*b<0;
    const auto kind=cfg.family=="calibration"?"CATEGORICAL_SYNTHETIC_CALIBRATION":
                    crossover?"CATEGORICAL_PERFORMANCE_CROSSOVER":"CATEGORICAL_PERFORMANCE_CONTRAST";
    consider(scope,l->first,r->first,effect,kind,"");
   }
  }
  return best;
 }

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
// Multi-change discovery scores are exploratory only. They neither supply
// p-values nor convert the finite-range segmentation into a proof of Big-O.
static std::vector<Detection> detect_multi_v2(Store& store,const Config& cfg){
 std::map<std::string,std::map<std::int64_t,std::vector<double>>> cohorts;
 std::map<std::string,Case> representatives;
 for(const auto& c:store.cases(false)){
  if(const auto value=metric(store,c,cfg,false)){
   const auto scope=scope_key(c,cfg);
   cohorts[scope][axis_value(c,cfg.axis)].push_back(*value);
   representatives.emplace(scope,c);
  }
 }
 std::vector<Detection> found;
 for(const auto& [scope,points]:cohorts){
  if(points.size()<6)continue;
  std::vector<std::int64_t> x;std::vector<double> y;
  for(const auto& [pos,values]:points){x.push_back(pos);y.push_back(sample_median(values));}
  std::vector<double> gradients;
  for(std::size_t j=1;j<y.size();++j)gradients.push_back(std::abs(y[j]-y[j-1]));
  const auto baseline=sample_median(gradients);
  const auto penalty=std::max(1e-6,4.0*baseline*baseline*std::log(static_cast<double>(y.size())));
  const auto fit=cfg.detector_version=="multi_change_pelt_v2"?
   fit_multi_change_pelt_v2(y,2,3,penalty):fit_multi_change_v2(y,2,3,penalty);
  for(const auto at:fit.changes){
   const auto effect=std::abs(y[at]-y[at-1]);
   std::vector<double> alternatives;
   for(std::size_t j=1;j<y.size();++j)if(j!=at)alternatives.push_back(std::abs(y[j]-y[j-1]));
   const auto background=sample_median(alternatives);
   if(effect<std::log(cfg.min_ratio)||effect<2.5*background)continue;
   found.push_back(Detection{x[at-1],x[at],effect,
    cfg.family=="calibration"?"SYNTHETIC_CALIBRATION":"PERFORMANCE_CLIFF",
    case_params(representatives.at(scope)),scope,""});
  }
 }
 std::stable_sort(found.begin(),found.end(),[](const auto& a,const auto& b){return a.effect>b.effect;});
 return found;
}

static std::size_t adaptive_next(Store& store,const Config& cfg,const std::vector<Case>& pool,const std::vector<bool>& chosen,std::uint64_t& random_state){
 std::vector<std::size_t> available;available.reserve(pool.size());
 for(std::size_t i=0;i<pool.size();i++)if(!chosen[i])available.push_back(i);
 if(available.empty())throw Error("adaptive scheduler exhausted");
 const auto count=pool.size()-available.size();
 if(count==0)return numeric_axis(cfg.axis)?available.front():
   available[static_cast<std::size_t>(randbelow(random_state,available.size()))];
 if(count==1&&numeric_axis(cfg.axis))return available.back();
 /* Deterministic fraction schedule: floor(t*p) quota increments imply
    exactly floor(N*p) exploration proposals after N selections.
    This preserves restart decisions independently of timing noise. */
 const auto quota=[](std::size_t n,double fraction){return static_cast<std::size_t>(std::floor(static_cast<double>(n)*fraction+1e-9));};
 if(quota(count+1,cfg.exploration_fraction)>quota(count,cfg.exploration_fraction))return available[static_cast<std::size_t>(randbelow(random_state,available.size()))];
 if(!numeric_axis(cfg.axis)){
  // A categorical adaptive policy must not compare labels by distance.
  // Prefer a new category inside a previously measured conditioned scope,
  // then a new scope/category; explicitly reserve the configured quota for
  // random exploration as in numeric adaptive search.
  std::map<std::string,std::map<std::string,std::size_t>> coverage;
  for(std::size_t i=0;i<pool.size();i++)if(chosen[i])
   ++coverage[scope_key(pool[i],cfg)][axis_label(pool[i],cfg.axis)];
  std::size_t best=available.front();double best_score=-1;
  for(const auto index:available){
   const auto scope=scope_key(pool[index],cfg);
   const auto label=axis_label(pool[index],cfg.axis);
   const auto found=coverage.find(scope);
   const std::size_t group_count=found==coverage.end()?0:found->second.count(label)?found->second.at(label):0;
   const auto scope_count=found==coverage.end()?0:found->second.size();
   const double score=100.0/(1.0+static_cast<double>(group_count))+
     (scope_count>0?10.0:0.0);
   if(score>best_score){best=index;best_score=score;}
  }
  return best;
 }
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
// Discovery-only conditional 2x2 screen. All four nominal cells must be
// independently sampled under every replicate seed; missing cells abstain.
static void v2_factorial_diagnostic(Store& store,const Config& cfg){
 if(cfg.axes.size()!=2||cfg.replicate_count<3)return;
 auto first=cfg.axes.begin(),second=std::next(first);
 if(first->second.size()!=2||second->second.size()!=2)return;
 std::vector<FactorialCellV2> cells;
 for(const auto& c:store.cases(false)){
  auto m=metric(store,c,cfg,false);
  std::string status="ok";
  if(!m){
   status="incomplete";
   for(const auto& obs:store.observations(c.id,false))
    if(obs.algorithm==cfg.a&&obs.status!="ok"){status=obs.status;break;}
  }
  cells.push_back(FactorialCellV2{axis_label(c,first->first),axis_label(c,second->first),c.seed,m,status});
 }
 const auto result=screen_factorial_v2(
  {first->second[0],first->second[1]},
  {second->second[0],second->second[1]},cells,3,std::log(cfg.min_ratio));
 Json record;record.put("classification",result.kind);
 record.put("effect",result.median_contrast);
 record.put("complete_cohorts",static_cast<std::int64_t>(result.complete_cohorts));
 record.put("incomplete_cohorts",static_cast<std::int64_t>(result.incomplete_cohorts));
 record.put("failure_status",result.failure_status);
 record.put("axis_a",first->first);record.put("axis_b",second->first);
 store.event("v2_factorial_screen_discovery_only",record.str());
}

// V2 uniform baseline: virtual candidate indices, no Cartesian materialization.
// Case IDs and RNG are checked against the committed decision transcript.
static void execute_lazy_v2(const Config& cfg,const std::string& out,Store& store,std::size_t pause_after){
 const AdmissibleSearchSpace space(std::vector<LazySearchSpace::Axis>(cfg.axes.begin(),cfg.axes.end()),cfg.admissible_subspaces);
 const auto replicas=static_cast<std::uint64_t>(cfg.replicate_count);
 if(space.cardinality()>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())/replicas)
  throw Error("CARDINALITY_OVERFLOW: v2 coordinate/replicate universe");
 const auto virtual_count=space.cardinality()*replicas;
 SparsePermutation sampler(virtual_count,cfg.seed^0xb7d2f937c4abULL);
 std::optional<StratifiedIndexSamplerV2> stratified;
 if(cfg.method=="stratified_v2")
  stratified.emplace(std::vector<LazySearchSpace::Axis>(cfg.axes.begin(),cfg.axes.end()),
    cfg.admissible_subspaces,cfg.axis,replicas,cfg.seed^0xb4ad19f14acULL);
 std::optional<HybridMixedPolicyV2> hybrid;
 if(cfg.method=="hybrid_mixed_v2")
  hybrid.emplace(std::vector<LazySearchSpace::Axis>(cfg.axes.begin(),cfg.axes.end()),
   cfg.admissible_subspaces,cfg.axis,replicas,cfg.seed^0x827d391bULL,
   cfg.exploration_fraction,numeric_axis(cfg.axis));
 std::optional<HierarchicalPolicyV2> hierarchy;
 if(cfg.method=="hierarchical_boundary_v2")
  hierarchy.emplace(static_cast<std::size_t>(space.cardinality()),cfg.seed^0xd9af2e13ULL,cfg.exploration_fraction);
 const auto rng_state=[&](){return hybrid?hybrid->rng_state():(hierarchy?hierarchy->rng_state():(stratified?stratified->state():sampler.state()));};
 const auto cpu_ms=[&](const Case& c){
  double ms=0;
  for(const auto& o:store.observations(c.id,false))
   if(o.cpu_ns>0)ms+=static_cast<double>(o.cpu_ns)/1000000.;
  return std::max(1.,ms);
 };
 auto add_evidence=[&](std::uint64_t i,const Case& c){
  if(hierarchy)hierarchy->observe(static_cast<std::size_t>(i),metric(store,c,cfg,false),cpu_ms(c));
  if(hybrid)hybrid->observe(i,metric(store,c,cfg,false),cpu_ms(c));
 };
 auto next_valid=[&]()->std::optional<std::pair<std::uint64_t,Case>>{
  if(hybrid){
   for(std::size_t attempts=0;attempts<10000;++attempts){
    const auto index=hybrid->next();
    if(!index)return std::nullopt;
    Case c=cfg.base;c.seed=cfg.seed+(*index%replicas);
    for(const auto& [axis,value]:space.at(*index/replicas))assign(c,axis,value);
    try{validate_case_schema(c,cfg.max_bytes);}
    catch(const Error&){hybrid->reject(*index);continue;}
    return std::pair<std::uint64_t,Case>{*index,std::move(c)};
   }
   throw Error("v2 hybrid exceeded deterministic invalid-proposal cap");
  }
  if(hierarchy){
   const auto pos=hierarchy->next();if(!pos)return std::nullopt;
   Case c=cfg.base;c.seed=cfg.seed;
   for(const auto& [axis,value]:space.at(*pos))assign(c,axis,value);
   validate_case_schema(c,cfg.max_bytes);
   return std::pair<std::uint64_t,Case>{static_cast<std::uint64_t>(*pos),std::move(c)};
  }
  for(std::size_t attempts=0;attempts<10000&&(stratified?stratified->has_next():sampler.has_next());++attempts){
   auto index=stratified?stratified->next():sampler.next();Case c=cfg.base;c.seed=cfg.seed+(index%replicas);
   for(const auto& [axis,value]:space.at(index/replicas))assign(c,axis,value);
   try{validate_case_schema(c,cfg.max_bytes);}catch(const Error&){continue;}
   return std::pair<std::uint64_t,Case>{index,std::move(c)};
  }
  if(stratified?stratified->has_next():sampler.has_next())throw Error("v2 exceeded deterministic invalid-proposal cap");
  return std::nullopt;
 };
 std::size_t completed=0;
 std::uint64_t sampled_cpu_used=0;
 bool sampled_cpu_unknown=false;
 const auto account_samples=[&](const std::vector<Observation>& observations){
  if(!cfg.max_sampled_cpu_ns)return;
  for(const auto& o:observations){
   if(o.status!="ok"||o.cpu_ns<0){sampled_cpu_unknown=true;continue;}
   const auto charge=static_cast<std::uint64_t>(o.cpu_ns);
   if(charge>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())-sampled_cpu_used)
    throw Error("v2 sampled worker CPU budget accounting overflow");
   sampled_cpu_used+=charge;
  }
 };
 for(const auto& d:store.scheduler_decisions()){
  auto next=next_valid();
  if(!next||d.pool_index<0||static_cast<std::uint64_t>(d.pool_index)!=next->first||
     d.next_rng_state!=rng_state()||make_case(next->second,cfg.max_bytes).id!=d.case_id)
   throw Error("v2 immutable scheduler replay mismatch");
  add_evidence(next->first,make_case(next->second,cfg.max_bytes));
  account_samples(store.observations(d.case_id,false));
  if(cfg.max_sampled_cpu_ns)
   store.verify_v2_budget_entry(d.pool_index,d.case_id,sampled_cpu_used,cfg.max_sampled_cpu_ns);
  ++completed;
 }
 if(completed>cfg.max_cases)throw Error("v2 persisted decisions exceed case budget");
 if(sampled_cpu_unknown)throw Error("v2 sampled worker CPU budget is unaccountable: incomplete or invalid timed observations preserved");
 bool sampled_cpu_limit=cfg.max_sampled_cpu_ns>0&&sampled_cpu_used>=cfg.max_sampled_cpu_ns;
 // A finalized budget-capped experiment is terminal across subsequent resumes.
 if(sampled_cpu_limit){
  sqlite3_stmt* stmt=nullptr;
  if(sqlite3_prepare_v2(store.raw(),"SELECT 1 FROM events WHERE event='search_complete' LIMIT 1",-1,&stmt,nullptr)!=SQLITE_OK)
   throw Error("cannot inspect v2 budget-complete evidence");
  const bool already_completed=sqlite3_step(stmt)==SQLITE_ROW;
  sqlite3_finalize(stmt);
  if(already_completed){report(out);return;}
 }
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(cfg.total_wall_s);
 bool budget_hit=false,exhausted=false;std::size_t this_call=0;
 while(completed<cfg.max_cases&&!sampled_cpu_limit&&(pause_after==0||this_call<pause_after)){
  if(std::chrono::steady_clock::now()>=deadline){budget_hit=true;break;}
  auto next=next_valid();
  if(!next){exhausted=true;break;}
  auto c=make_case(next->second,cfg.max_bytes);std::vector<Observation> obs;
  try{
   obs=measure(c,cfg.b.empty()?std::vector<std::string>{cfg.a}:std::vector<std::string>{cfg.a,cfg.b},
    cfg.blocks,cfg.warmup,cfg.sample_ms,cfg.rss_mb,cfg.minimum_batch_ms,
    cfg.isolation_mode=="per_sample",deadline,cfg.measurement_protocol_version);
  }catch(const Error& e){
   if(std::string(e.what()).find("total_wall_seconds budget exhausted")!=std::string::npos){budget_hit=true;break;}
   throw;
  }
  if(std::chrono::steady_clock::now()>=deadline){budget_hit=true;break;}
  account_samples(obs);
  store.exec("BEGIN IMMEDIATE");
  try{
   for(const auto& o:obs)store.add_observation(c,o,false);
   store.record_decision(static_cast<std::size_t>(next->first),c.id,rng_state());
   if(cfg.max_sampled_cpu_ns)
    store.record_v2_budget_entry(c.id,sampled_cpu_used,cfg.max_sampled_cpu_ns,!sampled_cpu_unknown);
   if(hierarchy)store.event("v2_acquisition",std::to_string(next->first)+";origin="+hierarchy->origin());
   if(hybrid)store.event("v2_acquisition",std::to_string(next->first)+";origin="+hybrid->origin());
   if(stratified)store.event("v2_acquisition",std::to_string(next->first)+";origin=stratified_primary_axis");
   if(cfg.max_sampled_cpu_ns){
    Json cost;cost.put("sampled_cpu_ns",static_cast<std::int64_t>(sampled_cpu_used));
    cost.put("limit_ns",static_cast<std::int64_t>(cfg.max_sampled_cpu_ns));
    cost.put("accounting_complete",!sampled_cpu_unknown);
    store.event("v2_sampled_cpu_budget_ledger",cost.str());
   }
   store.exec("COMMIT");
  }catch(...){store.exec("ROLLBACK");throw;}
  add_evidence(next->first,c);
  ++completed;++this_call;
  if(sampled_cpu_unknown){report(out);throw Error("v2 sampled worker CPU budget cannot account invalid/unknown timed samples; preserved");}
  if(cfg.max_sampled_cpu_ns&&sampled_cpu_used>=cfg.max_sampled_cpu_ns){
   sampled_cpu_limit=true;break;
  }
 }
 if(completed==cfg.max_cases||exhausted||sampled_cpu_limit){
  v2_factorial_diagnostic(store,cfg);
  std::optional<Detection> selected=detect(store,cfg);
  std::vector<Detection> ranked_multi;
  if(cfg.detector_version=="multi_change_v2"||cfg.detector_version=="multi_change_pelt_v2"){
   ranked_multi=detect_multi_v2(store,cfg);
   std::size_t rank=0;
   for(const auto& c:ranked_multi){
    Json evidence;evidence.put("rank",static_cast<std::int64_t>(++rank));
    evidence.put("left",c.left);evidence.put("right",c.right);
    evidence.put("effect",c.effect);evidence.put("scope",c.scope);
    evidence.put("model_version",cfg.detector_version);
    store.event("v2_multiple_change_discovery_only",evidence.str());
   }
   if(!ranked_multi.empty()&&(!selected||
      (selected->kind!="CORRECTNESS_BOUNDARY"&&
       selected->kind!="MEMORY_OR_RESOURCE_BOUNDARY"&&
       selected->kind!="PERFORMANCE_CROSSOVER"&&
       ranked_multi.front().effect>=selected->effect)))
    selected=ranked_multi.front();
  }
  if(auto d=selected){
   store.hypothesis("H1","OBSERVED",cfg.axis,d->left,d->right,d->effect,
    "Exploratory v2 candidate, not independently confirmed",d->kind,d->frozen_params);
   store.hypothesis("H1","HYPOTHESIZED",cfg.axis,d->left,d->right,d->effect,
    "V2 discovery; frozen independent holdout required",d->kind,d->frozen_params);
   // Up to 15 further distinct geometric boundaries remain HYPOTHESIZED,
   // never automatically confirmed. All must be predeclared together before
   // holdout in a family manifest; same-geometry scopes cannot currently
   // claim independent conditional effects.
   std::set<std::pair<std::int64_t,std::int64_t>> geometry;
   geometry.emplace(d->left,d->right);
   std::size_t next_id=2;
   for(const auto& candidate:ranked_multi){
    if(next_id>16)break;
    if(!geometry.emplace(candidate.left,candidate.right).second)continue;
    const auto id="H"+std::to_string(next_id++);
    store.hypothesis(id,"OBSERVED",cfg.axis,candidate.left,candidate.right,
      candidate.effect,"Exploratory ranked multi-change candidate; untested",
      candidate.kind,candidate.frozen_params);
    store.hypothesis(id,"HYPOTHESIZED",cfg.axis,candidate.left,candidate.right,
      candidate.effect,"Frozen discovery rank; independent holdout and campaign multiplicity required",
      candidate.kind,candidate.frozen_params);
   }
  }else store.hypothesis("H1","NO_CONFIRMED_BOUNDARY",cfg.axis,0,0,0,
    "No convincing isolated structural discontinuity under declared thresholds");
  if(sampled_cpu_limit)store.event("v2_sampled_cpu_limit_reached",
   std::to_string(sampled_cpu_used)+";limit="+std::to_string(cfg.max_sampled_cpu_ns));
  store.event("search_complete",std::to_string(completed));
 }else{
  store.event("search_paused",std::to_string(completed));
  if(budget_hit)store.event("wall_budget_exhausted",std::to_string(cfg.total_wall_s));
 }
 std::cout<<"V2 explored "<<completed<<" from virtual "<<virtual_count<<" coordinate/replicate candidates; "<<store.summary().str()<<"\n";
 report(out);
 if(budget_hit)throw Error("total_wall_seconds budget exhausted; completed cases preserved; resume the partial run");
}

static void execute_schedule(const Config& cfg,const std::string& out,Store& store,std::size_t pause_after){
 if(cfg.policy_version=="v2"){execute_lazy_v2(cfg,out,store,pause_after);return;}
 auto choices=scheduled(cfg);std::vector<bool> chosen(choices.size(),false);
 const bool feedback=cfg.method=="adaptive_partition";
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
 const auto hard_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(cfg.total_wall_s);
 bool budget_hit=false;std::size_t this_call=0;
 while(completed<cfg.max_cases&&completed<choices.size()&&(pause_after==0||this_call<pause_after)){
  if(std::chrono::steady_clock::now()>=hard_deadline){budget_hit=true;break;}
  const auto index=feedback?adaptive_next(store,cfg,choices,chosen,rng):completed;
  if(chosen[index])throw Error("scheduler selected previously evaluated case");
  auto c=make_case(choices[index],cfg.max_bytes);
  std::vector<Observation> obs;
  try{
   obs=measure(c,cfg.b.empty()?std::vector<std::string>{cfg.a}:std::vector<std::string>{cfg.a,cfg.b},cfg.blocks,cfg.warmup,cfg.sample_ms,cfg.rss_mb,cfg.minimum_batch_ms,cfg.isolation_mode=="per_sample",hard_deadline,cfg.measurement_protocol_version);
  }catch(const Error& error){
   if(std::string(error.what()).find("total_wall_seconds budget exhausted")!=std::string::npos){budget_hit=true;break;}
   throw;
  }
  if(std::chrono::steady_clock::now()>=hard_deadline){budget_hit=true;break;}
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
   store.hypothesis("H1","OBSERVED",cfg.axis,d->left,d->right,d->effect,
      "Candidate observed before hypothesis freezing",d->kind,d->frozen_params);
   store.hypothesis("H1","HYPOTHESIZED",cfg.axis,d->left,d->right,d->effect,"Exploratory candidate, one frozen hypothesis with independent holdout; conditioned scope="+d->scope+"; failure_status="+d->failure_status,d->kind,d->frozen_params);
  }else store.hypothesis("H1","NO_CONFIRMED_BOUNDARY",cfg.axis,0,0,0,"No convincing isolated structural discontinuity under declared thresholds");
  store.event("search_complete",std::to_string(completed));
 }else {store.event("search_paused",std::to_string(completed));
  if(budget_hit)store.event("wall_budget_exhausted",std::to_string(cfg.total_wall_s));}
 std::cout<<"Explored "<<completed<<" / "<<std::min<std::size_t>(cfg.max_cases,choices.size())<<" cases; "<<store.summary().str()<<"\n";
 report(out);
 if(budget_hit)throw Error("total_wall_seconds budget exhausted; completed cases preserved; resume the partial run");
}
void explore(const Config& cfg,const std::string& out,std::size_t pause_after){
 if(fs::exists(out+"/run.sqlite"))throw Error("output run already exists (will not overwrite evidence)");
 fs::create_directories(out);Store s(out+"/run.sqlite");if(cfg.policy_version=="v2")s.upgrade_v2_schema();s.experiment(config_json(cfg).str(),environment());
 execute_schedule(cfg,out,s,pause_after);
}
void resume(const std::string& run,std::size_t pause_after){
 if(!fs::exists(run+"/run.sqlite"))throw Error("run database missing for resume");
 Store s(run+"/run.sqlite");const auto cfg=config_from_json(Json::parse(s.config()));if(cfg.policy_version=="v2")s.upgrade_v2_schema();
 sqlite3_stmt* st=nullptr;
 if(sqlite3_prepare_v2(s.raw(),"SELECT COUNT(*) FROM hypotheses",-1,&st,nullptr)!=SQLITE_OK)throw Error("hypothesis lookup failed");
 auto rc=sqlite3_step(st);const auto done=rc==SQLITE_ROW?sqlite3_column_int64(st,0):0;sqlite3_finalize(st);
 if(done){
  // Narrow opt-in exception: a fully sealed sampled-CPU-budget stop is
  // idempotently resumable as a no-op, never as a new discovery write.
  if(cfg.policy_version=="v2"&&cfg.max_sampled_cpu_ns>0){
   sqlite3_stmt* check=nullptr;
   if(sqlite3_prepare_v2(s.raw(),
     "SELECT COUNT(DISTINCT event) FROM events WHERE event IN ('v2_sampled_cpu_limit_reached','search_complete')",
     -1,&check,nullptr)!=SQLITE_OK)throw Error("budget finalization evidence query failed");
   const auto step=sqlite3_step(check);
   const bool terminal=step==SQLITE_ROW&&sqlite3_column_int(check,0)==2;
   sqlite3_finalize(check);
   if(terminal){execute_lazy_v2(cfg,run,s,0);return;}
  }
  throw Error("experiment has finalized hypothesis; resume would overwrite evidence");
 }
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
 // Distinct hypothesis IDs must never share a fixed holdout seed stream.
 // Keep H1 bitwise compatible; all v2 H2..H16 use disjoint 1e9-wide windows
 // for primary, failure-status and negative-control seed cohorts.
 std::uint64_t hypothesis_seed_offset=0;
 if(cfg.policy_version=="v2"&&id!="H1"){
  if(id.size()<2||id[0]!='H')throw Error("v2 hypothesis must be H1..H16");
  std::size_t parsed=0;std::uint64_t rank=0;
  try{rank=std::stoull(id.substr(1),&parsed);}
  catch(const std::exception&){throw Error("unsupported v2 hypothesis identity");}
  if(parsed!=id.size()-1||rank<2||rank>16)
   throw Error("v2 hypothesis rank out of predeclared bounds");
  hypothesis_seed_offset=(rank-1)*1000000000ULL;
 }
 // Reject unsafe negative-control and paired holdout seeds without mutating
 // a frozen hypothesis or retaining an incomplete overflowed population.
 if(cfg.policy_version=="v2")
  validate_v2_confirmation_seed_space(cfg.seed,
   hypothesis_seed_offset/1000000000ULL+1ULL,cfg.confirm_blocks);
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(cfg.total_wall_s);
 const auto prescribed_seeds=[&](bool failure){
  std::vector<std::uint64_t> seeds;
  if(cfg.policy_version!="v2")return seeds;
  for(int i=0;i<cfg.confirm_blocks;++i)
   seeds.push_back(cfg.seed+hypothesis_seed_offset+(failure?7000000ULL:1000000ULL)+
    static_cast<std::uint64_t>(i)*(failure?101ULL:97ULL));
  return seeds;
 };
 if(status=="NO_CONFIRMED_BOUNDARY"){std::cout<<"No candidate to confirm\n";return;}
 if(status!="HYPOTHESIZED")throw Error("confirmation already attempted or candidate not eligible: "+status);
 if(cfg.policy_version=="v2"){
  sqlite3_stmt* members=nullptr;
  if(sqlite3_prepare_v2(store.raw(),
   "SELECT COUNT(*) FROM hypotheses WHERE status!='NO_CONFIRMED_BOUNDARY'",-1,&members,nullptr)!=SQLITE_OK)
   throw Error("cannot inspect v2 frozen candidate family");
  const auto state=sqlite3_step(members);
  const auto candidates=state==SQLITE_ROW?sqlite3_column_int64(members,0):0;
  sqlite3_finalize(members);
  if(state!=SQLITE_ROW)throw Error("cannot count v2 frozen candidate family");
  if(candidates>1||id!="H1"){
   // A multiple-candidate confirmation is invalid unless its
   // predeclared, immutable Holm family is frozen before holdout.
   sqlite3_stmt* family=nullptr;
   if(sqlite3_prepare_v2(store.raw(),
    "SELECT c.state FROM campaign_members_v2 m JOIN campaigns_v2 c"
    " ON c.campaign_id=m.campaign_id WHERE m.hypothesis_id=?",-1,&family,nullptr)!=SQLITE_OK){
    // SQLite v8 doesn't have campaign tables: fail closed.
    throw Error("v2 multi-hypothesis confirmation requires frozen campaign");
   }
   sqlite3_bind_text(family,1,id.c_str(),-1,SQLITE_TRANSIENT);
   const auto row=sqlite3_step(family);
   const bool campaign_is_frozen=row==SQLITE_ROW&&
     std::string(reinterpret_cast<const char*>(sqlite3_column_text(family,0)))=="FROZEN";
   sqlite3_finalize(family);
   if(!campaign_is_frozen)throw Error("v2 multi-hypothesis confirmation requires frozen campaign membership");
  }
 }
 /* Preserve the original candidate's frozen failure subtype/scope when
    entering TESTED: failure confirmations parse that exact evidence. */
 sqlite3_stmt* detail_stmt=nullptr;
 if(sqlite3_prepare_v2(store.raw(),"SELECT detail FROM hypotheses WHERE id=?",-1,&detail_stmt,nullptr)!=SQLITE_OK)throw Error("candidate detail query failed");
 sqlite3_bind_text(detail_stmt,1,id.c_str(),-1,SQLITE_TRANSIENT);
 if(sqlite3_step(detail_stmt)!=SQLITE_ROW){sqlite3_finalize(detail_stmt);throw Error("candidate detail missing");}
 const auto original_detail=std::string(reinterpret_cast<const char*>(sqlite3_column_text(detail_stmt,0)));
 sqlite3_finalize(detail_stmt);
 store.hypothesis(id,"TESTED",axis,left,right,0,original_detail+"; Holdout started with frozen geometry");
 try{
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
  bool all_valid=true;int completed_pairs=0;
  for(int i=0;i<cfg.confirm_blocks&&all_valid;i++){
   const auto seed=cfg.seed+hypothesis_seed_offset+7000000ULL+static_cast<std::uint64_t>(i)*101ULL;
   for(int side=0;side<2;side++){
    if(std::chrono::steady_clock::now()>=deadline)throw Error("total_wall_seconds budget exhausted during confirmation");
    Case c=frozen;assign(c,axis,frozen_axis_value(cfg,side==0?left:right));
    c.seed=seed;c=make_case(c,cfg.max_bytes);
    if(prior.count(c.id)){all_valid=false;break;}
    auto observations=measure(c,cfg.b.empty()?std::vector<std::string>{cfg.a}:
       std::vector<std::string>{cfg.a,cfg.b},1,cfg.warmup,cfg.sample_ms,cfg.rss_mb,cfg.minimum_batch_ms,cfg.isolation_mode=="per_sample",deadline,cfg.measurement_protocol_version);
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
  // A separate seed-matched baseline control must remain oracle-valid.
  // A categorical axis has no notion of "farther", so recheck the first
  // category rather than inventing a categorical distance metric.
  if(all_valid){
   Case control=frozen;
   assign(control,axis,frozen_axis_value(cfg,left));
   control.seed=cfg.seed+hypothesis_seed_offset+30000000ULL;control=make_case(control,cfg.max_bytes);
   if(prior.count(control.id))all_valid=false;
   else{
    const auto control_rows=measure(control,cfg.b.empty()?std::vector<std::string>{cfg.a}:
      std::vector<std::string>{cfg.a,cfg.b},1,cfg.warmup,cfg.sample_ms,cfg.rss_mb,
      cfg.minimum_batch_ms,cfg.isolation_mode=="per_sample",deadline,cfg.measurement_protocol_version);
    for(const auto& row:control_rows){
     store.add_observation(control,row,true);
     if(row.status!="ok")all_valid=false;
    }
    if(!metric(store,control,cfg,true))all_valid=false;
   }
  }
  const auto verdict=all_valid&&completed_pairs==cfg.confirm_blocks?"CONFIRMED":"NOT_CONFIRMED";
  const auto evidence="Fresh seed-separated failure-status cases="+std::to_string(completed_pairs)+
      " pairs expected="+std::to_string(cfg.confirm_blocks)+"; left=ok right="+subtype+
      "; categorical status, censored timing not a speedup; no finite ratio inferred"+
      "; independent baseline negative-control oracle_valid="+(all_valid?"true":"false");
  // The seal and final status commit as one SQLite transaction.
  store.exec("BEGIN IMMEDIATE");
  try{
   if(std::string(verdict)=="CONFIRMED")
    store.seal_confirmation(id,cfg.a,cfg.confirm_blocks,false,prescribed_seeds(true));
   store.hypothesis(id,verdict,axis,left,right,0,evidence);
   store.exec("COMMIT");
  }catch(...){store.exec("ROLLBACK");throw;}
  report(run);std::cout<<verdict<<" "<<evidence<<"\n";return;
 }
 std::array<std::vector<double>,2> side_estimates;
 std::vector<double> paired_differences;
 std::set<std::string> discovery_ids;for(const auto& c:store.cases(false))discovery_ids.insert(c.id);
 auto reject=[&](const std::string& why){store.hypothesis(id,"NOT_CONFIRMED",axis,left,right,0,why);report(run);};
 for(int i=0;i<cfg.confirm_blocks;i++){
  const auto seed=cfg.seed+hypothesis_seed_offset+1000000ULL+static_cast<std::uint64_t>(i)*97ULL;
  std::array<double,2> estimates{};
  for(int order=0;order<2;order++){
   const auto side=static_cast<std::size_t>((i+order)%2);
   if(std::chrono::steady_clock::now()>=deadline)throw Error("total_wall_seconds budget exhausted during confirmation");
   Case c=frozen;assign(c,axis,frozen_axis_value(cfg,side==0?left:right));c.seed=seed;c=make_case(c,cfg.max_bytes);
   if(discovery_ids.count(c.id)){reject("Held-out input overlaps discovery: confirmation invalid");return;}
   auto obs=measure(c,cfg.b.empty()?std::vector<std::string>{cfg.a}:std::vector<std::string>{cfg.a,cfg.b},1,cfg.warmup,cfg.sample_ms,cfg.rss_mb,cfg.minimum_batch_ms,cfg.isolation_mode=="per_sample",deadline,cfg.measurement_protocol_version);
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
 std::optional<double> campaign_p_value;
 if(cfg.family=="calibration"){
  confirmed=std::abs(delta)>=threshold;
  detail<<"Independent seed-separated deterministic operation-count holdout; pairs="<<cfg.confirm_blocks<<". NOT a timing/algorithm discovery.";
 }else{
  const auto interval=paired_case_bootstrap(paired_differences,cfg.seed^0x5b86d2a7c9e3ULL,cfg.confirmation_confidence);
  const auto ci_l=case_bootstrap(side_estimates[0],cfg.seed^0x7ac6314fe29ULL,cfg.confirmation_confidence);
  const auto ci_r=case_bootstrap(side_estimates[1],cfg.seed^0x87f3e195c1eULL,cfg.confirmation_confidence);
  const auto p=paired_sign_flip_p(paired_differences,cfg.seed^0xb3970a42e86ULL^hypothesis_seed_offset);
  if(cfg.policy_version=="v2")campaign_p_value=p;
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
 /* Predeclared negative-control gate: one new independent case must pass
    the same oracle/worker validity checks as the two primary holdout sides.
    The control is *not* required to have a particular performance magnitude:
    a distant category/size may legitimately have another regime.  A failed
    or censored control vetoes confirmation; never ignore a failed worker. */
 {
  bool has_distant=false;
  std::string selected;
  if(!numeric_axis(axis)){
   const auto& labels=cfg.axes.at(axis);
   for(std::size_t j=0;j<labels.size();j++)
    if(static_cast<std::int64_t>(j)!=left&&static_cast<std::int64_t>(j)!=right){
     selected=labels[j];has_distant=true;break;
    }
  }else if(cfg.axes.count(axis)){
   const auto& values=cfg.axes.at(axis);
   std::int64_t most_distance=-1;
   for(const auto& value:values){
    Case option=frozen;assign(option,axis,value);
    const auto coordinate=axis_value(option,axis);
    if(coordinate==left||coordinate==right)continue;
    const auto distance=static_cast<std::int64_t>(std::min(std::abs(coordinate-left),std::abs(coordinate-right)));
    if(distance>most_distance){most_distance=distance;selected=value;has_distant=true;}
   }
  }
  Case c=frozen;
  if(has_distant)assign(c,axis,selected);
  else selected=axis_label(frozen,axis); // no third point exists in this design
  c.seed=cfg.seed+hypothesis_seed_offset+30000000ULL;c=make_case(c,cfg.max_bytes);
  if(discovery_ids.count(c.id)){
   confirmed=false;detail<<"; negative-control invalid: discovery overlap";
  }else{
   const auto observations=measure(c,cfg.b.empty()?std::vector<std::string>{cfg.a}:std::vector<std::string>{cfg.a,cfg.b},
     1,cfg.warmup,cfg.sample_ms,cfg.rss_mb,cfg.minimum_batch_ms,cfg.isolation_mode=="per_sample",deadline,cfg.measurement_protocol_version);
   for(const auto& o:observations)store.add_observation(c,o,true);
   const auto valid=metric(store,c,cfg,true).has_value();
   if(!valid)confirmed=false;
   detail<<"; negative-control="<<(has_distant?"independent-distant-design-point":"independent-repeat-no-third-point")
         <<" axis_value="<<axis_label(c,axis)<<" case="<<c.id
         <<" oracle_worker_valid="<<(valid?"true":"false")
         <<" veto_if_invalid=true";
  }
 }
 // Keep sealed positive status and family p-value indivisible. A failed SQL
 // attestation may preserve raw holdout evidence but not a positive verdict.
 store.exec("BEGIN IMMEDIATE");
 try{
  if(confirmed)store.seal_confirmation(id,cfg.a,cfg.confirm_blocks,false,prescribed_seeds(false));
  store.hypothesis(id,confirmed?"CONFIRMED":"NOT_CONFIRMED",axis,left,right,std::abs(delta),detail.str()+(confirmed?"":"; prespecified evidence threshold not met"));
  if(confirmed&&campaign_p_value)
   record_campaign_signflip_v2(store,id,*campaign_p_value,cfg.confirm_blocks);
  store.exec("COMMIT");
 }catch(...){store.exec("ROLLBACK");throw;}
 report(run);std::cout<<store.summary().str()<<"\n";
 }catch(const Error& error){
  if(std::string(error.what()).find("total_wall_seconds budget exhausted")!=std::string::npos){
   store.hypothesis(id,"NOT_CONFIRMED",axis,left,right,0,
      "Independent confirmation exceeded declared total wall time; partial samples preserved");
   store.event("wall_budget_exhausted","confirmation");report(run);
  }
  throw;
 }
}
void ablate(const Config& cfg,const std::string& out){
 if(!numeric_axis(cfg.axis))throw Error("ablation requires an ordered numeric primary axis; categorical axes must be conditioned on");
 if(fs::exists(out))throw Error("ablation output already exists");
 fs::create_directories(out);
 std::ostringstream csv;csv<<"method,case_budget,discovery_cases,total_cases,raw_samples,seconds,status,left,right,localization_error,worker_cpu_seconds,completion_fraction,confirmed_discovery_yield,null_false_positive,witness_byte_reduction_percent\n";
 Json scorecard;scorecard.put("fixture_family",cfg.family);scorecard.put("methods_evaluated",static_cast<std::int64_t>(4));
 scorecard.put("budget_kind","equal requested discovery case budget; actual worker CPU and wall time separately recorded");
 auto* evaluations=json_object_new_array();int confirmed_findings=0,null_false_positives=0;
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
  sqlite3_stmt* cpu=nullptr;
  if(sqlite3_prepare_v2(store.raw(),"SELECT COALESCE(SUM(cpu_ns),0) FROM samples WHERE cpu_ns IS NOT NULL",-1,&cpu,nullptr)!=SQLITE_OK)
   throw Error("ablation CPU accounting query failed");
  if(sqlite3_step(cpu)!=SQLITE_ROW){sqlite3_finalize(cpu);throw Error("ablation CPU accounting unavailable");}
  const auto cpu_seconds=static_cast<double>(sqlite3_column_int64(cpu,0))/1000000000.0;
  sqlite3_finalize(cpu);
  const auto complete=static_cast<double>(discovery_cases)/static_cast<double>(std::min<std::uint64_t>(c.max_cases,scheduled(c).size()));
  const bool found=status=="CONFIRMED"||status=="MINIMIZED_CONFIRMED";
  const bool null_control=cfg.family=="calibration"&&cfg.base.negative;
  if(found)++confirmed_findings;
  if(null_control&&found)++null_false_positives;
  const auto yield=found?1:0;
  const auto false_positive=null_control?(found?"1":"0"):"NA";
  std::string witness_reduction="NA";
  if(cfg.family=="calibration"&&found&&right==cfg.base.threshold&&left<right){
   // Execute actual synthetic-witness minimization, then compare persisted
   // original right-side bytes and the exact verified witness bytes.
   minimize(path);
   std::size_t original=0;
   for(const auto& case_item:store.cases(false))if(axis_value(case_item,cfg.axis)==right){
    if(original==0||case_item.bytes.size()<original)original=case_item.bytes.size();
   }
   sqlite3_stmt* witness_row=nullptr;
   if(sqlite3_prepare_v2(store.raw(),"SELECT MIN(LENGTH(c.input)) FROM witnesses w JOIN cases c ON c.case_id=w.case_id WHERE w.status='MINIMIZED_CONFIRMED'",-1,&witness_row,nullptr)!=SQLITE_OK)
    throw Error("ablation witness size query failed");
   if(sqlite3_step(witness_row)==SQLITE_ROW&&sqlite3_column_type(witness_row,0)!=SQLITE_NULL&&original>0){
    const auto smallest=static_cast<std::size_t>(sqlite3_column_int64(witness_row,0));
    if(smallest<=original)witness_reduction=std::to_string(100.0*static_cast<double>(original-smallest)/static_cast<double>(original));
   }
   sqlite3_finalize(witness_row);
  }
  Json record;record.put("method",name);record.put("confirmed_discovery_yield",static_cast<std::int64_t>(yield));
  record.put("null_false_positive",false_positive);record.put("witness_byte_reduction_percent",witness_reduction);
  record.put("total_cases",summary.i("cases"));record.put("worker_cpu_seconds",cpu_seconds);
  record.put("wall_seconds",sec);record.put("completion_fraction",complete);
  json_object_array_add(evaluations,record.p);record.p=nullptr;
  csv<<name<<','<<c.max_cases<<','<<discovery_cases<<','<<summary.i("cases")<<','<<summary.i("samples")<<','<<sec<<','<<status<<','<<left<<','<<right<<','<<err<<','<<cpu_seconds<<','<<complete<<','<<yield<<','<<false_positive<<','<<witness_reduction<<'\n';
 }
 json_object_object_add(scorecard.p,"method_results",evaluations);
 scorecard.put("confirmed_discovery_yield_sum",static_cast<std::int64_t>(confirmed_findings));
 if(cfg.family=="calibration"&&cfg.base.negative)scorecard.put("null_false_positive_count",static_cast<std::int64_t>(null_false_positives));
 scorecard.put("witness_reduction_limit","witness reduction percentages are measured only for independently minimized cases; NA denotes unmeasured or ineligible cases");
 write_file(out+"/ablation_scorecard.json",scorecard.str());
 write_file(out+"/ablation.csv",csv.str());
 write_file(out+"/README.md","# Equal-budget search comparison\n\nRows summarize grid, random, stratified and adaptive runs with identical user-specified evaluation and wall budgets. `localization_error=NA` for real algorithms because no ground-truth boundary is assumed. Rows include actual discovery case counts to expose incomplete runs; equal requested budgets do not guarantee equal completed counts or CPU time. The worker_cpu_seconds column is the sum of sampled worker process CPU time when available, not scheduler CPU time or a hardware-normalized speedup. The scorecard reports positive discovery yield and deterministic-null false positives where a ground truth exists. Witness byte shrink is computed from persisted original/witness input bytes for independently minimized synthetic findings, and NA when no such reduction was run; no reduction is invented. All raw cases and timing measurements remain in each method subdirectory.\n");
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
 const auto artifacts=witness_output_root(run,id,cfg.policy_version).string();
 if(fs::exists(artifacts+"/minimization.sqlite")||fs::exists(artifacts+"/witness.json"))
  throw Error("minimization evidence already exists; refusing to overwrite");
 fs::create_directories(artifacts);
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
 Store evidence(artifacts+"/minimization.sqlite");evidence.experiment(config_json(cfg).str(),environment());
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
    Case c=candidate;assign(c,axis,frozen_axis_value(cfg,side==0?left:right));
    c.seed=base_seed+static_cast<std::uint64_t>(block)*97ULL;
    try{c=make_case(c,cfg.max_bytes);}catch(const Error&){return false;}
    auto observations=measure(c,cfg.b.empty()?std::vector<std::string>{cfg.a}:std::vector<std::string>{cfg.a,cfg.b},
                              1,cfg.warmup,cfg.sample_ms,cfg.rss_mb,cfg.minimum_batch_ms,cfg.isolation_mode=="per_sample",deadline,cfg.measurement_protocol_version);
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
 record.put("minimization_evidence",witness_evidence_relative(id,cfg.policy_version));
 auto* path=json_object_new_array();
 if(initially_valid)for(const auto& step:result.trace){Json item;item.put("case_id",step.case_id);
  item.put("input_bytes",static_cast<std::int64_t>(step.input_bytes));item.put("action",step.action);
  item.put("preserved",step.predicate_preserved);json_object_array_add(path,item.p);item.p=nullptr;}
 json_object_object_add(record.p,"shrink_trace",path);
 source.add_case(smallest);source.witness(id,smallest.id,status,record.str());
 source.hypothesis(id,status,axis,left,right,0, "Performance witness "+std::string(status)+"; independent minimization evidence in minimization.sqlite; raw observation and null outcomes retained");
 write_file(artifacts+"/witness.json",record.str());write_file(artifacts+"/witness.hex",hex_encode(smallest.bytes)+"\n");
 if(cfg.a.rfind("plugin:",0)!=0)write_file(artifacts+"/witness.cpp",standalone_reproducer(smallest,cfg.a));
 report(run);
 std::cout<<"Performance witness "<<status<<" case="<<smallest.id<<" evaluated="<<attempts<<"\n";
}

/* An unordered category has no scalar midpoint. Minimize only shared
   nuisance structure, then re-run BOTH frozen labels on fresh matched seeds.
   Every accepted candidate is independently rechecked; no timing result is
   ever inferred from the numeric indices used to identify the two labels. */
static void minimize_categorical(const std::string& run,const std::string& id,Store& source,
                                 const Config& cfg,std::int64_t left,std::int64_t right){
 const auto artifacts=witness_output_root(run,id,cfg.policy_version).string();
 if(fs::exists(artifacts+"/minimization.sqlite")||fs::exists(artifacts+"/witness.json"))
  throw Error("categorical minimization evidence already exists; refusing to overwrite");
 fs::create_directories(artifacts);
 sqlite3_stmt* raw=nullptr;
 if(sqlite3_prepare_v2(source.raw(),"SELECT kind FROM hypotheses WHERE id=?",-1,&raw,nullptr)!=SQLITE_OK)
  throw Error("categorical minimization kind lookup failed");
 std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)> statement(raw,&sqlite3_finalize);
 sqlite3_bind_text(statement.get(),1,id.c_str(),-1,SQLITE_TRANSIENT);
 if(sqlite3_step(statement.get())!=SQLITE_ROW)throw Error("categorical minimization hypothesis missing");
 const std::string kind=reinterpret_cast<const char*>(sqlite3_column_text(statement.get(),0));
 const bool failure=kind=="CORRECTNESS_BOUNDARY"||kind=="MEMORY_OR_RESOURCE_BOUNDARY";
 const bool calibration=kind=="CATEGORICAL_SYNTHETIC_CALIBRATION";
 if(!failure&&!calibration&&kind!="CATEGORICAL_PERFORMANCE_CONTRAST"&&
    kind!="CATEGORICAL_PERFORMANCE_CROSSOVER")throw Error("categorical minimization kind unsupported: "+kind);
 const auto frozen=frozen_case(source,cfg,id);
 const auto scope=scope_key(frozen,cfg);
 std::vector<double> observed_left,observed_right;
 std::string failure_status;
 for(const auto& candidate:source.cases(false)){
  if(scope_key(candidate,cfg)!=scope)continue;
  const auto label=axis_label(candidate,cfg.axis);
  if(label==categorical_value(cfg,left)){
   if(auto m=metric(source,candidate,cfg,false))observed_left.push_back(*m);
  }else if(label==categorical_value(cfg,right)){
   if(auto m=metric(source,candidate,cfg,false))observed_right.push_back(*m);
   if(failure)for(const auto& observation:source.observations(candidate.id,false))
    if(observation.algorithm==cfg.a&&observation.status!="ok"){
     if(!failure_status.empty()&&failure_status!=observation.status)
      throw Error("categorical failure subtype is inconsistent in discovery evidence");
     failure_status=observation.status;
    }
  }
 }
 if(failure&&failure_status.empty())throw Error("categorical failure lacks a frozen observed failure status");
 if(!failure&&(observed_left.empty()||observed_right.empty()))
  throw Error("categorical contrast lacks two admissible discovery categories");
 const double expected_shift=failure?0.0:sample_median(observed_right)-sample_median(observed_left);
 if(!failure&&std::abs(expected_shift)<std::log(cfg.min_ratio))throw Error("categorical discovery contrast is not practically significant");
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(std::min(120,cfg.total_wall_s));
 Store evidence(artifacts+"/minimization.sqlite");evidence.experiment(source.config(),environment());
 Case initial=frozen;assign(initial,cfg.axis,categorical_value(cfg,right));initial.seed=cfg.seed+50000000ULL;
 initial=make_case(initial,cfg.max_bytes);
 std::size_t attempts=0,successful=0;
 auto check=[&](const Case& parameters,const std::string& stage)->bool{
  if(std::chrono::steady_clock::now()>=deadline)throw Error("categorical witness total_wall_seconds budget exhausted");
  const auto nonce=attempts++;
  const int pair_count=20;
  std::vector<double> differences;
  for(int i=0;i<pair_count;i++){
   std::array<std::optional<double>,2> values;
   for(int side=0;side<2;side++){
    Case c=parameters;assign(c,cfg.axis,categorical_value(cfg,side==0?left:right));
    c.seed=cfg.seed+50000000ULL+static_cast<std::uint64_t>(nonce)*100000ULL+static_cast<std::uint64_t>(i)*101ULL;
    c=make_case(c,cfg.max_bytes);
    const auto names=cfg.b.empty()?std::vector<std::string>{cfg.a}:std::vector<std::string>{cfg.a,cfg.b};
    const auto obs=measure(c,names,1,cfg.warmup,cfg.sample_ms,cfg.rss_mb,
      cfg.minimum_batch_ms,cfg.isolation_mode=="per_sample",deadline,cfg.measurement_protocol_version);
    bool admissible=obs.size()==(cfg.b.empty()?1U:4U);
    bool seen_primary=false;
    for(const auto& row:obs){
     evidence.add_observation(c,row,true);
     if(row.algorithm==cfg.a){
      seen_primary=true;
      if(row.status!=(failure&&side==1?failure_status:"ok"))admissible=false;
     }else if(row.algorithm==cfg.b&&row.status!="ok")admissible=false;
    }
    if(!admissible||!seen_primary){evidence.event("categorical_minimization_rejected",stage+": worker failure");return false;}
    if(!failure){values[static_cast<std::size_t>(side)]=metric(evidence,c,cfg,true);
     if(!values[static_cast<std::size_t>(side)])return false;}
   }
   if(!failure)differences.push_back(*values[1]-*values[0]);
  }
  bool preserved=true;
  if(!failure){
   if(calibration){
    const auto change=sample_median(differences);
    preserved=change*expected_shift>0&&std::abs(change)>=std::log(cfg.min_ratio);
   }else{
    const auto interval=paired_case_bootstrap(differences,cfg.seed+attempts*101ULL,0.99);
    const auto probability=paired_sign_flip_p(differences,cfg.seed+attempts*397ULL);
    const auto threshold=std::log(cfg.min_ratio);
    preserved=probability<=std::min(0.01,cfg.fdr_q)&&
      (expected_shift>0?interval.lower>threshold:interval.upper< -threshold);
   }
  }
  if(preserved)successful++;
  evidence.event("categorical_minimization_check",stage+": "+(preserved?"preserved":"rejected")+
                 "; independent seed pairs="+std::to_string(pair_count));
  return preserved;
 };
 ShrinkResult reduced;reduced.original=initial;reduced.smallest=initial;
 bool baseline=false,validated=false;
 try{
  baseline=check(initial,"baseline_independent_revalidation");
  if(baseline){
   reduced=shrink_case(initial,[&](const Case& candidate){return check(candidate,"pair_preserving_candidate");},8,cfg.max_bytes);
   validated=check(reduced.smallest,"final_independent_revalidation");
  }
 }catch(const std::exception& error){evidence.event("categorical_minimization_error",error.what());}
 const auto& smallest=baseline?reduced.smallest:initial;
 const auto verdict=validated&&reduced.reduced?"MINIMIZED_CONFIRMED":
                    validated?"MINIMIZATION_FAILED":"MINIMIZED_PROVISIONAL";
 Json record;record.put("schema_version",static_cast<std::int64_t>(1));
 record.put("category","CATEGORICAL_PAIR_WITNESS");record.put("hypothesis_id",id);
 record.put("kind",kind);record.put("status",verdict);
 record.put("axis",cfg.axis);record.put("left_label",categorical_value(cfg,left));
 record.put("right_label",categorical_value(cfg,right));
 record.put("left_category_index",left);record.put("right_category_index",right);
 record.put("left_right_are_unordered",true);
 record.put("baseline_revalidated",baseline);record.put("final_revalidated",validated);
 record.put("expected_failure_status",failure_status);
 record.put("original_case_id",initial.id);record.put("case_id",smallest.id);
 record.put("original_input_bytes",static_cast<std::int64_t>(initial.bytes.size()));
 record.put("input_bytes",static_cast<std::int64_t>(smallest.bytes.size()));
 record.put("input_sha256",sha256(smallest.bytes));
 record.put("parameters",Json::parse(case_params(smallest)));
 record.put("evaluations",static_cast<std::int64_t>(attempts));
 record.put("preserved_evaluations",static_cast<std::int64_t>(successful));
 record.put("fresh_seed_pairs_per_evaluation",static_cast<std::int64_t>(20));
 record.put("minimization_evidence",witness_evidence_relative(id,cfg.policy_version));
 auto* steps=json_object_new_array();for(const auto& step:reduced.trace){
  Json v;v.put("case_id",step.case_id);v.put("action",step.action);
  v.put("input_bytes",static_cast<std::int64_t>(step.input_bytes));
  v.put("preserved",step.predicate_preserved);
  json_object_array_add(steps,v.p);v.p=nullptr;
 }
 json_object_object_add(record.p,"shrink_trace",steps);
 source.add_case(smallest);source.witness(id,smallest.id,verdict,record.str());
 source.hypothesis(id,verdict,cfg.axis,left,right,std::abs(expected_shift),
   "Unordered-category pair witness "+std::string(verdict)+
   "; labels retained; independent evidence in minimization.sqlite",kind,case_params(frozen));
 write_file(artifacts+"/witness.json",record.str());write_file(artifacts+"/witness.hex",hex_encode(smallest.bytes)+"\n");
 report(run);
 std::cout<<"Categorical pair witness "<<verdict<<" hypothesis="<<id
          <<" fresh_pair_evaluations="<<attempts<<"\n";
}

static void minimize_failure_case_to(const std::string& run,
 const std::string& case_id,const std::string& algorithm,const std::string& artifacts);
void minimize(const std::string& run,const std::string& id){
 Store s(run+"/run.sqlite");auto cfg=config_from_json(Json::parse(s.config()));auto [status,axis,left,right]=read_hypothesis(s,id);
 if(status!="CONFIRMED")throw Error("only confirmed hypotheses can be minimized");
 if(!numeric_axis(axis)){minimize_categorical(run,id,s,cfg,left,right);return;}
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
    const auto artifacts=witness_output_root(run,id,cfg.policy_version).string();
    fs::create_directories(artifacts);
    minimize_failure_case_to(run,original->id,cfg.a,artifacts);
    const auto record=Json::file(artifacts+"/witness.json");
    const auto verdict=record.s("status");
    s.witness(id,record.s("case_id"),verdict,record.str());
    s.hypothesis(id,verdict,axis,left,right,0,"Freshly replayed wrong-answer witness and shrink trace; earlier independent confirmation retained");
    report(run);return;
   }
   /* Crash/timeout/OOM witnesses require exact failure-mode preservation.
      Never turn censored failures into synthetic large timing values. */
   const auto artifacts=witness_output_root(run,id,cfg.policy_version).string();
   if(fs::exists(artifacts+"/witness.json")||fs::exists(artifacts+"/witness.hex"))
    throw Error("witness artifacts already exist");
   fs::create_directories(artifacts);
   const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(std::min(120,cfg.total_wall_s));
   auto predicate=[&](const Case& input)->bool{
    if(std::chrono::steady_clock::now()>deadline)throw Error("failure shrink time budget exhausted");
    const auto obs=measure(input,{cfg.a},1,0,cfg.sample_ms,cfg.rss_mb,cfg.minimum_batch_ms,cfg.isolation_mode=="per_sample",deadline,cfg.measurement_protocol_version);
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
   write_file(artifacts+"/witness.json",record.str());write_file(artifacts+"/witness.hex",hex_encode(shrunk.smallest.bytes)+"\n");
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
struct RecordedAlgorithm {
 std::string version,output_contract;
 int abi=0;
};
static std::optional<RecordedAlgorithm> recorded_algorithm(Store& store,const std::string& algorithm){
 sqlite3_stmt* raw=nullptr;
 if(sqlite3_prepare_v2(store.raw(),"SELECT version,adapter_abi,output_contract FROM algorithms WHERE algorithm_id=?",-1,&raw,nullptr)!=SQLITE_OK)
  throw Error("algorithm provenance query failed");
 std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)> stmt(raw,&sqlite3_finalize);
 if(sqlite3_bind_text(stmt.get(),1,algorithm.c_str(),-1,SQLITE_TRANSIENT)!=SQLITE_OK)
  throw Error("algorithm provenance binding failed");
 const auto rc=sqlite3_step(stmt.get());
 if(rc==SQLITE_DONE)return std::nullopt;
 if(rc!=SQLITE_ROW)throw Error("algorithm provenance lookup failed");
 auto value=[&](int column){const auto* bytes=sqlite3_column_text(stmt.get(),column);
  return bytes?std::string(reinterpret_cast<const char*>(bytes)):std::string{};};
 return RecordedAlgorithm{value(0),value(2),sqlite3_column_int(stmt.get(),1)};
}
static void require_replay_identity(Store& store,const std::string& algorithm,const std::string& family){
 if(algorithm.rfind("plugin:",0)!=0)return;
 const auto recorded=recorded_algorithm(store,algorithm);
 if(!recorded||recorded->abi!=1||recorded->output_contract!=family+":exact-oracle-values"||
    recorded->version.rfind("external-abi-v1;sha256=",0)!=0)
  throw Error("ARTIFACT_MISMATCH: missing or incompatible native-plugin algorithm provenance");
 const auto path=algorithm.substr(7);
 if(!fs::is_regular_file(path))throw Error("UNAVAILABLE_PLUGIN: native-plugin artifact is not present: "+path);
 if(recorded->version!="external-abi-v1;sha256="+sha256_file(path))
  throw Error("ARTIFACT_MISMATCH: native-plugin file SHA-256 differs from persisted run: "+path);
}
static void minimize_failure_case_to(const std::string& run,const std::string& case_id,const std::string& algorithm,
 const std::string& artifacts){
 if(algorithm.empty())throw Error("minimize-case requires an explicit algorithm");
 Store store(run+"/run.sqlite");const auto cfg=config_from_json(Json::parse(store.config()));
 const auto origin=locate_case(store,case_id);
 bool observed=false;
 for(const auto& o:store.observations(origin.id,false))
  if(o.algorithm==algorithm&&(o.status=="wrong_answer"||o.status=="signaled"||o.status=="crashed"))observed=true;
 if(!observed)throw Error("cannot shrink unrecorded or successful correctness case");
 if(fs::exists(artifacts+"/witness.json")||fs::exists(artifacts+"/witness.hex"))throw Error("witness artifacts already exist; refusing to overwrite");
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(std::min(cfg.total_wall_s,120));
 auto predicate=[&](const Case& candidate){
  if(std::chrono::steady_clock::now()>deadline)throw Error("witness minimization wall budget exhausted");
  /* Worker validation includes oracle comparison before collecting timing. */
  const auto result=measure(candidate,{algorithm},1,0,cfg.sample_ms,cfg.rss_mb,cfg.minimum_batch_ms,cfg.isolation_mode=="per_sample",deadline,cfg.measurement_protocol_version);
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
 write_file(artifacts+"/witness.json",record.str());write_file(artifacts+"/witness.hex",hex_encode(result.smallest.bytes)+"\n");
 report(run);
 std::cout<<"Correctness witness "<<verdict<<" case="<<result.smallest.id<<" original="<<origin.bytes.size()
          <<" bytes minimal="<<result.smallest.bytes.size()<<" bytes\n";
}
void minimize_failure_case(const std::string& run,const std::string& case_id,const std::string& algorithm){
 minimize_failure_case_to(run,case_id,algorithm,run);
}
void replay_expected_failure(const std::string& run,const std::string& case_id,const std::string& algorithm,const std::string& expected_status,const std::string& db){
 static const std::set<std::string> accepted={"wrong_answer","timeout","out_of_memory","crashed","signaled"};
 if(!accepted.count(expected_status)||algorithm.empty())throw Error("expected-failure replay requires an explicit algorithm and supported status");
 Store store(db.empty()?run+"/run.sqlite":db,true);auto c=locate_case(store,case_id);
 require_replay_identity(store,algorithm,c.family);
 for(const auto& o:measure(c,{algorithm},1,0,3000,1024)){
  if(o.status!=expected_status)throw Error("witness failure did not reproduce: expected "+expected_status+" got "+o.status);
  std::cout<<"REPLAY EXPECTED_FAILURE status="<<o.status<<" case="<<c.id<<" input_sha256="<<sha256(c.bytes)<<"\n";
  return;
 }
 throw Error("empty witness replay");
}
void replay(const std::string& run,const std::string& case_id,const std::string& db){
 Store store(db.empty()?run+"/run.sqlite":db,true);
 const auto cfg=config_from_json(Json::parse(store.config()));
 const auto c=locate_case(store,case_id); // Exact or uniquely prefixed ID; never first-match an ambiguous prefix.
 require_replay_identity(store,cfg.a,c.family);
 if(cfg.a.rfind("plugin:",0)==0){
  // All external code executes inside the same bounded worker as ordinary
  // measurements. Replaying never modifies the historical SQLite observations.
  const auto obs=measure(c,{cfg.a},1,0,cfg.sample_ms,cfg.rss_mb,cfg.minimum_batch_ms,
                         cfg.isolation_mode=="per_sample");
  if(obs.size()!=1)throw Error("REPLAY_FAILED: native plugin did not return exactly one observation");
  if(obs.front().status=="wrong_answer")throw Error("WRONG_ANSWER: native plugin disagrees with independently regenerated oracle");
  if(obs.front().status!="ok"||obs.front().ns<=0||obs.front().batch<=0)
   throw Error("REPLAY_FAILED: native plugin status="+obs.front().status+" "+obs.front().detail);
  const auto expected_hash=sha256(encode_values(oracle(c).values));
  if(obs.front().digest!=expected_hash)throw Error("WRONG_ANSWER: native-plugin output digest disagrees with oracle");
 }else{
  if(execute(c,cfg.a).values!=oracle(c).values)throw Error("WRONG_ANSWER: replay correctness failure");
 }
 std::cout<<"REPLAY OK case="<<c.id<<" input_sha256="<<sha256(c.bytes)<<" algorithm="<<cfg.a<<"\n";
}
static std::string csv_escape(const std::string& s){std::string o="\"";for(char c:s){if(c=='\"')o+="\"\"";else o+=c;}return o+"\"";}
static std::string html_escape(const std::string& s){std::string o;for(char c:s){if(c=='&')o+="&amp;";else if(c=='<')o+="&lt;";else if(c=='>')o+="&gt;";else if(c=='\"')o+="&quot;";else o+=c;}return o;}
void report(const std::string& run,const std::string& formats){
 std::set<std::string> selected;
 static const std::set<std::string> supported={"markdown","json","csv","html"};
 std::size_t pos=0;
 do{
  const auto end=formats.find(',',pos);const auto format=formats.substr(pos,end==std::string::npos?end:end-pos);
  if(!supported.count(format)||!selected.insert(format).second)throw Error("unsupported or duplicate report --format: "+format);
  if(end==std::string::npos)break;
  pos=end+1;
 }while(true);
 Store s(run+"/run.sqlite");s.refresh_aggregates();auto summary=s.summary();auto cfg=config_from_json(Json::parse(s.config()));
 // Render category identifiers with their original labels; the stored
 // integer indices are exact design references, never metric coordinates.
 if(!numeric_axis(cfg.axis)&&cfg.axes.count(cfg.axis)){
  if(auto* hypotheses=summary.get("hypotheses"))
   for(std::size_t i=0;i<json_object_array_length(hypotheses);i++){
    Json h(json_object_get(json_object_array_get_idx(hypotheses,i)));
    if(h.s("status")!="NO_CONFIRMED_BOUNDARY"){
     h.put("left_label",categorical_value(cfg,h.i("left")));
     h.put("right_label",categorical_value(cfg,h.i("right")));
    }
   }
 }
 Json report;report.put("schema_version",static_cast<std::int64_t>(1));report.put("experiment_id",cfg.experiment_id);
 if(cfg.policy_version=="v2")report.put("measurement_protocol_version",cfg.measurement_protocol_version);
 report.put("family",cfg.family);report.put("algorithm_a",cfg.a);report.put("algorithm_b",cfg.b);report.put("environment",Json::parse(s.environment()));report.put("summary",std::move(summary));
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
 if(selected.count("json"))write_file(run+"/report.json",report.str());
 if(selected.count("csv"))write_file(run+"/samples.csv",csv.str());
 auto* summary_obj=report.get("summary");Json summ(json_object_get(summary_obj));
 std::ostringstream md;md<<"# PHASESHIFT experimental report\n\n- Experiment: "<<cfg.experiment_id<<"\n- Family: "<<cfg.family<<"\n- Comparison: "<<cfg.a<<(cfg.b.empty()?"":" vs "+cfg.b)<<"\n- Discovered cases and holdouts: "<<summ.i("cases")<<" total\n- Raw observations: "<<summ.i("samples")<<"\n- Incorrect, timeout, crash or invalid observations: "<<summ.i("invalid_samples")<<"\n- Primary axis: "<<cfg.axis<<"\n- Full raw evidence: `samples.csv`, `report.json`, `run.sqlite`\n\n## Boundary candidates and verdicts\n\n";
 auto* hypotheses=summ.get("hypotheses");for(std::size_t i=0;i<json_object_array_length(hypotheses);i++){
  Json x(json_object_get(json_object_array_get_idx(hypotheses,i)));md<<"- "<<x.s("id")<<": **"<<x.s("status")<<"**; between "<<(x.get("left_label")?x.s("left_label"):std::to_string(x.i("left")))
    <<" and "<<(x.get("right_label")?x.s("right_label"):std::to_string(x.i("right")))
    <<" (effect="<<x.d("effect")<<"). "<<x.s("detail")<<"\n";
 }
 md<<"\n## Export completeness\n\n- Raw CSV/JSON projection "<<(truncated?"TRUNCATED by max_report_bytes; authoritative complete raw data remain in run.sqlite":"contains every recorded sample")<<".\n\n## Scientific limitations\n\n- Synthetic work units are not time; this fixture is an engineered calibration, not a natural discovery.\n- CPU timings vary by host, cache, thermal conditions, and compiler settings.\n- Real performance candidates are **not confirmed** without calibrated paired-case intervals and multiplicity controls.\n- Observational morphology alone does not establish causality.\n- Reproduce with `phaseshift replay --run "<<run<<" --case <case-id>` and inspect raw rows.\n";
 if(selected.count("markdown"))write_file(run+"/report.md",md.str());
 if(selected.count("html"))write_file(run+"/report.html","<!doctype html><meta charset=\"utf-8\"><title>PHASESHIFT report</title><style>body{font:16px system-ui;max-width:900px;margin:3em auto;white-space:pre-wrap}</style><main>"+html_escape(md.str())+"</main>");
 for(const auto& format:selected){
  const std::string filename=format=="markdown"?"report.md":format=="json"?"report.json":format=="csv"?"samples.csv":"report.html";
  s.artifact(filename,sha256_file(run+"/"+filename),"experiment-report");
 }
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
void intervene(const std::string& source,const std::string& out,const std::string& id,int seed_pairs){
 if(seed_pairs<3||seed_pairs>100)throw Error("matched intervention requires 3..100 independent seed pairs");
 if(fs::exists(out))throw Error("intervention output already exists");
 Store prior(source+"/run.sqlite");const auto cfg=config_from_json(Json::parse(prior.config()));
 const auto [status,axis,left,right]=read_hypothesis(prior,id);
 if(status=="NO_CONFIRMED_BOUNDARY"||status=="NOT_CONFIRMED"||status=="INVALIDATED")
  throw Error("matched intervention requires a frozen non-rejected boundary hypothesis");
 if(!numeric_axis(axis))throw Error("categorical intervention is not an ordered numeric boundary");
 const auto frozen=frozen_case(prior,cfg,id);
 fs::create_directories(out);Store samples(out+"/run.sqlite");
 auto new_config=cfg;new_config.experiment_id="intervention-"+cfg.experiment_id;
 samples.experiment(config_json(new_config).str(),environment());
 const auto algorithms=cfg.b.empty()?std::vector<std::string>{cfg.a}:std::vector<std::string>{cfg.a,cfg.b};
 Json evidence;evidence.put("purpose","matched structural intervention; controls are observational diagnostics");
 evidence.put("source_experiment",cfg.experiment_id);evidence.put("source_hypothesis",id);
 evidence.put("source_status",status);evidence.put("axis",axis);evidence.put("left",left);evidence.put("right",right);
 evidence.put("intended_nuisance_parameters_equal",true);evidence.put("causal_claim",false);
 evidence.put("independent_seed_pairs_requested",static_cast<std::int64_t>(seed_pairs));
 auto* pairs=json_object_new_array();std::vector<double> paired_log_contrasts;
 std::set<std::string> source_ids;for(const auto& c:prior.cases())source_ids.insert(c.id);
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(cfg.total_wall_s);
 std::size_t complete=0;
 bool budget_exhausted=false;
 for(int i=0;i<seed_pairs;i++){
  if(std::chrono::steady_clock::now()>=deadline){budget_exhausted=true;break;}
  const auto seed=cfg.seed+40000000ULL+static_cast<std::uint64_t>(i)*99991ULL;
  std::array<Case,2> generated;
  std::array<std::optional<double>,2> metrics;
  bool valid=true;
  for(int side=0;side<2;side++){
   Case case_def=frozen;case_def.seed=seed;assign(case_def,axis,formatted_axis_value(axis,side?right:left));
   generated[static_cast<std::size_t>(side)]=make_case(case_def,cfg.max_bytes);
   if(source_ids.count(generated[static_cast<std::size_t>(side)].id))throw Error("intervention case overlaps discovery or confirmation");
   std::vector<Observation> observations;
   try{
    observations=measure(generated[static_cast<std::size_t>(side)],algorithms,
     1,cfg.warmup,cfg.sample_ms,cfg.rss_mb,cfg.minimum_batch_ms,cfg.isolation_mode=="per_sample",deadline,cfg.measurement_protocol_version);
   }catch(const Error& error){
    if(std::string(error.what()).find("total_wall_seconds budget exhausted")!=std::string::npos){
     budget_exhausted=true;break;
    }
    throw;
   }
   for(const auto& observation:observations)samples.add_observation(generated[static_cast<std::size_t>(side)],observation,true);
   metrics[static_cast<std::size_t>(side)]=metric(samples,generated[static_cast<std::size_t>(side)],cfg,true);
   if(!metrics[static_cast<std::size_t>(side)])valid=false;
  }
  if(budget_exhausted)break;
  Json pair;pair.put("seed",static_cast<std::int64_t>(seed));pair.put("left_case_id",generated[0].id);pair.put("right_case_id",generated[1].id);
  pair.put("measurements_valid",valid);
  const auto l=Json::parse(morphology_features(generated[0]).str());
  const auto h=Json::parse(morphology_features(generated[1]).str());
  auto* changed=json_object_new_array();
  json_object_object_foreach(l.p,key,value){
   auto* rhs=h.get(key);
   if(!rhs||std::string(json_object_to_json_string_ext(value,JSON_C_TO_STRING_PLAIN))!=
     json_object_to_json_string_ext(rhs,JSON_C_TO_STRING_PLAIN)){
    json_object_array_add(changed,json_object_new_string(key));
   }
  }
  json_object_object_add(pair.p,"observed_feature_differences",changed);
  if(valid){
   const auto contrast=*metrics[1]-*metrics[0];
   pair.put("log_right_minus_left",contrast);paired_log_contrasts.push_back(contrast);
   complete++;
  }
  json_object_array_add(pairs,pair.p);pair.p=nullptr;
 }
 evidence.put("valid_seed_pairs",static_cast<std::int64_t>(complete));
 evidence.put("wall_budget_exhausted",budget_exhausted);
 json_object_object_add(evidence.p,"paired_controls",pairs);
 if(paired_log_contrasts.size()>=3){
  evidence.put("median_log_contrast",sample_median(paired_log_contrasts));
  if(paired_log_contrasts.size()>=20){
   const auto ci=paired_case_bootstrap(paired_log_contrasts,cfg.seed^0x511f00dULL,cfg.confirmation_confidence);
   const auto p=paired_sign_flip_p(paired_log_contrasts,cfg.seed^0x127badeULL);
   evidence.put("case_paired_ci_lower",ci.lower);evidence.put("case_paired_ci_upper",ci.upper);
   evidence.put("signflip_p",p);
  }
 }
 evidence.put("verdict",complete==static_cast<std::size_t>(seed_pairs)?"MATCHED_MEASUREMENTS_COMPLETE":"INCOMPLETE_OR_INVALID");
 evidence.put("limitation","Changing one requested morphology axis may change multiple observed input features; this is not causal identification. Matched seeds and unmodified nuisance parameters are necessary but not sufficient for causality.");
 write_file(out+"/interventions.json",evidence.str());
 samples.event("matched_intervention",std::to_string(complete)+" of "+std::to_string(seed_pairs)+" independent seed pairs");
 if(budget_exhausted)samples.event("wall_budget_exhausted","intervention");
 report(out);
 if(budget_exhausted)throw Error("total_wall_seconds budget exhausted during intervention; partial evidence preserved");
 std::cout<<"Matched intervention: "<<complete<<"/"<<seed_pairs<<" seed pairs; causal claim withheld; "<<out<<"\n";
}
void compare(const std::string& a,const std::string& b){Store left(a+"/run.sqlite"),right(b+"/run.sqlite");auto x=config_from_json(Json::parse(left.config())),y=config_from_json(Json::parse(right.config()));
 if(x.family!=y.family||x.a!=y.a||x.b!=y.b)throw Error("incompatible algorithm output contracts");
 // A plugin path is not an implementation identity. Reject mixed binary
 // versions rather than treating a same-path comparison as reproducible.
 for(const auto& algorithm:(x.b.empty()?std::vector<std::string>{x.a}:std::vector<std::string>{x.a,x.b})){
  const auto lhs=recorded_algorithm(left,algorithm),rhs=recorded_algorithm(right,algorithm);
  const bool is_plugin=algorithm.rfind("plugin:",0)==0;
  if(is_plugin&&(!lhs||!rhs))throw Error("ARTIFACT_MISMATCH: native-plugin provenance missing from one run: "+algorithm);
  if(lhs&&rhs&&(lhs->version!=rhs->version||lhs->abi!=rhs->abi||lhs->output_contract!=rhs->output_contract))
   throw Error("ARTIFACT_MISMATCH: recorded algorithm identity/output contract differs for "+algorithm+
               " A="+lhs->version+" B="+rhs->version);
  if(is_plugin&&(lhs->abi!=1||lhs->output_contract!=x.family+":exact-oracle-values"))
   throw Error("ARTIFACT_MISMATCH: native-plugin ABI/output contract invalid for "+algorithm);
 }
 std::cout<<"ALGORITHM_IDENTITY: RECORDED_ARTIFACTS_MATCH\n";
 std::cout<<"A: "<<left.summary().str()<<"\nB: "<<right.summary().str()<<"\n";
 const auto env_a=Json::parse(left.environment()),env_b=Json::parse(right.environment());
 const std::vector<std::string> compatibility={"architecture","cpu_model","compiler_id","compiler_version","stdlib","stdlib_version","build_type","build_flags","source_revision","cpu_affinity","cpu_governor","process_priority","sanitized_build"};
 int missing=0,mismatched=0;
 for(const auto& field:compatibility){
  auto* l=env_a.get(field.c_str());auto* r=env_b.get(field.c_str());
  if(!l||!r){std::cout<<"UNAVAILABLE: "<<field<<"\n";missing++;continue;}
  if(json_object_to_json_string_ext(l,JSON_C_TO_STRING_PLAIN)!=std::string(json_object_to_json_string_ext(r,JSON_C_TO_STRING_PLAIN))){
   std::cout<<"MISMATCH: "<<field<<"\n";mismatched++;
  }
 }
 std::cout<<"ENVIRONMENT_COMPATIBILITY: "<<(mismatched?"MISMATCH":missing?"INCOMPLETE":"METADATA_MATCH")
          <<" mismatched_fields="<<mismatched<<" missing_fields="<<missing<<"\n";
 std::cout<<"WARNING: matching metadata cannot establish governor stability, thermal equivalence, workload identity or causal speedup.\n";
}
}
