// Experimental opt-in v3 seed-matched discovery runner.
// Separate CLI, v10 run.sqlite schema, and worker-backed observations.
// No v1/v2 configuration, generator, CLI or Store migrations are modified.
#include "phaseshift/experiment.hpp"
#include "phaseshift/v3/replicate_planner.hpp"
#include "phaseshift/v3/adaptive_pair.hpp"
#include "phaseshift/v3/conditional_pair.hpp"
#include <sqlite3.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace {
using namespace ps;
using namespace ps::v3;
namespace fs=std::filesystem;
void require(bool ok,const std::string& message){if(!ok)throw Error("v3 pairs: "+message);}
std::int64_t integer(const std::string& text){
 std::size_t end=0;
 std::int64_t value=0;
 try{value=std::stoll(text,&end);}catch(const std::exception&){throw Error("v3 noninteger numeric axis value");}
 require(end==text.size(),"invalid trailing numeric axis value");
 return value;
}
void apply_axis(Case& c,const std::string& key,const std::string& value){
 if(key=="x")c.x=integer(value);
 else if(key=="n")c.n=integer(value);
 else if(key=="k")c.k=integer(value);
 else if(key=="vertices")c.vertices=integer(value);
 else if(key=="edges")c.edges=integer(value);
 else if(key=="threshold")c.threshold=integer(value);
 else if(key=="value_range")c.value_range=integer(value);
 else if(key=="unique_count")c.unique_count=integer(value);
 else if(key=="distribution")c.distribution=value;
 else if(key=="weight_class")c.weight_class=value;
 else if(key=="negative"){require(value=="true"||value=="false","invalid negative");c.negative=value=="true";}
 else if(key=="directed"){require(value=="true"||value=="false","invalid directed");c.directed=value=="true";}
 else throw Error("v3 unsupported structural axis in pilot: "+key);
}
struct Sqlite {
 sqlite3* db=nullptr;
 explicit Sqlite(const fs::path& path){
  const auto result=sqlite3_open_v2(path.string().c_str(),&db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,nullptr);
  if(result!=SQLITE_OK){const auto error=db?sqlite3_errmsg(db):"open failed";if(db)sqlite3_close(db);db=nullptr;throw Error("v3 SQLite: "+std::string(error));}
  exec("PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA busy_timeout=5000;");
 }
 ~Sqlite(){if(db)sqlite3_close(db);}
 Sqlite(const Sqlite&)=delete;Sqlite& operator=(const Sqlite&)=delete;
 void exec(const char* sql){
  char* error=nullptr;
  const auto code=sqlite3_exec(db,sql,nullptr,nullptr,&error);
  if(code!=SQLITE_OK){const std::string message=error?error:"unknown";sqlite3_free(error);throw Error("v3 SQLite: "+message);}
 }
};
struct Statement {
 sqlite3* db;sqlite3_stmt* stmt=nullptr;
 Statement(Sqlite& owner,const char* sql):db(owner.db){
  if(sqlite3_prepare_v2(db,sql,-1,&stmt,nullptr)!=SQLITE_OK)
   throw Error("v3 SQL prepare: "+std::string(sqlite3_errmsg(db)));
 }
 ~Statement(){sqlite3_finalize(stmt);}
 Statement(const Statement&)=delete;Statement& operator=(const Statement&)=delete;
 void bind(int pos,const std::string& value){
  if(sqlite3_bind_text(stmt,pos,value.c_str(),static_cast<int>(value.size()),SQLITE_TRANSIENT)!=SQLITE_OK)
   throw Error("v3 SQL bind text");
 }
 void bind(int pos,std::int64_t value){
  if(sqlite3_bind_int64(stmt,pos,value)!=SQLITE_OK)throw Error("v3 SQL bind integer");
 }
 void bind_blob(int pos,const Bytes& data){
  require(data.size()<static_cast<std::size_t>(std::numeric_limits<int>::max()),"oversized raw input");
  if(sqlite3_bind_blob(stmt,pos,data.data(),static_cast<int>(data.size()),SQLITE_TRANSIENT)!=SQLITE_OK)
   throw Error("v3 SQL bind blob");
 }
 void bind_null(int pos){if(sqlite3_bind_null(stmt,pos)!=SQLITE_OK)throw Error("v3 SQL bind null");}
 bool row(){const auto rc=sqlite3_step(stmt);if(rc==SQLITE_ROW)return true;if(rc==SQLITE_DONE)return false;throw Error("v3 SQL read: "+std::string(sqlite3_errmsg(db)));}
 void done(){require(!row(),"unexpected extra v3 result");}
 std::int64_t number(int col)const{return sqlite3_column_int64(stmt,col);}
 std::string str(int col)const{
  const auto* p=sqlite3_column_text(stmt,col);
  require(p!=nullptr,"unexpected v3 NULL text");
  return reinterpret_cast<const char*>(p);
 }
};
struct Plan {
 std::string source,axis,left,right;
 Config config;
 int pairs=0;
 bool adaptive=false;
 bool conditional=false;
 bool implication=false; // opt-in v13 frozen implication semantics
};
std::vector<LazySearchSpace::Axis> ordered_axes(const Config& cfg){
 return {cfg.axes.begin(),cfg.axes.end()};
}
void validate_plan(const Plan& plan){
 require(plan.pairs>0&&plan.pairs<=100,"discovery pair bound must be 1..100");
 require(plan.config.policy_version=="v2","pilot requires a frozen validated v2 config as input, not a changed v1/v2 default");
 require(plan.conditional ? plan.config.axes.size()>=2&&plan.config.axes.size()<=32 : plan.config.axes.size()==1,
  "v12 conditional expects 2..32 axes; v10/v11 expect one");
 if(plan.conditional && !plan.implication)
  require(plan.config.conditional_rules.empty(),
   "v12 disjoint rectangles only; implication rules require v13");
 if(plan.implication)require(plan.conditional&&!plan.config.conditional_rules.empty(),
   "v13 requires frozen nonempty conditional implication rules");
 require(plan.config.base.generator_version==1||plan.config.base.generator_version==2,"unsupported generator");
 require(plan.config.max_cases/2>=static_cast<std::uint64_t>(plan.pairs),"pair count exceeds config case cap");
 require(plan.config.seed<static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())-500000100ULL,"holdout seed exclusion window overflow");
 const auto& labels=plan.config.axes.at(plan.axis);
 if(plan.adaptive||plan.conditional){
  require(labels.size()>=(plan.conditional?2U:3U)&&labels.size()<=4096,
   "adaptive policy requires 3..4096 ordered structural values");
  static const std::set<std::string> numeric={
    "x","n","k","vertices","edges","threshold","value_range","unique_count"};
  require(numeric.count(plan.axis)!=0,"adaptive policy requires a genuinely ordered numeric axis");
  for(std::size_t i=1;i<labels.size();++i)
   require(integer(labels[i])>integer(labels[i-1]),
    "adaptive labels must be strictly increasing numeric coordinates");
 }
 require(plan.left!=plan.right,"pair endpoints must differ");
 require(std::find(labels.begin(),labels.end(),plan.left)!=labels.end()&&
  std::find(labels.begin(),labels.end(),plan.right)!=labels.end(),"pair coordinate not in frozen legal space");
 if(!plan.conditional){
  Case trial=plan.config.base;
  apply_axis(trial,plan.axis,plan.left);validate_case_schema(trial,plan.config.max_bytes);
  trial=plan.config.base;
  apply_axis(trial,plan.axis,plan.right);validate_case_schema(trial,plan.config.max_bytes);
 }
}
Plan make_plan(const std::string& source,const std::string& left,
 const std::string& right,int count){
 Plan plan;plan.source=source;plan.config=config_from_json(Json::parse(source));
 require(plan.config.axes.size()==1,"pilot supports one structural axis");
 plan.axis=plan.config.axes.begin()->first;
 plan.left=left;plan.right=right;plan.pairs=count;
 validate_plan(plan);return plan;
}
std::vector<std::string> adaptive_legal_labels(const Plan& plan){
 const auto axes=ordered_axes(plan.config);
 const AdmissibleSearchSpace legal(axes,plan.config.admissible_subspaces);
 std::vector<std::string> result;
 for(const auto& label:plan.config.axes.at(plan.axis)){
  if(!legal.index_of({{plan.axis,label}}))continue;
  auto candidate=plan.config.base;
  apply_axis(candidate,plan.axis,label);
  try{validate_case_schema(candidate,plan.config.max_bytes);}
  catch(const Error&){continue;} // same admissible construction filter as v2
  result.push_back(label);
 }
 require(result.size()>=3&&result.size()<=4096,
  "adaptive policy requires at least three legal and valid ordered structures");
 return result;
}
Plan make_adaptive_plan(const std::string& source,int count){
 const auto cfg=config_from_json(Json::parse(source));
 require(cfg.axes.size()==1,"adaptive pilot currently supports exactly one declared structural axis");
 Plan draft;draft.source=source;draft.config=cfg;draft.axis=cfg.axes.begin()->first;
 draft.pairs=count;draft.adaptive=true;
 const auto labels=adaptive_legal_labels(draft);
 draft.left=labels[0];draft.right=labels[1];
 validate_plan(draft);
 return draft;
}
std::vector<AdmissibleSearchSpace::Subspace>
conditional_scopes(const Plan& plan){
 // config_from_json has already compiled implication rules through the
 // canonical v2 path. A second compilation would alter bounded strata
 // decomposition and needlessly duplicate the source of legal truth.
 return plan.config.admissible_subspaces;
}
Plan make_conditional_plan(const std::string& source,int count,bool implication=false){
 const auto cfg=config_from_json(Json::parse(source));
 Plan result;result.source=source;result.config=cfg;result.axis=cfg.axis;
 result.pairs=count;result.conditional=true;result.implication=implication;
 require(cfg.axes.count(result.axis)!=0,"v12 primary axis not in frozen declared space");
 const auto& labels=cfg.axes.at(result.axis);
 require(labels.size()>=2,"v12 requires at least two primary labels");
 result.left=labels[0];result.right=labels[1];
 validate_plan(result);
 const auto axes=ordered_axes(cfg);
 const auto scopes=conditional_scopes(result);
 const AdmissibleSearchSpace legal(axes,scopes);
 ConditionalPairPortfolio probe(legal,axes,result.axis,scopes,
  cfg.seed,static_cast<std::size_t>(count),cfg.min_ratio);
 require(probe.next().has_value(),"v12 no feasible conditional pair");
 return result;
}
void enable_adaptive_schema(Sqlite& db);
void enable_conditional_schema(Sqlite& db,bool implication);
void create_schema(Sqlite& db,const Plan& plan){
 db.exec(R"SQL(
 BEGIN IMMEDIATE;
 CREATE TABLE v3_plan(
  id INTEGER PRIMARY KEY CHECK(id=1), source_json TEXT NOT NULL,
  source_sha256 TEXT NOT NULL, axis TEXT NOT NULL,
  left_label TEXT NOT NULL, right_label TEXT NOT NULL,
  max_pairs INTEGER NOT NULL CHECK(max_pairs BETWEEN 1 AND 100),
  protocol TEXT NOT NULL CHECK(protocol='v3_matched_discovery_pilot_1'));
 CREATE TABLE v3_cases(
  case_id TEXT PRIMARY KEY, family TEXT NOT NULL, params TEXT NOT NULL,
  seed INTEGER NOT NULL, input BLOB NOT NULL, input_sha256 TEXT NOT NULL);
 CREATE TABLE v3_pairs(
  pair_number INTEGER PRIMARY KEY, seed INTEGER NOT NULL UNIQUE,
  left_first INTEGER NOT NULL CHECK(left_first IN (0,1)),
  left_case_id TEXT NOT NULL REFERENCES v3_cases(case_id),
  right_case_id TEXT NOT NULL REFERENCES v3_cases(case_id),
  status TEXT NOT NULL CHECK(status IN ('ok','censored')),
  log_contrast REAL, worker_timed_cpu_ns INTEGER,
  pair_wall_ns INTEGER NOT NULL CHECK(pair_wall_ns>0),
  CHECK((status='ok' AND log_contrast IS NOT NULL) OR
        (status='censored' AND log_contrast IS NULL)));
 CREATE TABLE v3_samples(
  pair_number INTEGER NOT NULL REFERENCES v3_pairs(pair_number),
  side INTEGER NOT NULL CHECK(side IN (0,1)),
  ordinal INTEGER NOT NULL, case_id TEXT NOT NULL REFERENCES v3_cases(case_id),
  algorithm TEXT NOT NULL, status TEXT NOT NULL,
  ns INTEGER, batch INTEGER, work INTEGER, cpu_ns INTEGER,
  peak_rss_bytes INTEGER, pilot_ns INTEGER, pilot_iterations INTEGER,
  protocol TEXT NOT NULL, block INTEGER NOT NULL,
  output_digest TEXT NOT NULL, detail TEXT NOT NULL,
  order_position INTEGER, order_seed TEXT NOT NULL,
  PRIMARY KEY(pair_number,side,ordinal),
  CHECK((status='ok' AND ns>0) OR (status!='ok' AND ns IS NULL)));
 PRAGMA user_version=10;
 )SQL");
 Statement st(db,"INSERT INTO v3_plan VALUES(1,?,?,?,?,?,?, 'v3_matched_discovery_pilot_1')");
 st.bind(1,plan.source);st.bind(2,sha256(plan.source));
 st.bind(3,plan.axis);st.bind(4,plan.left);st.bind(5,plan.right);
 st.bind(6,static_cast<std::int64_t>(plan.pairs));st.done();
 if(plan.adaptive)enable_adaptive_schema(db); // same atomic schema/plan commit
 if(plan.conditional)enable_conditional_schema(db,plan.implication);
 db.exec("COMMIT");
}
void enable_adaptive_schema(Sqlite& db){
 db.exec(R"SQL(
 CREATE TABLE v3_adaptive_decisions(
  pair_number INTEGER PRIMARY KEY REFERENCES v3_pairs(pair_number),
  axis TEXT NOT NULL, left_label TEXT NOT NULL, right_label TEXT NOT NULL,
  arm TEXT NOT NULL CHECK(arm IN ('explore','replicate')),
  prior_evidence_digest TEXT NOT NULL CHECK(length(prior_evidence_digest)=64),
  CHECK(left_label<>right_label));
 PRAGMA user_version=11;
 )SQL");
}
void enable_conditional_schema(Sqlite& db,bool implication){
 db.exec(R"SQL(
 CREATE TABLE v3_conditional_decisions(
  pair_number INTEGER PRIMARY KEY REFERENCES v3_pairs(pair_number),
  axis TEXT NOT NULL, left_coords TEXT NOT NULL,right_coords TEXT NOT NULL,
  arm TEXT NOT NULL CHECK(arm IN ('explore','replicate')),
  prior_evidence_digest TEXT NOT NULL CHECK(length(prior_evidence_digest)=64),
  CHECK(left_coords<>right_coords));
 )SQL");
 db.exec(implication?"PRAGMA user_version=13;":"PRAGMA user_version=12;");
}
std::string encode_coords(const std::vector<AxisValue>& coords){
 std::string out;
 for(const auto& [name,value]:coords){
  out+=std::to_string(name.size())+":"+name;
  out+=std::to_string(value.size())+":"+value;
 }
 return out;
}
Plan read_plan(Sqlite& db){
 Statement version(db,"PRAGMA user_version");
 require(version.row(),"cannot inspect v3 schema version");
 const auto schema=version.number(0);
 require(schema==10||schema==11||schema==12||schema==13,"not a supported v3 pilot SQLite database");
 Statement st(db,"SELECT source_json,source_sha256,axis,left_label,right_label,max_pairs,protocol FROM v3_plan WHERE id=1");
 require(st.row(),"missing immutable v3 plan");
 const auto source=st.str(0),digest=st.str(1),axis=st.str(2),left=st.str(3),right=st.str(4);
 const auto count=st.number(5);
 const auto protocol=st.str(6);
 require(sha256(source)==digest,"frozen plan digest mismatch");
 require(protocol=="v3_matched_discovery_pilot_1"&&count>0&&count<=100,"invalid v3 protocol/count");
 auto result=(schema==12||schema==13)?
   make_conditional_plan(source,static_cast<int>(count),schema==13):
   make_plan(source,left,right,static_cast<int>(count));
 result.adaptive=schema==11;
 if(result.adaptive){
  validate_plan(result);
  const auto labels=adaptive_legal_labels(result);
  require(left==labels.at(0)&&right==labels.at(1),
   "v11 frozen adaptive metadata anchors mismatch");
 }
 if(schema>=12)require(result.left==left&&result.right==right,
   "v12 frozen conditional metadata anchors mismatch");
 require(result.axis==axis,"stored axis changed during replay");
 return result;
}
ReplicatePlanner planner_for(const Plan& plan){
 std::set<std::uint64_t> excluded;
 for(std::uint64_t i=0;i<100;++i)excluded.insert(plan.config.seed+500000000ULL+i);
 return ReplicatePlanner(plan.config.seed^0x9b629d5aULL,
  static_cast<std::uint32_t>(plan.pairs),std::move(excluded));
}
MatchedDiscoveryPair proposed(ReplicatePlanner& planner,const Plan& plan){
 const auto axes=ordered_axes(plan.config);
 AdmissibleSearchSpace legal(axes,plan.config.admissible_subspaces);
 const auto left=legal.index_of({{plan.axis,plan.left}});
 const auto right=legal.index_of({{plan.axis,plan.right}});
 require(left.has_value()&&right.has_value(),"frozen endpoints not feasible");
 return planner.propose(legal,{{{plan.axis,plan.left}},static_cast<std::uint32_t>(plan.config.base.generator_version)},
  {{{plan.axis,plan.right}},static_cast<std::uint32_t>(plan.config.base.generator_version)});
}
struct ScheduledV3 {
 MatchedDiscoveryPair matched;
 std::string left,right;
 std::optional<AdaptivePairChoice> adaptive_choice;
 std::optional<ConditionalPairChoice> conditional_choice;
};
ScheduledV3 select_pair(ReplicatePlanner& planner,const Plan& plan,
                       AdaptivePairPolicy* adaptive,
                       ConditionalPairPortfolio* conditional){
 if(conditional){
  const auto choice=conditional->next();
  require(choice.has_value(),"v12 conditional frontier exhausted under bounded budget");
  const AdmissibleSearchSpace legal(ordered_axes(plan.config),
     conditional_scopes(plan));
  const auto version=static_cast<std::uint32_t>(plan.config.base.generator_version);
  const auto paired=planner.propose(legal,{choice->left,version},
                                    {choice->right,version});
  return {paired,"","",std::nullopt,*choice};
 }
 if(!adaptive)return {proposed(planner,plan),plan.left,plan.right,std::nullopt};
 const auto choice=adaptive->next();
 require(choice.has_value(),"adaptive exploration/replication frontier exhausted");
 const AdmissibleSearchSpace legal(ordered_axes(plan.config),
   plan.config.admissible_subspaces);
 const auto version=static_cast<std::uint32_t>(plan.config.base.generator_version);
 auto paired=planner.propose(legal,{{{plan.axis,choice->left_label}},version},
                                  {{{plan.axis,choice->right_label}},version});
 return {std::move(paired),choice->left_label,choice->right_label,choice};
}
Case materialize(const Plan& plan,const std::string& value,std::uint64_t seed){
 Case c=plan.config.base;
 c.seed=seed;
 apply_axis(c,plan.axis,value);
 return make_case(c,plan.config.max_bytes);
}
Case materialize_selected(const Plan& plan,const ScheduledV3& selected,
                          int side,std::uint64_t seed){
 if(!selected.conditional_choice)
  return materialize(plan,side==0?selected.left:selected.right,seed);
 Case c=plan.config.base;c.seed=seed;
 const auto& coords=side==0?selected.conditional_choice->left:
                             selected.conditional_choice->right;
 for(const auto& [axis,value]:coords)apply_axis(c,axis,value);
 return make_case(c,plan.config.max_bytes);
}
std::optional<double> per_side_metric(const std::vector<Observation>& rows,
 const std::string& main,const std::string& competitor);
std::size_t replay(Sqlite& db,const Plan& plan,ReplicatePlanner& planner,
                   AdaptivePairPolicy* adaptive,ConditionalPairPortfolio* conditional){
 Statement rows(db,"SELECT pair_number,seed,left_first,left_case_id,right_case_id,status,log_contrast,pair_wall_ns FROM v3_pairs ORDER BY pair_number");
 std::size_t completed=0;
 while(rows.row()){
  require(completed<static_cast<std::size_t>(plan.pairs),"persisted pairs exceed frozen cap");
  const auto selected=select_pair(planner,plan,adaptive,conditional);
  const auto& proposal=selected.matched;
  const auto seed=proposal.first.key.replicate.seed;
  if(adaptive){
   const auto& choice=*selected.adaptive_choice;
   Statement decision(db,"SELECT axis,left_label,right_label,arm,prior_evidence_digest FROM v3_adaptive_decisions WHERE pair_number=?");
   decision.bind(1,static_cast<std::int64_t>(completed));
   require(decision.row()&&decision.str(0)==plan.axis&&
     decision.str(1)==choice.left_label&&decision.str(2)==choice.right_label&&
     decision.str(3)==choice.arm&&decision.str(4)==choice.prior_evidence_digest,
     "v11 adaptive transcript tamper or committed-only policy replay mismatch");
   require(!decision.row(),"duplicate v11 adaptive design");
  }
  if(conditional){
   const auto& choice=*selected.conditional_choice;
   Statement decision(db,"SELECT axis,left_coords,right_coords,arm,prior_evidence_digest FROM v3_conditional_decisions WHERE pair_number=?");
   decision.bind(1,static_cast<std::int64_t>(completed));
   require(decision.row()&&decision.str(0)==plan.axis&&
      decision.str(1)==encode_coords(choice.left)&&
      decision.str(2)==encode_coords(choice.right)&&
      decision.str(3)==choice.arm&&
      decision.str(4)==choice.prior_evidence_digest,
      "v12 committed conditional policy schedule/evidence mismatch");
   require(!decision.row(),"duplicate v12 conditional decision");
  }
  require(rows.number(0)==static_cast<std::int64_t>(completed)&&
   rows.number(1)==static_cast<std::int64_t>(seed)&&
   rows.number(2)==static_cast<std::int64_t>(proposal.first_runs_first?1:0),
   "deterministic v3 pair transcript mismatch");
  const auto a=materialize_selected(plan,selected,0,seed),
             b=materialize_selected(plan,selected,1,seed);
  require(rows.str(3)==a.id&&rows.str(4)==b.id&&rows.number(7)>0,
   "v3 committed case identity or cost mismatch");
  const auto status=rows.str(5);
  require(status=="ok"||status=="censored","invalid stored v3 status");
  require((status=="ok")== (sqlite3_column_type(rows.stmt,6)!=SQLITE_NULL),
    "censored pair cannot carry numeric contrast");
  for(const auto& c:{a,b}){
   Statement source(db,"SELECT family,params,seed,input_sha256,input FROM v3_cases WHERE case_id=?");
   source.bind(1,c.id);
   require(source.row()&&source.str(0)==c.family&&source.str(1)==case_params(c)&&
    source.number(2)==static_cast<std::int64_t>(seed)&&source.str(3)==sha256(c.bytes)&&
    sqlite3_column_bytes(source.stmt,4)==static_cast<int>(c.bytes.size())&&
    (c.bytes.empty()||std::memcmp(sqlite3_column_blob(source.stmt,4),c.bytes.data(),c.bytes.size())==0),
    "case bytes/provenance replay mismatch");
  }
  std::vector<std::vector<Observation>> persisted(2);
  for(int side=0;side<2;++side){
   Statement sample(db,"SELECT status,ns,case_id,batch,algorithm,ordinal FROM v3_samples WHERE pair_number=? AND side=? ORDER BY ordinal");
   sample.bind(1,static_cast<std::int64_t>(completed));
   sample.bind(2,static_cast<std::int64_t>(side));
   const auto expected=static_cast<std::size_t>(plan.config.blocks)*(plan.config.b.empty()?1U:4U);
   std::size_t seen=0;
   while(sample.row()){
    require(sample.str(2)==(side==0?a.id:b.id),"sample linked to wrong seed/structure");
    require(sample.number(5)==static_cast<std::int64_t>(seen),"v3 raw observation ordinal gap");
    const auto valid=sample.str(0)=="ok";
    require(valid==(sqlite3_column_type(sample.stmt,1)!=SQLITE_NULL),
      "failed sample promoted to a time metric");
    require(valid==(sqlite3_column_type(sample.stmt,3)!=SQLITE_NULL),
      "timing and batch validity mismatch");
    Observation raw;
    raw.status=sample.str(0);raw.algorithm=sample.str(4);
    raw.ns=valid?sample.number(1):0;raw.batch=valid?sample.number(3):0;
    persisted[static_cast<std::size_t>(side)].push_back(std::move(raw));
    ++seen;
   }
   require(seen==expected,"incomplete persisted real-worker observation batch");
  }
  const auto left_metric=per_side_metric(persisted[0],plan.config.a,plan.config.b);
  const auto right_metric=per_side_metric(persisted[1],plan.config.a,plan.config.b);
  const auto recalculated=left_metric&&right_metric?
   std::optional<double>{*right_metric-*left_metric}:std::nullopt;
  require((status=="ok")==recalculated.has_value(),
   "pair status contradicts raw worker evidence");
  if(recalculated){
   const double stored=sqlite3_column_double(rows.stmt,6);
   require(std::isfinite(stored)&&
    std::abs(stored-*recalculated)<=1e-12*(1.+std::abs(*recalculated)),
    "stored pair contrast differs from independent raw sample reconstruction");
  }
  if(adaptive)adaptive->on_committed(*selected.adaptive_choice,
                                  Cohort::Discovery,recalculated);
  if(conditional)conditional->on_committed(*selected.conditional_choice,
                                  Cohort::Discovery,recalculated);
  ++completed;
 }
 return completed;
}
std::optional<double> per_side_metric(const std::vector<Observation>& rows,
 const std::string& main,const std::string& competitor){
 // Complete block required. Status failures poison this pair; no substitution.
 for(const auto& r:rows)if(r.status!="ok"||r.ns<=0||r.batch<=0)
  return std::nullopt;
 auto median=[&](const std::string& algorithm)->std::optional<double>{
  std::vector<double> values;
  for(const auto& r:rows)if(r.algorithm==algorithm)
   values.push_back(static_cast<double>(r.ns)/static_cast<double>(r.batch));
  if(values.empty())return std::nullopt;
  std::sort(values.begin(),values.end());
  const auto mid=values.size()/2;
  return values.size()%2?values[mid]:(values[mid-1]+values[mid])/2.;
 };
 const auto a=median(main);
 if(!a||*a<=0)return std::nullopt;
 if(competitor.empty())return std::log(*a);
 const auto b=median(competitor);
 if(!b||*b<=0)return std::nullopt;
 return std::log(*a)-std::log(*b);
}
void insert_case(Sqlite& db,const Case& c){
 Statement st(db,"INSERT OR IGNORE INTO v3_cases(case_id,family,params,seed,input,input_sha256) VALUES(?,?,?,?,?,?)");
 st.bind(1,c.id);st.bind(2,c.family);st.bind(3,case_params(c));
 st.bind(4,static_cast<std::int64_t>(c.seed));st.bind_blob(5,c.bytes);
 st.bind(6,sha256(c.bytes));st.done();
}
void insert_samples(Sqlite& db,std::size_t index,int side,const Case& c,
 const std::vector<Observation>& rows){
 for(std::size_t ordinal=0;ordinal<rows.size();++ordinal){
  const auto& r=rows[ordinal];
  Statement st(db,"INSERT INTO v3_samples(pair_number,side,ordinal,case_id,algorithm,status,ns,batch,work,cpu_ns,peak_rss_bytes,pilot_ns,pilot_iterations,protocol,block,output_digest,detail,order_position,order_seed) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
  st.bind(1,static_cast<std::int64_t>(index));st.bind(2,static_cast<std::int64_t>(side));
  st.bind(3,static_cast<std::int64_t>(ordinal));st.bind(4,c.id);st.bind(5,r.algorithm);
  st.bind(6,r.status);
  if(r.status=="ok"&&r.ns>0&&r.batch>0){st.bind(7,r.ns);st.bind(8,r.batch);st.bind(9,r.work);}
  else{st.bind_null(7);st.bind_null(8);st.bind_null(9);}
  if(r.cpu_ns>=0)st.bind(10,r.cpu_ns);else st.bind_null(10);
  if(r.peak_rss_bytes>=0)st.bind(11,r.peak_rss_bytes);else st.bind_null(11);
  if(r.pilot_ns>=0)st.bind(12,r.pilot_ns);else st.bind_null(12);
  if(r.pilot_iterations>=0)st.bind(13,r.pilot_iterations);else st.bind_null(13);
  st.bind(14,r.measurement_protocol_version);st.bind(15,r.block);
  st.bind(16,r.digest);st.bind(17,r.detail);
  if(r.order_position>=0)st.bind(18,r.order_position);else st.bind_null(18);
  st.bind(19,r.order_seed);st.done();
 }
}
void execute_pairs(Sqlite& db,const Plan& plan,ReplicatePlanner& planner,
 AdaptivePairPolicy* adaptive,ConditionalPairPortfolio* conditional,
 std::size_t completed,std::size_t pause){
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(plan.config.total_wall_s);
 const std::vector<std::string> algorithms=plan.config.b.empty()?
  std::vector<std::string>{plan.config.a}:
  std::vector<std::string>{plan.config.a,plan.config.b};
 std::size_t this_call=0;
 while(completed<static_cast<std::size_t>(plan.pairs)&&(pause==0||this_call<pause)){
  const auto selected=select_pair(planner,plan,adaptive,conditional);
  const auto& proposal=selected.matched;
  const auto seed=proposal.first.key.replicate.seed;
  const auto a=materialize_selected(plan,selected,0,seed),
             b=materialize_selected(plan,selected,1,seed);
  const auto started=std::chrono::steady_clock::now();
  auto measure_one=[&](const Case& c){
   return measure(c,algorithms,plan.config.blocks,plan.config.warmup,
    plan.config.sample_ms,plan.config.rss_mb,plan.config.minimum_batch_ms,
    plan.config.isolation_mode=="per_sample",deadline,plan.config.measurement_protocol_version);
  };
  std::vector<Observation> left,right;
  if(proposal.first_runs_first){left=measure_one(a);right=measure_one(b);}
  else{right=measure_one(b);left=measure_one(a);}
  if(const auto* fault=std::getenv("PHASESHIFT_V3_FAILPOINT");fault&&std::string(fault)=="before_pair_commit")
   std::_Exit(88);
  const auto wall=std::chrono::duration_cast<std::chrono::nanoseconds>(
   std::chrono::steady_clock::now()-started).count();
  require(wall>0,"zero real-worker block wall");
  const auto l=per_side_metric(left,plan.config.a,plan.config.b);
  const auto r=per_side_metric(right,plan.config.a,plan.config.b);
  const auto contrast=l&&r?std::optional<double>{*r-*l}:std::nullopt;
  require(!contrast||std::isfinite(*contrast),"nonfinite paired log contrast");
  std::int64_t cpu=0;
  bool cpu_known=true;
  for(const auto& rows:{left,right})for(const auto& row:rows){
   if(row.cpu_ns<0||row.cpu_ns>std::numeric_limits<std::int64_t>::max()-cpu)
    cpu_known=false;
   else if(cpu_known)cpu+=row.cpu_ns;
  }
  db.exec("BEGIN IMMEDIATE");
  try{
   insert_case(db,a);insert_case(db,b);
   Statement st(db,"INSERT INTO v3_pairs(pair_number,seed,left_first,left_case_id,right_case_id,status,log_contrast,worker_timed_cpu_ns,pair_wall_ns) VALUES(?,?,?,?,?,?,?,?,?)");
   st.bind(1,static_cast<std::int64_t>(completed));
   st.bind(2,static_cast<std::int64_t>(seed));
   st.bind(3,static_cast<std::int64_t>(proposal.first_runs_first?1:0));
   st.bind(4,a.id);st.bind(5,b.id);
   st.bind(6,contrast?std::string("ok"):std::string("censored"));
   if(contrast){
    // SQLite only accepts genuinely finite measurements; no failed status promoted.
    if(sqlite3_bind_double(st.stmt,7,*contrast)!=SQLITE_OK)
     throw Error("v3 finite contrast SQL binding");
   }else st.bind_null(7);
   if(cpu_known)st.bind(8,cpu);else st.bind_null(8);
   st.bind(9,wall);st.done();
   if(adaptive){
    const auto& chosen=*selected.adaptive_choice;
    Statement choice(db,"INSERT INTO v3_adaptive_decisions(pair_number,axis,left_label,right_label,arm,prior_evidence_digest) VALUES(?,?,?,?,?,?)");
    choice.bind(1,static_cast<std::int64_t>(completed));choice.bind(2,plan.axis);
    choice.bind(3,chosen.left_label);choice.bind(4,chosen.right_label);
    choice.bind(5,chosen.arm);choice.bind(6,chosen.prior_evidence_digest);
    choice.done();
   }
   if(conditional){
    const auto& chosen=*selected.conditional_choice;
    Statement st(db,"INSERT INTO v3_conditional_decisions(pair_number,axis,left_coords,right_coords,arm,prior_evidence_digest) VALUES(?,?,?,?,?,?)");
    st.bind(1,static_cast<std::int64_t>(completed));st.bind(2,plan.axis);
    st.bind(3,encode_coords(chosen.left));st.bind(4,encode_coords(chosen.right));
    st.bind(5,chosen.arm);st.bind(6,chosen.prior_evidence_digest);st.done();
   }
   insert_samples(db,completed,0,a,left);insert_samples(db,completed,1,b,right);
   db.exec("COMMIT");
  }catch(...){db.exec("ROLLBACK");throw;}
  if(const auto* fault=std::getenv("PHASESHIFT_V3_FAILPOINT");fault&&std::string(fault)=="after_pair_commit")
   std::_Exit(89);
  if(adaptive)adaptive->on_committed(*selected.adaptive_choice,
                                  Cohort::Discovery,contrast);
  if(conditional)conditional->on_committed(*selected.conditional_choice,
                                  Cohort::Discovery,contrast);
  ++completed;++this_call;
 }
 std::cout<<"v3 matched discovery committed "<<completed<<"/"<<plan.pairs
          <<" pairs; cohort=discovery; significance=not_established\n";
}
std::string option(int argc,char** argv,const std::string& name){
 for(int i=2;i+1<argc;i+=2)if(argv[i]==name)return argv[i+1];
 return {};
}
int numeric_option(int argc,char** argv,const std::string& key,int fallback){
 const auto raw=option(argc,argv,key);
 if(raw.empty())return fallback;
 const auto parsed=integer(raw);
 require(parsed>=0&&parsed<=100,"bounded "+key+" expected 0..100");
 return static_cast<int>(parsed);
}
void syntax(int argc,char** argv,const std::string& command){
 const std::set<std::string> accepted=command=="run"?
  std::set<std::string>{"--config","--out","--left","--right","--pairs","--pause-after"}:
  command=="adaptive"||command=="conditional"||command=="conditional-rules"?
  std::set<std::string>{"--config","--out","--pairs","--pause-after"}:
  command=="verify"?std::set<std::string>{"--run"}:
  std::set<std::string>{"--run","--pause-after"};
 require((argc-2)%2==0,"options require key/value pairs");
 std::set<std::string> used;
 for(int i=2;i<argc;i+=2){
  require(accepted.count(argv[i])!=0,"unknown CLI option");
  require(used.insert(argv[i]).second,"duplicate CLI option");
 }
}
} // namespace

int main(int argc,char** argv){
 try{
  require(argc>=2,"expected run, resume or verify");
  const std::string command=argv[1];
  require(command=="run"||command=="adaptive"||command=="conditional"||command=="conditional-rules"||command=="resume"||command=="verify","unknown pilot command");
  syntax(argc,argv,command);
  const auto pause=numeric_option(argc,argv,"--pause-after",0);
  if(command=="run"||command=="adaptive"||command=="conditional"||command=="conditional-rules"){
   const auto config=option(argc,argv,"--config"),out=option(argc,argv,"--out");
   require(!config.empty()&&!out.empty(),"run needs config and output path");
   Plan plan;
   if(command=="adaptive")
    plan=make_adaptive_plan(read_file(config),numeric_option(argc,argv,"--pairs",3));
   else if(command=="conditional"||command=="conditional-rules")
    plan=make_conditional_plan(read_file(config),numeric_option(argc,argv,"--pairs",3),
             command=="conditional-rules");
   else{
    const auto left=option(argc,argv,"--left"),right=option(argc,argv,"--right");
    require(!left.empty()&&!right.empty(),"run needs two frozen endpoint labels");
    plan=make_plan(read_file(config),left,right,numeric_option(argc,argv,"--pairs",3));
   }
   require(!fs::exists(out),"run directory exists; cannot overwrite immutable evidence");
   fs::create_directories(out);
   Sqlite db(fs::path(out)/"run.sqlite");
   create_schema(db,plan);
   auto planner=planner_for(plan);
   std::optional<AdaptivePairPolicy> adaptive;
   if(plan.adaptive)adaptive.emplace(adaptive_legal_labels(plan),
                  plan.config.seed,static_cast<std::size_t>(plan.pairs),plan.config.min_ratio);
   std::optional<AdmissibleSearchSpace> legal;
   std::optional<ConditionalPairPortfolio> conditional;
   if(plan.conditional){
    const auto axes=ordered_axes(plan.config);
    const auto scopes=conditional_scopes(plan);
    legal.emplace(axes,scopes);
    conditional.emplace(*legal,axes,plan.axis,scopes,
      plan.config.seed,static_cast<std::size_t>(plan.pairs),plan.config.min_ratio);
   }
   execute_pairs(db,plan,planner,adaptive?&*adaptive:nullptr,
                 conditional?&*conditional:nullptr,0,static_cast<std::size_t>(pause));
   return 0;
  }
  const auto run=option(argc,argv,"--run");
  require(!run.empty(),"resume/verify need --run");
  require(fs::is_regular_file(fs::path(run)/"run.sqlite"),"missing authoritative run.sqlite");
  Sqlite db(fs::path(run)/"run.sqlite");
  const auto plan=read_plan(db);
  auto planner=planner_for(plan);
  std::optional<AdaptivePairPolicy> adaptive;
  if(plan.adaptive)adaptive.emplace(adaptive_legal_labels(plan),
                   plan.config.seed,static_cast<std::size_t>(plan.pairs),plan.config.min_ratio);
  std::optional<AdmissibleSearchSpace> legal;
  std::optional<ConditionalPairPortfolio> conditional;
  if(plan.conditional){
   const auto axes=ordered_axes(plan.config);
   const auto scopes=conditional_scopes(plan);
   legal.emplace(axes,scopes);
   conditional.emplace(*legal,axes,plan.axis,scopes,
     plan.config.seed,static_cast<std::size_t>(plan.pairs),plan.config.min_ratio);
  }
  const auto completed=replay(db,plan,planner,adaptive?&*adaptive:nullptr,
                              conditional?&*conditional:nullptr);
  if(command=="verify"){
   std::cout<<"v3 replay verified "<<completed<<" committed matched pairs\n";return 0;
  }
  execute_pairs(db,plan,planner,adaptive?&*adaptive:nullptr,
                conditional?&*conditional:nullptr,completed,static_cast<std::size_t>(pause));
  return 0;
 }catch(const std::exception& e){std::cerr<<"PHASESHIFT v3 error: "<<e.what()<<"\n";return 2;}
}
