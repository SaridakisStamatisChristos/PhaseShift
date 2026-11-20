#include "phaseshift/experiment.hpp"
#include <algorithm>
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
static std::string value_string(json_object* v){if(!v)throw Error("null axis value");auto t=json_object_get_type(v);if(t==json_type_string)return json_object_get_string(v);if(t==json_type_int)return std::to_string(json_object_get_int64(v));throw Error("axis values must be strings or integers");}
static void assign(Case& c,const std::string& key,const std::string& v){
 if(key=="distribution"){c.distribution=v;return;}if(key=="weight_class"){c.weight_class=v;return;}
 std::int64_t n;try{std::size_t p=0;n=std::stoll(v,&p);if(p!=v.size())throw Error("invalid numeric axis");}catch(const std::exception&){throw Error("invalid numeric parameter "+key+"="+v);}
 if(key=="x")c.x=n;else if(key=="n")c.n=n;else if(key=="k")c.k=n;else if(key=="vertices")c.vertices=n;else if(key=="edges")c.edges=n;else if(key=="source")c.source=n;else if(key=="threshold")c.threshold=n;else throw Error("unknown axis: "+key);
}
static void apply_field(Case& c,const std::string& key,json_object* value){if(key=="negative"){if(json_object_get_type(value)!=json_type_boolean)throw Error("negative must be bool");c.negative=json_object_get_boolean(value)!=0;return;}assign(c,key,value_string(value));}
static bool numeric_axis(const std::string& s){return s!="distribution"&&s!="weight_class";}
static Json config_json(const Config& c){
 Json j;j.put("schema_version",static_cast<std::int64_t>(1));j.put("experiment_id",c.experiment_id);j.put("problem_family",c.family);j.put("algorithm_a",c.a);j.put("algorithm_b",c.b);j.put("search_method",c.method);j.put("primary_axis",c.axis);j.put("seed",static_cast<std::int64_t>(c.seed));j.put("max_cases",static_cast<std::int64_t>(c.max_cases));j.put("max_case_bytes",static_cast<std::int64_t>(c.max_bytes));j.put("discovery_blocks",static_cast<std::int64_t>(c.blocks));j.put("confirm_blocks",static_cast<std::int64_t>(c.confirm_blocks));j.put("warmup",static_cast<std::int64_t>(c.warmup));j.put("sample_ms",static_cast<std::int64_t>(c.sample_ms));j.put("rss_mb",static_cast<std::int64_t>(c.rss_mb));j.put("total_wall_seconds",static_cast<std::int64_t>(c.total_wall_s));j.put("min_practical_ratio",c.min_ratio);j.put("base_params",case_params(c.base));
 Json axes;for(const auto& [name,values]:c.axes){auto* arr=json_object_new_array();for(const auto& val:values)json_object_array_add(arr,json_object_new_string(val.c_str()));json_object_object_add(axes.p,name.c_str(),arr);}j.put("axes",std::move(axes));return j;
}
static Config config_from_json(const Json& j){
 if(j.i("schema_version",1)!=1)throw Error("only config schema version 1 supported");
 Config c;c.experiment_id=j.s("experiment_id","experiment");c.family=j.s("problem_family",j.s("family","calibration"));
 c.a=j.s("algorithm_a","");c.b=j.s("algorithm_b","");
 if(auto* a=j.get("algorithms")){if(json_object_get_type(a)!=json_type_array||json_object_array_length(a)<1||json_object_array_length(a)>2)throw Error("algorithms must contain one or two elements");c.a=json_object_get_string(json_object_array_get_idx(a,0));if(json_object_array_length(a)>1)c.b=json_object_get_string(json_object_array_get_idx(a,1));}
 if(c.a.empty())c.a=c.family=="calibration"?"calibration":c.family=="integer_array"?"std_sort":c.family=="topk"?"topk_heap":"dijkstra_heap";
 c.method=j.s("search_method",j.s("method","grid"));if(auto* search=j.get("search")){Json k(json_object_get(search));c.method=k.s("method",c.method);c.max_cases=static_cast<std::uint64_t>(k.i("max_cases",static_cast<std::int64_t>(c.max_cases)));}
 c.axis=j.s("primary_axis",c.family=="calibration"?"x":c.family=="graph"?"edges":"n");
 c.seed=static_cast<std::uint64_t>(j.i("seed",137));c.max_cases=static_cast<std::uint64_t>(j.i("max_cases",static_cast<std::int64_t>(c.max_cases)));c.max_bytes=static_cast<std::uint64_t>(j.i("max_case_bytes",64*1024*1024));c.blocks=static_cast<int>(j.i("discovery_blocks",9));c.confirm_blocks=static_cast<int>(j.i("confirm_blocks",20));c.warmup=static_cast<int>(j.i("warmup",2));c.sample_ms=static_cast<int>(j.i("sample_ms",3000));c.rss_mb=static_cast<int>(j.i("rss_mb",1024));c.total_wall_s=static_cast<int>(j.i("total_wall_seconds",600));c.min_ratio=j.d("min_practical_ratio",1.2);
 if(auto* m=j.get("measurement")){Json x(json_object_get(m));c.blocks=static_cast<int>(x.i("paired_discovery_blocks",c.blocks));c.confirm_blocks=static_cast<int>(x.i("paired_confirm_blocks",c.confirm_blocks));c.warmup=static_cast<int>(x.i("warmup_iterations",c.warmup));}
 if(auto* d=j.get("detection")){Json x(json_object_get(d));c.min_ratio=x.d("min_practical_ratio",c.min_ratio);}
 if(auto* l=j.get("limits")){Json x(json_object_get(l));c.total_wall_s=static_cast<int>(x.i("total_wall_seconds",c.total_wall_s));c.max_cases=static_cast<std::uint64_t>(x.i("max_cases",static_cast<std::int64_t>(c.max_cases)));c.max_bytes=static_cast<std::uint64_t>(x.i("max_case_bytes",static_cast<std::int64_t>(c.max_bytes)));c.rss_mb=static_cast<int>(x.i("max_worker_rss_mb",c.rss_mb));c.sample_ms=static_cast<int>(x.i("max_sample_wall_ms",c.sample_ms));}
 c.base.family=c.family;c.base.seed=c.seed;
 if(auto* base=j.get("base_params")){Json p=json_object_get_type(base)==json_type_string?Json::parse(json_object_get_string(base)):Json(json_object_get(base));
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
 if(c.max_cases==0||c.max_cases>20000||c.max_bytes<64||c.max_bytes>64ULL*1024*1024||c.blocks<1||c.blocks>100||c.confirm_blocks<1||c.confirm_blocks>100||c.warmup<0||c.warmup>100||c.sample_ms<100||c.sample_ms>60000||c.total_wall_s<1||c.total_wall_s>86400||c.rss_mb<64||c.rss_mb>65536||!std::isfinite(c.min_ratio)||c.min_ratio<1.01||c.min_ratio>100000)throw Error("configuration resource/measurement bounds invalid");
 if(!c.axes.count(c.axis))c.axis=c.axes.begin()->first;
 auto valid=algorithms_for(c.family);if(std::find(valid.begin(),valid.end(),c.a)==valid.end()&&c.a.rfind("plugin:",0)!=0)throw Error("unknown primary algorithm for family");
 if(!c.b.empty()&&std::find(valid.begin(),valid.end(),c.b)==valid.end()&&c.b.rfind("plugin:",0)!=0)throw Error("unknown secondary algorithm for family");
 for(const auto& [key,values]:c.axes){for(const auto& v:values){Case sample=c.base;assign(sample,key,v); /* validate full cartesian separately below */}}
 return c;
}
Config parse_config(const std::string& path){return config_from_json(Json::file(path));}
static std::vector<Case> grid_cases(const Config& c){
 std::vector<Case> out;std::vector<std::pair<std::string,std::vector<std::string>>> axes(c.axes.begin(),c.axes.end());
 std::function<void(std::size_t,Case)> recurse=[&](std::size_t i,Case base){
  if(out.size()>=c.max_cases)return;
  if(i==axes.size()){
   base.seed=c.seed+static_cast<std::uint64_t>(out.size());
   out.push_back(make_case(base,c.max_bytes));return;
  }
  for(const auto& value:axes[i].second){auto candidate=base;assign(candidate,axes[i].first,value);recurse(i+1,candidate);if(out.size()>=c.max_cases)return;}
 };recurse(0,c.base);return out;
}
static std::vector<Case> scheduled(const Config& c){
 auto cases=grid_cases(c);
 if(c.method=="random"||c.method=="stratified"){
  std::uint64_t seed=c.seed^0x925c42;
  for(std::size_t i=cases.size();i>1;i--){auto j=randbelow(seed,i);std::swap(cases[i-1],cases[static_cast<std::size_t>(j)]);}
 }else if(c.method=="adaptive_partition"&&numeric_axis(c.axis)){
  /* Space-filling coarse-first, then binary subdivision, with fixed 20% exploration. */
  std::sort(cases.begin(),cases.end(),[&](const Case& a,const Case& b){auto get=[&](const Case& x){if(c.axis=="x")return x.x;if(c.axis=="n")return x.n;if(c.axis=="k")return x.k;if(c.axis=="edges")return x.edges;return x.vertices;};return get(a)<get(b);});
  std::vector<Case> ordered;std::vector<bool> used(cases.size(),false);
  std::function<void(std::size_t,std::size_t)> split=[&](std::size_t l,std::size_t r){if(l>=r)return;auto m=l+(r-l)/2;if(!used[m]){ordered.push_back(cases[m]);used[m]=true;}split(l,m);split(m+1,r);};
  if(!cases.empty()){ordered.push_back(cases.front());used.front()=true;if(cases.size()>1){ordered.push_back(cases.back());used.back()=true;}split(1,cases.size()>1?cases.size()-1:1);}
  cases=std::move(ordered);
 }
 return cases;
}
static double median(std::vector<double> v){if(v.empty())throw Error("median of empty sample");std::sort(v.begin(),v.end());const auto n=v.size();return n%2?v[n/2]:0.5*(v[n/2-1]+v[n/2]);}
static std::int64_t axis_value(const Case& c,const std::string& axis){if(axis=="x")return c.x;if(axis=="n")return c.n;if(axis=="k")return c.k;if(axis=="edges")return c.edges;if(axis=="vertices")return c.vertices;return 0;}
static std::optional<double> metric(Store& s,const Case& c,const Config& cfg,bool holdout){
 auto obs=s.observations(c.id,holdout);std::vector<double> a,b;
 for(const auto& o:obs){if(o.status!="ok"||o.batch<1)continue;double v;
  if(cfg.family=="calibration")v=static_cast<double>(o.work);else {if(o.ns<=0)continue;v=static_cast<double>(o.ns)/static_cast<double>(o.batch);}
  if(v<=0)continue;
  if(o.algorithm==cfg.a)a.push_back(v);
  if(o.algorithm==cfg.b)b.push_back(v);
 }
 if(a.empty()||(!cfg.b.empty()&&b.empty()))return {};
 if(cfg.b.empty())return std::log(median(a));
 return std::log(median(a)/median(b));
}
static std::optional<std::tuple<std::int64_t,std::int64_t,double>> detect(Store& s,const Config& cfg){
 if(!numeric_axis(cfg.axis))return {};
 for(const auto& [key,values]:cfg.axes){if(key!=cfg.axis&&values.size()>1)return {};}
 std::map<std::int64_t,std::vector<double>> grouped;
 for(const auto& c:s.cases(false)){if(auto m=metric(s,c,cfg,false))grouped[axis_value(c,cfg.axis)].push_back(*m);}
 if(grouped.size()<4)return {};
 std::vector<std::int64_t> x;std::vector<double> y;for(const auto& [n,v]:grouped){x.push_back(n);y.push_back(median(v));}
 const double mean=std::accumulate(y.begin(),y.end(),0.0)/static_cast<double>(y.size());
 double null_sse=0;for(auto v:y)null_sse+=(v-mean)*(v-mean);
 double best_sse=std::numeric_limits<double>::infinity();std::size_t cut=0;
 for(std::size_t j=1;j<y.size();j++){
  auto ml=std::accumulate(y.begin(),y.begin()+static_cast<std::ptrdiff_t>(j),0.0)/static_cast<double>(j);
  auto mr=std::accumulate(y.begin()+static_cast<std::ptrdiff_t>(j),y.end(),0.0)/static_cast<double>(y.size()-j);
  double sse=0;for(std::size_t k=0;k<j;k++)sse+=(y[k]-ml)*(y[k]-ml);for(std::size_t k=j;k<y.size();k++)sse+=(y[k]-mr)*(y[k]-mr);
  if(sse<best_sse){best_sse=sse;cut=j;}
 }
 const auto effect=std::abs(y[cut]-y[cut-1]);
 const auto n=static_cast<double>(y.size());const double bic0=n*std::log((null_sse+1e-12)/n)+std::log(n);const double bic1=n*std::log((best_sse+1e-12)/n)+3.0*std::log(n);
 if(bic0-bic1<6||effect<std::log(cfg.min_ratio))return {};
 /* Single largest local contrast must dominate smooth gradients. */
 double other=0;for(std::size_t i=1;i<y.size();i++)if(i!=cut)other=std::max(other,std::abs(y[i]-y[i-1]));
 if(effect<2.0*other)return {};
 return std::tuple{x[cut-1],x[cut],effect};
}
void explore(const Config& cfg,const std::string& out){
 if(fs::exists(out+"/run.sqlite"))throw Error("output run already exists (will not overwrite evidence)");
 fs::create_directories(out);Store s(out+"/run.sqlite");s.experiment(config_json(cfg).str(),environment());
 auto choices=scheduled(cfg);std::size_t completed=0;
 const auto start=std::chrono::steady_clock::now();
 for(const auto& c:choices){
  auto elapsed=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-start).count();
  if(elapsed>=cfg.total_wall_s)break;
  const std::vector<std::string> names=cfg.b.empty()?std::vector<std::string>{cfg.a}:std::vector<std::string>{cfg.a,cfg.b};
  auto obs=measure(c,names,cfg.blocks,cfg.warmup,cfg.sample_ms,cfg.rss_mb);
  for(const auto& o:obs)s.add_observation(c,o,false);
  completed++;
 }
 if(auto d=detect(s,cfg)){
  auto [l,r,e]=*d;s.hypothesis("H1","HYPOTHESIZED",cfg.axis,l,r,e,"Exploratory segmented fit vs no-break null (BIC>6); holdout required");
 }else s.hypothesis("H1","NO_CONFIRMED_BOUNDARY",cfg.axis,0,0,0,"No convincing isolated structural discontinuity under declared thresholds");
 std::cout<<"Explored "<<completed<<" cases; "<<s.summary().str()<<"\n";
 report(out);
}
static std::tuple<std::string,std::string,std::int64_t,std::int64_t> read_hypothesis(Store& s,const std::string& id){
 sqlite3_stmt* st=nullptr;if(sqlite3_prepare_v2(s.raw(),"SELECT status,axis,left_value,right_value FROM hypotheses WHERE id=?",-1,&st,nullptr)!=SQLITE_OK)throw Error("hypothesis query failed");
 sqlite3_bind_text(st,1,id.c_str(),-1,SQLITE_TRANSIENT);if(sqlite3_step(st)!=SQLITE_ROW){sqlite3_finalize(st);throw Error("unknown hypothesis "+id);}
 std::string status=reinterpret_cast<const char*>(sqlite3_column_text(st,0)),axis=reinterpret_cast<const char*>(sqlite3_column_text(st,1));auto l=sqlite3_column_int64(st,2),r=sqlite3_column_int64(st,3);sqlite3_finalize(st);return {status,axis,l,r};
}
void confirm(const std::string& run,const std::string& id){
 Store s(run+"/run.sqlite");auto cfg=config_from_json(Json::parse(s.config()));auto [status,axis,left,right]=read_hypothesis(s,id);
 if(status=="NO_CONFIRMED_BOUNDARY") {std::cout<<"No candidate to confirm\n";return;}
 if(status!="HYPOTHESIZED")throw Error("confirmation already attempted or candidate not eligible: "+status);
 std::vector<std::pair<std::int64_t,double>> sides;
 for(auto x:{left,right}){
  std::vector<double> estimates;
  for(int i=0;i<cfg.confirm_blocks;i++){
   Case c=cfg.base;assign(c,axis,std::to_string(x));c.seed=cfg.seed+1000000+static_cast<std::uint64_t>(i)*31+static_cast<std::uint64_t>(x%31);
   c=make_case(c,cfg.max_bytes);
   const std::vector<std::string> names=cfg.b.empty()?std::vector<std::string>{cfg.a}:std::vector<std::string>{cfg.a,cfg.b};
   const auto obs=measure(c,names,1,cfg.warmup,cfg.sample_ms,cfg.rss_mb);
   for(const auto& o:obs)s.add_observation(c,o,true);
   if(auto m=metric(s,c,cfg,true))estimates.push_back(*m);
  }
  if(estimates.size()<static_cast<std::size_t>(cfg.confirm_blocks)){s.hypothesis(id,"NOT_CONFIRMED",axis,left,right,0,"Holdout correctness/measurement failures or insufficient samples");report(run);return;}
  sides.emplace_back(x,median(estimates));
 }
 const auto difference=std::abs(sides[1].second-sides[0].second);
 /* Conservative deterministic split: non-overlapping empirical range of case-level estimates.
    This is NOT a general 99% CI; any noncalibration result remains provisional. */
 if(cfg.family!="calibration"){
  s.hypothesis(id,"NOT_CONFIRMED",axis,left,right,difference,"Real timing cannot be marked confirmed without calibrated paired-case 99% CI and multiplicity control");
 }else if(difference>=std::log(cfg.min_ratio)){
  s.hypothesis(id,"CONFIRMED",axis,left,right,difference,"Independent seed-separated work-count holdout, exactly deterministic; synthetic calibration only");
 }else s.hypothesis(id,"NOT_CONFIRMED",axis,left,right,difference,"Holdout effect below threshold");
 report(run);std::cout<<s.summary().str()<<"\n";
}
void minimize(const std::string& run,const std::string& id){
 Store s(run+"/run.sqlite");auto cfg=config_from_json(Json::parse(s.config()));auto [status,axis,left,right]=read_hypothesis(s,id);
 if(status!="CONFIRMED")throw Error("only confirmed hypotheses can be minimized");
 if(cfg.family!="calibration"||axis!="x"){
  s.hypothesis(id,"MINIMIZATION_FAILED",axis,left,right,0,"No validated family-aware minimization predicate available");return;
 }
 Case c=cfg.base;c.x=right;c.seed=cfg.seed+2000000;c=make_case(c,cfg.max_bytes);
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
 Store s(run+"/run.sqlite");auto summary=s.summary();auto cfg=config_from_json(Json::parse(s.config()));
 Json report;report.put("schema_version",static_cast<std::int64_t>(1));report.put("experiment_id",cfg.experiment_id);report.put("family",cfg.family);report.put("algorithm_a",cfg.a);report.put("algorithm_b",cfg.b);report.put("environment",Json::parse(s.environment()));report.put("summary",std::move(summary));
 auto* raw=json_object_new_array();std::ostringstream csv;csv<<"case_id,family,axis_value,holdout,algorithm,status,batch_ns,batch_iterations,work_units,block,output_digest,detail\n";
 for(const auto& c:s.cases())for(int holdout=0;holdout<2;holdout++)for(const auto& o:s.observations(c.id,holdout!=0)){
  Json row;row.put("case_id",c.id);row.put("family",c.family);row.put("axis_value",axis_value(c,cfg.axis));row.put("holdout",holdout!=0);row.put("algorithm",o.algorithm);row.put("status",o.status);row.put("batch_ns",o.ns);row.put("batch_iterations",o.batch);row.put("work_units",o.work);row.put("block",o.block);row.put("output_digest",o.digest);row.put("detail",o.detail);json_object_array_add(raw,row.p);row.p=nullptr;
  csv<<csv_escape(c.id)<<','<<csv_escape(c.family)<<','<<axis_value(c,cfg.axis)<<','<<holdout<<','<<csv_escape(o.algorithm)<<','<<csv_escape(o.status)<<','<<o.ns<<','<<o.batch<<','<<o.work<<','<<o.block<<','<<csv_escape(o.digest)<<','<<csv_escape(o.detail)<<'\n';
 }
 json_object_object_add(report.p,"raw_samples",raw);write_file(run+"/report.json",report.str());write_file(run+"/samples.csv",csv.str());
 auto* summary_obj=report.get("summary");Json summ(json_object_get(summary_obj));
 std::ostringstream md;md<<"# PHASESHIFT experimental report\n\n- Experiment: "<<cfg.experiment_id<<"\n- Family: "<<cfg.family<<"\n- Comparison: "<<cfg.a<<(cfg.b.empty()?"":" vs "+cfg.b)<<"\n- Discovered cases and holdouts: "<<summ.i("cases")<<" total\n- Raw observations: "<<summ.i("samples")<<"\n- Incorrect, timeout, crash or invalid observations: "<<summ.i("invalid_samples")<<"\n- Primary axis: "<<cfg.axis<<"\n- Full raw evidence: `samples.csv`, `report.json`, `run.sqlite`\n\n## Boundary candidates and verdicts\n\n";
 auto* hypotheses=summ.get("hypotheses");for(std::size_t i=0;i<json_object_array_length(hypotheses);i++){
  Json x(json_object_get(json_object_array_get_idx(hypotheses,i)));md<<"- "<<x.s("id")<<": **"<<x.s("status")<<"**; between "<<x.i("left")<<" and "<<x.i("right")<<" (effect="<<x.d("effect")<<"). "<<x.s("detail")<<"\n";
 }
 md<<"\n## Scientific limitations\n\n- Synthetic work units are not time; this fixture is an engineered calibration, not a natural discovery.\n- CPU timings vary by host, cache, thermal conditions, and compiler settings.\n- Real performance candidates are **not confirmed** without calibrated paired-case intervals and multiplicity controls.\n- Observational morphology alone does not establish causality.\n- Reproduce with `phaseshift replay --run "<<run<<" --case <case-id>` and inspect raw rows.\n";
 write_file(run+"/report.md",md.str());write_file(run+"/report.html","<!doctype html><meta charset=\"utf-8\"><title>PHASESHIFT report</title><style>body{font:16px system-ui;max-width:900px;margin:3em auto;white-space:pre-wrap}</style><main>"+html_escape(md.str())+"</main>");
}
void export_run(const std::string& run,const std::string& out){
 if(fs::exists(out))throw Error("bundle output exists");
 fs::create_directories(out);report(run);
 for(const auto& file:{"run.sqlite","report.json","report.md","report.html","samples.csv"})fs::copy_file(run+"/"+file,out+"/"+file);
 if(fs::exists(run+"/witness.cpp"))fs::copy_file(run+"/witness.cpp",out+"/witness.cpp");
 if(fs::exists(run+"/witness.hex"))fs::copy_file(run+"/witness.hex",out+"/witness.hex");
 Json manifest;for(const auto& file:fs::directory_iterator(out))if(file.is_regular_file()){auto data=read_file(file.path().string());manifest.put(file.path().filename().string().c_str(),sha256(data));}
 write_file(out+"/SHA256.json",manifest.str());write_file(out+"/README.md","# PHASESHIFT portable evidence bundle\nVerify SHA256.json digests, then use the documented replay command with run.sqlite. SQLite WAL checkpointing must be complete before copying while closed.\n");
 std::cout<<"Exported "<<out<<"\n";
}
void compare(const std::string& a,const std::string& b){Store left(a+"/run.sqlite"),right(b+"/run.sqlite");auto x=config_from_json(Json::parse(left.config())),y=config_from_json(Json::parse(right.config()));
 if(x.family!=y.family||x.a!=y.a||x.b!=y.b)throw Error("incompatible algorithm output contracts");
 std::cout<<"A: "<<left.summary().str()<<"\nB: "<<right.summary().str()<<"\n";
 std::cout<<"WARNING: hardware equivalence has not been established; do not compare absolute times as speedup.\n";
}
}
