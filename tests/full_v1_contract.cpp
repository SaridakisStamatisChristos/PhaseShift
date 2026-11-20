#include "phaseshift/experiment.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <vector>
namespace fs=std::filesystem;
using namespace ps;
static void require(bool ok,const std::string& why){if(!ok)throw Error("full_v1_contract: "+why);}
static Json configuration(const std::string& family,const std::string& axes,
                          const std::string& primary,const std::string& algorithm,
                          const std::string& fixed,const int wall=45,const int count=3,
                          const std::string& method="grid",const int blocks=1){
 return Json::parse("{\"experiment_id\":\"complete-v1-acceptance\",\"problem_family\":\""+family+
 "\",\"algorithms\":[\""+algorithm+"\"],\"primary_axis\":\""+primary+
 "\",\"seed\":137,\"axes\":"+axes+",\"fixed\":"+fixed+
 ",\"search\":{\"method\":\""+method+"\",\"max_cases\":"+std::to_string(count)+
 "},\"measurement\":{\"paired_discovery_blocks\":"+std::to_string(blocks)+
 ",\"paired_confirm_blocks\":20,\"warmup_iterations\":0,\"minimum_batch_ms\":1},"
 "\"limits\":{\"total_wall_seconds\":"+std::to_string(wall)+
 ",\"max_sample_wall_ms\":2200}}");
}
static std::string state(const std::string& run){
 Store s(run+"/run.sqlite");auto j=s.summary();
 return Json(json_object_get(json_object_array_get_idx(j.get("hypotheses"),0))).s("status");
}
static void seed_and_strata(const fs::path& root){
 const auto json=configuration("integer_array",
  "{\"distribution\":[\"uniform\",\"ascending\",\"all_equal\"],\"n\":[64,128,256]}",
  "distribution","std_sort","{}",40,3,"stratified");
 auto cfg=config_from_json(json);
 for(const char* suffix:{"a","b"})explore(cfg,(root/suffix).string());
 Store a((root/"a/run.sqlite").string()),b((root/"b/run.sqlite").string());
 auto da=a.scheduler_decisions(),db=b.scheduler_decisions();
 require(da.size()==3 && da.size()==db.size(),"three stratum samples");
 for(std::size_t i=0;i<da.size();i++)require(da[i].case_id==db[i].case_id && da[i].pool_index==db[i].pool_index,"seed replay reproduces transcript");
 std::set<std::string> labels;for(const auto& c:a.cases(false))labels.insert(c.distribution);
 require(labels.size()==3,"categorical stratification covers all three categories in first three cases");
 cfg.seed=911;explore(cfg,(root/"different").string());
 Store diff((root/"different/run.sqlite").string());
 bool different=false;
 const auto first=a.cases(false),second=diff.cases(false);
 for(std::size_t i=0;i<first.size();i++)
  if(first[i].distribution!=second[i].distribution||first[i].n!=second[i].n)different=true;
 require(different,"stratification order depends on experiment seed");
 const auto numeric=configuration("calibration","{\"x\":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15]}",
   "x","calibration","{\"threshold\":8}",40,8,"stratified");
 auto cn=config_from_json(numeric);explore(cn,(root/"numeric").string());
 Store n((root/"numeric/run.sqlite").string());
 std::set<std::int64_t> bins;
 for(const auto& c:n.cases(false))bins.insert(c.x/2);
 require(bins.size()>=6,"numeric quantile strata cover broad axis before exhaustion");
 cfg.method="adaptive_partition";cfg.max_cases=9;
 const auto paused=(root/"adaptive_pause").string(),whole=(root/"adaptive_full").string();
 explore(cfg,paused,3);resume(paused);explore(cfg,whole);
 Store left(paused+"/run.sqlite"),right(whole+"/run.sqlite");
 auto ld=left.scheduler_decisions(),rd=right.scheduler_decisions();
 require(ld.size()==9&&rd.size()==9,"categorical adaptive completes bounded design");
 for(std::size_t i=0;i<ld.size();i++)
  require(ld[i].pool_index==rd[i].pool_index&&ld[i].case_id==rd[i].case_id,
   "categorical adaptive transcript must be stable across pause and resume");
 std::set<std::string> early_categories;
 const auto observed=left.cases(false);
 for(std::size_t i=0;i<3;i++)early_categories.insert(observed[i].distribution);
 require(early_categories.size()==3,"categorical adaptive must prioritize distinct categories");
}
static void categorical_confirm(const fs::path& root){
 auto conf=configuration("calibration","{\"negative\":[false,true]}","negative","calibration",
  "{\"x\":0,\"threshold\":8}",60,2,"stratified");
 auto cfg=config_from_json(conf);const auto run=(root/"categorical").string();
 explore(cfg,run);require(state(run)=="HYPOTHESIZED","categorical calibration produces frozen edge");
 confirm(run,"H1");require(state(run)=="CONFIRMED","categorical calibration seals fresh paired holdout");
 Store s(run+"/run.sqlite");const auto summary=s.summary();
 Json h(json_object_get(json_object_array_get_idx(summary.get("hypotheses"),0)));
 require(h.s("kind")=="CATEGORICAL_SYNTHETIC_CALIBRATION", "categorical subtype retained");
 require(h.s("detail").find("negative-control")!=std::string::npos,"control evaluation included in verdict");
 auto report_file=Json::file(run+"/report.json");Json rs(json_object_get(report_file.get("summary")));
 Json rh(json_object_get(json_object_array_get_idx(rs.get("hypotheses"),0)));
 require(rh.s("left_label")=="false"&&rh.s("right_label")=="true", "unordered category labels exposed in report");
}
static void categorical_failure(const fs::path& root,const std::string& plugin){
 auto conf=configuration("integer_array",
  "{\"distribution\":[\"uniform\",\"ascending\"]}",
  "distribution","plugin:"+plugin,"{\"n\":128}",60,2,"grid");
 const auto cfg=config_from_json(conf);const auto run=(root/"category_failure").string();
 explore(cfg,run);require(state(run)=="HYPOTHESIZED","categorical oracle failure should be detectable");
 const auto summary=Json::file(run+"/report.json");
 Json x(json_object_get(summary.get("summary")));
 Json h(json_object_get(json_object_array_get_idx(x.get("hypotheses"),0)));
 require(h.s("kind")=="CORRECTNESS_BOUNDARY","categorical failure retains correctness taxonomy");
 confirm(run,"H1");require(state(run)=="CONFIRMED","categorical wrong-answer boundary confirms independently");
}
static void categorical_string_seal(const fs::path& root){
 const auto config=configuration("integer_array",
  "{\"distribution\":[\"uniform\",\"ascending\"]}",
  "distribution","std_sort","{\"n\":64}",60,2,"grid");
 const auto cfg=config_from_json(config);
 const auto run=root/"string_seal";fs::create_directories(run);
 Store s((run/"run.sqlite").string());s.experiment(config.str(),environment());
 s.hypothesis("H1","HYPOTHESIZED","distribution",0,1,2.0,
  "preselected unordered categories","CATEGORICAL_PERFORMANCE_CONTRAST",case_params(cfg.base));
 s.hypothesis("H1","TESTED","distribution",0,1,0,"holdout started");
 Observation v;v.algorithm="std_sort";v.status="ok";v.ns=1000;v.batch=1;
 for(int i=0;i<20;i++)for(int side=0;side<2;side++){
  Case c=cfg.base;c.distribution=side?"ascending":"uniform";
  c.seed=static_cast<std::uint64_t>(50000+i);c=make_case(c,cfg.max_bytes);
  s.add_observation(c,v,true);
 }
 s.seal_confirmation("H1","std_sort",20);
 s.hypothesis("H1","CONFIRMED","distribution",0,1,0,"independently matched cohort");
 Json rows=s.summary();Json h(json_object_get(json_object_array_get_idx(rows.get("hypotheses"),0)));
 require(h.s("status")=="CONFIRMED","string labels seal must be auditable");
 // A category absent from the frozen experiment cannot masquerade as a
 // valid index; the evidence seal must reject the entire cohort.
 const auto invalid=root/"invalid_string_seal";fs::create_directories(invalid);
 Store bad((invalid/"run.sqlite").string());bad.experiment(config.str(),environment());
 bad.hypothesis("H1","HYPOTHESIZED","distribution",0,1,2.0,
  "preselected pair","CATEGORICAL_PERFORMANCE_CONTRAST",case_params(cfg.base));
 bad.hypothesis("H1","TESTED","distribution",0,1,0,"holdout started");
 for(int i=0;i<20;i++)for(int side=0;side<2;side++){
  Case c=cfg.base;c.distribution=side?"nearly_sorted":"uniform";
  c.seed=static_cast<std::uint64_t>(60000+i);c=make_case(c,cfg.max_bytes);
  bad.add_observation(c,v,true);
 }
 bool rejected=false;
 try{bad.seal_confirmation("H1","std_sort",20);}catch(const Error&){rejected=true;}
 require(rejected,"unlisted categorical evidence must not pass the SQLite seal");
}
static void fixture(const fs::path& run,const Json& config){
 fs::create_directories(run);
 auto cfg=config_from_json(config);
 Store s((run/"run.sqlite").string());s.experiment(config.str(),environment());
 s.hypothesis("H1","HYPOTHESIZED",cfg.axis,16,32,1.0,"predeclared acceptance fixture",
  "PERFORMANCE_CLIFF",case_params(cfg.base));
}
static void control_veto(const fs::path& root,const std::string& plugin){
 const auto config=configuration("integer_array","{\"n\":[16,32,64]}","n", "plugin:"+plugin,
  "{\"distribution\":\"uniform\"}",40,3,"grid");
 const auto run=root/"control_veto";fixture(run,config);
 confirm(run.string(),"H1");require(state(run.string())=="NOT_CONFIRMED","failed control must veto positive verdict");
 const auto s=Json::file((run/"report.json").string());Json sum(json_object_get(s.get("summary")));
 Json h(json_object_get(json_object_array_get_idx(sum.get("hypotheses"),0)));
 require(h.s("detail").find("oracle_worker_valid=false")!=std::string::npos,
   "failed control persisted in evidence explanation");
}
static void deadline_falsifiers(const fs::path& root,const std::string& slow_plugin){
 const auto config=configuration("integer_array","{\"n\":[16,32,64]}","n", "plugin:"+slow_plugin,
  "{\"distribution\":\"uniform\"}",1,3,"grid");
 const auto run=root/"deadline_confirm";fixture(run,config);
 const auto start=std::chrono::steady_clock::now();bool failed=false;
 try{confirm(run.string(),"H1");}catch(const Error& e){
  failed=std::string(e.what()).find("total_wall_seconds budget exhausted")!=std::string::npos;
 }
 const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 require(failed && elapsed<3.5,"confirmation must terminate in-flight slow worker by absolute budget");
 require(state(run.string())=="NOT_CONFIRMED","confirmation failure must be durable and fail closed");
 const auto out=root/"deadline_intervene";const auto before=std::chrono::steady_clock::now();failed=false;
 try{intervene(run.string(),out.string(),"H1",3);}catch(const Error& e){
  failed=std::string(e.what()).find("total_wall_seconds budget exhausted")!=std::string::npos;
 }
 const auto duration=std::chrono::duration<double>(std::chrono::steady_clock::now()-before).count();
 // This source is now NOT_CONFIRMED; intervention correctly rejects it before executing.
 require(!failed && duration<1.0,"rejected source cannot start intervention");
 const auto source=root/"deadline_intervention_source";fixture(source,config);
 const auto out2=root/"deadline_intervention";failed=false;
 const auto start2=std::chrono::steady_clock::now();
 try{intervene(source.string(),out2.string(),"H1",3);}catch(const Error& e){
  failed=std::string(e.what()).find("total_wall_seconds budget exhausted")!=std::string::npos;
 }
 const auto elapsed2=std::chrono::duration<double>(std::chrono::steady_clock::now()-start2).count();
 require(failed && elapsed2<3.5,"intervention must enforce in-flight worker deadline");
 require(fs::is_regular_file(out2/"interventions.json")&&fs::is_regular_file(out2/"run.sqlite"),
   "partial intervention evidence must survive deadline");
 require(Json::file((out2/"interventions.json").string()).b("wall_budget_exhausted"),
   "budget exhaustion must be machine readable");
}
int main(int argc,char** argv){
 try{
  require(argc==5,"usage <out> <control-failure-plugin> <slow-plugin> <category-wrong-answer-plugin>");
  const fs::path root=argv[1];fs::remove_all(root);fs::create_directories(root);
  seed_and_strata(root);categorical_confirm(root);categorical_string_seal(root);
  categorical_failure(root,argv[4]);control_veto(root,argv[2]);deadline_falsifiers(root,argv[3]);
  std::cout<<"PASS: categorical discovery/holdout, randomized strata, control veto, deadlines and retained evidence\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
