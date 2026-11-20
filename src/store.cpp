#include "phaseshift/store.hpp"
#include "phaseshift/morphology.hpp"
#include <filesystem>
#include <algorithm>
#include <map>
#include <set>
#include <cmath>
#include <tuple>
#include <vector>
#include <cstring>
#include <sstream>
#include <array>
#include <cstdint>
namespace ps {
static void check(int rc,sqlite3* db,const std::string& what){if(rc!=SQLITE_OK&&rc!=SQLITE_DONE&&rc!=SQLITE_ROW)throw Error(what+": "+sqlite3_errmsg(db));}
struct Statement {
 sqlite3* db;sqlite3_stmt* p=nullptr;
 Statement(sqlite3* d,const char* sql):db(d){check(sqlite3_prepare_v2(db,sql,-1,&p,nullptr),db,"SQL prepare");}
 ~Statement(){sqlite3_finalize(p);}
 void text(int col,const std::string& s){check(sqlite3_bind_text(p,col,s.c_str(),static_cast<int>(s.size()),SQLITE_TRANSIENT),db,"bind text");}
 void integer(int col,std::int64_t x){check(sqlite3_bind_int64(p,col,x),db,"bind integer");}
 void real(int col,double x){check(sqlite3_bind_double(p,col,x),db,"bind real");}
 void blob(int col,const Bytes& b){check(sqlite3_bind_blob(p,col,b.data(),static_cast<int>(b.size()),SQLITE_TRANSIENT),db,"bind blob");}
 void null(int col){check(sqlite3_bind_null(p,col),db,"bind null");}
 void done(){check(sqlite3_step(p),db,"SQL step");}
};
Store::Store(const std::string& path,bool readonly){
 if(!readonly && !std::filesystem::path(path).parent_path().empty())std::filesystem::create_directories(std::filesystem::path(path).parent_path());
 std::string uri;
 if(readonly){
  static constexpr char hex[]="0123456789ABCDEF";
  uri="file:";
  for(char c:path){
   const auto ch=static_cast<unsigned char>(c);
   if((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='/'||ch=='.'||ch=='_'||ch=='-')uri+=static_cast<char>(ch);
   else{uri+='%';uri+=hex[ch>>4];uri+=hex[ch&15];}
  }
  uri+="?immutable=1";
 }
 check(sqlite3_open_v2(readonly?uri.c_str():path.c_str(),&db_,readonly?SQLITE_OPEN_READONLY|SQLITE_OPEN_URI:SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,nullptr),db_,"SQL open");
 if(!readonly)exec("PRAGMA foreign_keys=ON;PRAGMA journal_mode=WAL;PRAGMA busy_timeout=5000;");
 auto old=sqlite3_get_autocommit(db_);(void)old;Statement st(db_,"PRAGMA user_version");auto rc=sqlite3_step(st.p);check(rc,db_,"schema version");auto version=sqlite3_column_int(st.p,0);if(version>7|| (readonly&&version!=6&&version!=7))throw Error("unsupported database schema version");
 if(version==0&&!readonly){exec(R"SQL(
 BEGIN;
 CREATE TABLE IF NOT EXISTS experiments(id INTEGER PRIMARY KEY CHECK(id=1),config TEXT NOT NULL,config_hash TEXT NOT NULL,environment TEXT NOT NULL,created_at TEXT NOT NULL);
 CREATE TABLE IF NOT EXISTS cases(case_id TEXT PRIMARY KEY,family TEXT NOT NULL,params TEXT NOT NULL,seed INTEGER NOT NULL,input BLOB NOT NULL,input_sha256 TEXT NOT NULL);
 CREATE TABLE IF NOT EXISTS samples(id INTEGER PRIMARY KEY AUTOINCREMENT,case_id TEXT NOT NULL REFERENCES cases(case_id),algorithm TEXT NOT NULL,status TEXT NOT NULL,ns INTEGER,batch INTEGER,work INTEGER,block INTEGER,holdout INTEGER NOT NULL,output_digest TEXT,detail TEXT);
 CREATE TABLE IF NOT EXISTS hypotheses(id TEXT PRIMARY KEY,status TEXT NOT NULL,axis TEXT NOT NULL,left_value INTEGER NOT NULL,right_value INTEGER NOT NULL,effect REAL NOT NULL,detail TEXT NOT NULL);
 CREATE TABLE IF NOT EXISTS witnesses(id INTEGER PRIMARY KEY AUTOINCREMENT,hypothesis_id TEXT REFERENCES hypotheses(id),case_id TEXT NOT NULL REFERENCES cases(case_id),status TEXT NOT NULL,trace TEXT NOT NULL);
 CREATE TABLE IF NOT EXISTS events(id INTEGER PRIMARY KEY AUTOINCREMENT,at TEXT NOT NULL,event TEXT NOT NULL,detail TEXT NOT NULL);
 CREATE INDEX IF NOT EXISTS idx_samples_case ON samples(case_id,holdout);
 PRAGMA user_version=1;
 COMMIT;
 )SQL");}
 if(version<2&&!readonly){exec(R"SQL(
 BEGIN IMMEDIATE;
 CREATE TABLE IF NOT EXISTS scheduler_decisions(
  seq INTEGER PRIMARY KEY AUTOINCREMENT, pool_index INTEGER NOT NULL UNIQUE,
  case_id TEXT NOT NULL REFERENCES cases(case_id), rng_state TEXT NOT NULL,
  created_at TEXT NOT NULL);
 PRAGMA user_version=2;
 COMMIT;
 )SQL");}
 if(version<3&&!readonly){exec(R"SQL(
 BEGIN IMMEDIATE;
 ALTER TABLE hypotheses ADD COLUMN kind TEXT NOT NULL DEFAULT 'UNSTABLE_OR_INCONCLUSIVE';
 PRAGMA user_version=3;
 COMMIT;
 )SQL");}
 if(version<4&&!readonly){exec(R"SQL(
 BEGIN IMMEDIATE;
 ALTER TABLE cases ADD COLUMN features TEXT NOT NULL DEFAULT '{}';
 PRAGMA user_version=4;
 COMMIT;
 )SQL");}
 if(version<5&&!readonly){exec(R"SQL(
 BEGIN IMMEDIATE;
 ALTER TABLE hypotheses ADD COLUMN frozen_params TEXT NOT NULL DEFAULT '';
 PRAGMA user_version=5;
 COMMIT;
 )SQL");}
 if(version<6&&!readonly){exec(R"SQL(
 BEGIN IMMEDIATE;
 ALTER TABLE samples ADD COLUMN cpu_ns INTEGER;
 ALTER TABLE samples ADD COLUMN peak_rss_bytes INTEGER;
 ALTER TABLE samples ADD COLUMN order_position INTEGER;
 ALTER TABLE samples ADD COLUMN order_seed TEXT;
 CREATE TABLE IF NOT EXISTS environments(
  fingerprint TEXT PRIMARY KEY,metadata TEXT NOT NULL,recorded_at TEXT NOT NULL);
 CREATE TABLE IF NOT EXISTS algorithms(
  algorithm_id TEXT PRIMARY KEY,version TEXT NOT NULL,adapter_abi INTEGER NOT NULL,
  output_contract TEXT NOT NULL);
 CREATE TABLE IF NOT EXISTS generators(
  family TEXT NOT NULL,generator_version INTEGER NOT NULL,schema_version INTEGER NOT NULL,
  PRIMARY KEY(family,generator_version));
 CREATE TABLE IF NOT EXISTS aggregates(
  case_id TEXT NOT NULL REFERENCES cases(case_id),algorithm TEXT NOT NULL,
  holdout INTEGER NOT NULL,metric TEXT NOT NULL,completed_samples INTEGER NOT NULL,
  median_value REAL NOT NULL,analysis_version INTEGER NOT NULL,
  PRIMARY KEY(case_id,algorithm,holdout,metric));
 CREATE TABLE IF NOT EXISTS confirmations(
  hypothesis_id TEXT PRIMARY KEY REFERENCES hypotheses(id),verdict TEXT NOT NULL,
  detail TEXT NOT NULL,confirmed_at TEXT NOT NULL);
 CREATE TABLE IF NOT EXISTS artifacts(
  path TEXT PRIMARY KEY,role TEXT NOT NULL,sha256 TEXT NOT NULL,recorded_at TEXT NOT NULL);
 CREATE INDEX IF NOT EXISTS idx_samples_algorithm_block ON samples(algorithm,block,holdout);
 PRAGMA user_version=6;
 COMMIT;
 )SQL");}
 if(version<7&&!readonly){exec(R"SQL(
 BEGIN IMMEDIATE;
 CREATE TABLE IF NOT EXISTS confirmation_evidence(
  hypothesis_id TEXT NOT NULL REFERENCES hypotheses(id),
  case_id TEXT NOT NULL REFERENCES cases(case_id),
  sample_id INTEGER NOT NULL REFERENCES samples(id),
  side INTEGER NOT NULL CHECK(side IN (0,1)),
  seed_group TEXT NOT NULL,
  PRIMARY KEY(hypothesis_id,case_id),UNIQUE(hypothesis_id,sample_id));
 CREATE TABLE IF NOT EXISTS confirmation_seals(
  hypothesis_id TEXT PRIMARY KEY REFERENCES hypotheses(id),
  primary_algorithm TEXT NOT NULL,seed_pairs INTEGER NOT NULL,
  case_count INTEGER NOT NULL,kind TEXT NOT NULL,created_at TEXT NOT NULL);
 PRAGMA user_version=7;
 COMMIT;
 )SQL");}
}
Store::~Store(){if(db_)sqlite3_close(db_);}
void Store::exec(const std::string& sql){char* err=nullptr;int rc=sqlite3_exec(db_,sql.c_str(),nullptr,nullptr,&err);if(rc!=SQLITE_OK){std::string msg=err?err:"unknown sqlite error";sqlite3_free(err);throw Error("SQL: "+msg);}}
void Store::experiment(const std::string& config,const std::string& env){
 Statement s(db_,"INSERT INTO experiments(id,config,config_hash,environment,created_at) VALUES (1,?,?,?,?)");s.text(1,config);s.text(2,sha256(config));s.text(3,env);s.text(4,now_utc());s.done();
 Statement metadata(db_,"INSERT OR IGNORE INTO environments(fingerprint,metadata,recorded_at) VALUES (?,?,?)");
 metadata.text(1,sha256(env));metadata.text(2,env);metadata.text(3,now_utc());metadata.done();
}
void Store::add_case(const Case& c){
 Statement registered(db_,"INSERT OR IGNORE INTO generators(family,generator_version,schema_version) VALUES(?,?,?)");
 registered.text(1,c.family);registered.integer(2,c.generator_version);registered.integer(3,1);registered.done();
 Statement s(db_,"INSERT OR IGNORE INTO cases(case_id,family,params,seed,input,input_sha256,features) VALUES(?,?,?,?,?,?,?)");s.text(1,c.id);s.text(2,c.family);s.text(3,case_params(c));s.integer(4,static_cast<std::int64_t>(c.seed));s.blob(5,c.bytes);s.text(6,sha256(c.bytes));s.text(7,morphology_features(c).str());s.done();
}
void Store::add_observation(const Case& c,const Observation& v,bool holdout){
 add_case(c);
 const bool external=v.algorithm.rfind("plugin:",0)==0;
 std::string version="builtin-v1";
 if(external){
  const auto path=v.algorithm.substr(7);
  // Native ABI does not expose compiler flags/build IDs; record immutable
  // binary identity when the .so is available instead of inventing metadata.
  if(std::filesystem::is_regular_file(path))version="external-abi-v1;sha256="+sha256_file(path);
  else version="external-abi-v1;sha256=UNAVAILABLE";
 }
 Statement existing(db_,"SELECT version FROM algorithms WHERE algorithm_id=?");existing.text(1,v.algorithm);
 if(sqlite3_step(existing.p)==SQLITE_ROW){
  const std::string recorded=reinterpret_cast<const char*>(sqlite3_column_text(existing.p,0));
  if(recorded!=version && recorded!="external-abi-v1")throw Error("algorithm/plugin artifact changed during experiment; refusing mixed evidence");
 }
 Statement registered(db_,"INSERT OR IGNORE INTO algorithms(algorithm_id,version,adapter_abi,output_contract) VALUES(?,?,?,?)");
 registered.text(1,v.algorithm);registered.text(2,version);
 registered.integer(3,v.algorithm.rfind("plugin:",0)==0?1:0);registered.text(4,c.family+":exact-oracle-values");registered.done();
 Statement s(db_,"INSERT INTO samples(case_id,algorithm,status,ns,batch,work,block,holdout,output_digest,detail,cpu_ns,peak_rss_bytes,order_position,order_seed) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
 s.text(1,c.id);s.text(2,v.algorithm);s.text(3,v.status);
 if(v.status=="ok")s.integer(4,v.ns);else s.null(4);
 if(v.status=="ok")s.integer(5,v.batch);else s.null(5);
 if(v.status=="ok")s.integer(6,v.work);else s.null(6);
 s.integer(7,v.block);s.integer(8,holdout?1:0);s.text(9,v.digest);s.text(10,v.detail);
 if(v.status=="ok"&&v.cpu_ns>=0)s.integer(11,v.cpu_ns);else s.null(11);
 if(v.peak_rss_bytes>=0)s.integer(12,v.peak_rss_bytes);else s.null(12);
 if(v.order_position>=0)s.integer(13,v.order_position);else s.null(13);
 if(!v.order_seed.empty())s.text(14,v.order_seed);else s.null(14);
 s.done();
}
void Store::seal_confirmation(const std::string& id,const std::string& algorithm,int required_seed_pairs,bool scaling){
 // Every positive verdict must be backed by a frozen, verifiable population of
 // new held-out case IDs. No caller-supplied counts or statistical claims are
 // trusted here. Existing legacy v6 confirmations are not auto-attested.
 if(required_seed_pairs<20||required_seed_pairs>100)throw Error("confirmation requires at least 20 independent seed groups");
 Statement hyp(db_,"SELECT status,axis,left_value,right_value,kind FROM hypotheses WHERE id=?");hyp.text(1,id);
 if(sqlite3_step(hyp.p)!=SQLITE_ROW)throw Error("unknown hypothesis to seal");
 const std::string status=reinterpret_cast<const char*>(sqlite3_column_text(hyp.p,0));
 const std::string axis=reinterpret_cast<const char*>(sqlite3_column_text(hyp.p,1));
 const auto left=sqlite3_column_int64(hyp.p,2),right=sqlite3_column_int64(hyp.p,3);
 const std::string kind=reinterpret_cast<const char*>(sqlite3_column_text(hyp.p,4));
 if(status!="TESTED")throw Error("cannot seal confirmation without TESTED state");
 if((kind=="SCALING_REGIME_CHANGE")!=scaling)throw Error("confirmation cohort type conflicts with frozen hypothesis");
 if(algorithm.empty())throw Error("empty confirmed algorithm");
 struct Row{std::string id;std::int64_t sample=0;std::int64_t size=0;int side=0;std::uint64_t group=0;};
 std::map<std::string,Row> distinct;
 Statement rows(db_,R"SQL(SELECT s.id,c.case_id,c.params,c.seed,s.status
  FROM samples s JOIN cases c ON c.case_id=s.case_id
  WHERE s.holdout=1 AND s.algorithm=? ORDER BY s.id)SQL");rows.text(1,algorithm);
 auto is_failure=kind=="CORRECTNESS_BOUNDARY"||kind=="MEMORY_OR_RESOURCE_BOUNDARY";
 while(sqlite3_step(rows.p)==SQLITE_ROW){
  const auto sample=sqlite3_column_int64(rows.p,0);
  const std::string cid=reinterpret_cast<const char*>(sqlite3_column_text(rows.p,1));
  const auto params=Json::parse(reinterpret_cast<const char*>(sqlite3_column_text(rows.p,2)));
  std::string key=axis;
  if(key=="density"||key=="degree_skew"||key=="duplicate_fraction"||key=="disorder_fraction"||key=="sign_fraction")key+="_ppm";
  auto* raw_axis=params.get(key.c_str());
  if(!raw_axis)throw Error("holdout case missing candidate axis: "+axis);
  std::int64_t value=0;
  const auto type=json_object_get_type(raw_axis);
  if(type==json_type_string||type==json_type_boolean){
   // Categorical hypothesis coordinates are stable indices into the
   // immutable serialized experiment design, never numeric magnitudes.
   const auto frozen=Json::parse(config());
   auto* axes=frozen.get("axes");json_object* labels=nullptr;
   if(!axes||!json_object_object_get_ex(axes,axis.c_str(),&labels)||
      json_object_get_type(labels)!=json_type_array)
    throw Error("categorical holdout lacks frozen category design");
   const std::string observed=type==json_type_boolean?
    (json_object_get_boolean(raw_axis)?"true":"false"):
    std::string(json_object_get_string(raw_axis));
   bool matched=false;
   for(std::size_t index=0;index<json_object_array_length(labels);index++){
    auto* candidate=json_object_array_get_idx(labels,index);
    if(candidate&&json_object_get_type(candidate)==json_type_string&&
       observed==json_object_get_string(candidate)){
     value=static_cast<std::int64_t>(index);matched=true;break;
    }
   }
   if(!matched)throw Error("holdout category is not in frozen experiment design");
  }else if(type==json_type_int)value=params.i(key.c_str());
  else throw Error("invalid holdout structural axis type");
  int side=-1;
  if(scaling){if(value<=left)side=0;else if(value>=right)side=1;}
  else{if(value==left)side=0;else if(value==right)side=1;}
  if(side<0)continue; // Prespecified remote-axis negative controls are not evidence pairs.
  const std::string observed=reinterpret_cast<const char*>(sqlite3_column_text(rows.p,4));
  if((!is_failure&&observed!="ok")||(is_failure&&side==0&&observed!="ok")||(is_failure&&side==1&&observed=="ok"))
    throw Error("confirmation includes an invalid primary-algorithm observation");
  const auto raw_seed=static_cast<std::uint64_t>(sqlite3_column_int64(rows.p,3));
  const std::uint64_t seed_group=scaling?raw_seed-static_cast<std::uint64_t>(value)*131ULL:raw_seed;
  Row v{cid,sample,value,side,seed_group};
  auto [it,inserted]=distinct.emplace(cid,v);
  if(!inserted && (it->second.side!=side||it->second.group!=seed_group))throw Error("inconsistent duplicate holdout sample");
 }
 if(distinct.empty())throw Error("confirmed hypothesis has no eligible holdout samples");
 std::map<std::uint64_t,std::array<std::map<std::int64_t,Row>,2>> cohorts;
 for(const auto& [cid,row]:distinct){
  Statement overlap(db_,"SELECT 1 FROM samples WHERE case_id=? AND holdout=0 LIMIT 1");overlap.text(1,cid);
  if(sqlite3_step(overlap.p)==SQLITE_ROW)throw Error("confirmation reuses discovery case ID");
  cohorts[row.group][static_cast<std::size_t>(row.side)][row.size]=row;
 }
 std::vector<Row> accepted;
 int groups=0;
 for(const auto& [seed,pair]:cohorts){(void)seed;
  const auto needed=scaling?2U:1U;
  if(pair[0].size()!=needed||pair[1].size()!=needed)continue;
  groups++;
  for(const auto& side:pair)for(const auto& [size,row]:side){(void)size;accepted.push_back(row);}
 }
 if(groups<required_seed_pairs)throw Error("confirmation lacks required distinct paired holdout seeds");
 if(scaling){
  std::set<std::int64_t> left_sizes,right_sizes;
  for(const auto& row:accepted)(row.side==0?left_sizes:right_sizes).insert(row.size);
  if(left_sizes.size()!=2||right_sizes.size()!=2)throw Error("scaling holdout needs two novel sizes on each side");
 }
 exec("BEGIN IMMEDIATE;");
 try{
  Statement seal(db_,"INSERT INTO confirmation_seals(hypothesis_id,primary_algorithm,seed_pairs,case_count,kind,created_at) VALUES(?,?,?,?,?,?)");
  seal.text(1,id);seal.text(2,algorithm);seal.integer(3,groups);seal.integer(4,static_cast<std::int64_t>(accepted.size()));seal.text(5,kind);seal.text(6,now_utc());seal.done();
  for(const auto& row:accepted){
   Statement link(db_,"INSERT INTO confirmation_evidence(hypothesis_id,case_id,sample_id,side,seed_group) VALUES(?,?,?,?,?)");
   link.text(1,id);link.text(2,row.id);link.integer(3,row.sample);link.integer(4,row.side);
   link.text(5,std::to_string(row.group));link.done();
  }
  exec("COMMIT;");
 }catch(...){exec("ROLLBACK;");throw;}
}
void Store::hypothesis(const std::string& id,const std::string& status,const std::string& axis,std::int64_t left,std::int64_t right,double effect,const std::string& detail,const std::string& kind,const std::string& frozen_params){
 static const std::map<std::string,std::set<std::string>> transitions={
  {"OBSERVED",{"HYPOTHESIZED","NO_CONFIRMED_BOUNDARY","INVALIDATED"}},
  {"HYPOTHESIZED",{"TESTED","NOT_CONFIRMED","INVALIDATED"}},
  {"TESTED",{"CONFIRMED","NOT_CONFIRMED","INVALIDATED"}},
  {"CONFIRMED",{"MINIMIZED_CONFIRMED","MINIMIZED_PROVISIONAL","MINIMIZATION_FAILED","INVALIDATED"}},
  {"MINIMIZED_CONFIRMED",{"INVALIDATED"}},
  {"MINIMIZED_PROVISIONAL",{"INVALIDATED"}},
  {"MINIMIZATION_FAILED",{"INVALIDATED"}},
  {"NOT_CONFIRMED",{"INVALIDATED"}},
  {"NO_CONFIRMED_BOUNDARY",{"INVALIDATED"}},
  {"INVALIDATED",{}}
 };
 if(status.empty()||!std::isfinite(effect)||!transitions.count(status))throw Error("unknown hypothesis evidence state");
 Statement prior(db_,"SELECT status,axis,left_value,right_value FROM hypotheses WHERE id=?");prior.text(1,id);
 const auto found=sqlite3_step(prior.p);
 if(found==SQLITE_ROW){
  const std::string old=reinterpret_cast<const char*>(sqlite3_column_text(prior.p,0));
  if(old!=status&&(!transitions.count(old)||!transitions.at(old).count(status)))throw Error("illegal hypothesis transition: "+old+" -> "+status);
  if(axis!=reinterpret_cast<const char*>(sqlite3_column_text(prior.p,1))||left!=sqlite3_column_int64(prior.p,2)||right!=sqlite3_column_int64(prior.p,3))throw Error("frozen hypothesis geometry cannot change");
 }else if(found==SQLITE_DONE){
  if(status!="OBSERVED"&&status!="HYPOTHESIZED"&&status!="NO_CONFIRMED_BOUNDARY")throw Error("initial evidence state must be observation, hypothesis or negative");
 }else throw Error("hypothesis state query failed");
 if(status=="CONFIRMED"){
  Statement linked(db_,R"SQL(SELECT s.seed_pairs,s.case_count,COUNT(e.case_id)
   FROM confirmation_seals s JOIN confirmation_evidence e ON e.hypothesis_id=s.hypothesis_id
   WHERE s.hypothesis_id=? GROUP BY s.hypothesis_id)SQL");linked.text(1,id);
  if(sqlite3_step(linked.p)!=SQLITE_ROW||sqlite3_column_int(linked.p,0)<20||
     sqlite3_column_int(linked.p,1)!=sqlite3_column_int(linked.p,2))
   throw Error("CONFIRMED requires sealed, linked, fresh holdout evidence");
 }
 Statement s(db_,"INSERT INTO hypotheses(id,status,axis,left_value,right_value,effect,detail,kind,frozen_params) VALUES(?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET status=excluded.status,detail=excluded.detail");
 s.text(1,id);s.text(2,status);s.text(3,axis);s.integer(4,left);s.integer(5,right);s.real(6,effect);s.text(7,detail);s.text(8,kind);s.text(9,frozen_params);s.done();
 if(status=="CONFIRMED"||status=="NOT_CONFIRMED") {
  Statement conf(db_,"INSERT OR REPLACE INTO confirmations(hypothesis_id,verdict,detail,confirmed_at) VALUES (?,?,?,?)");
  conf.text(1,id);conf.text(2,status);conf.text(3,detail);conf.text(4,now_utc());conf.done();
 }
 Statement e(db_,"INSERT INTO events(at,event,detail) VALUES(?,?,?)");e.text(1,now_utc());e.text(2,"hypothesis:"+status);e.text(3,id+" "+detail);e.done();
}
std::vector<Store::SchedulerDecision> Store::scheduler_decisions(){
 Statement stmt(db_,"SELECT pool_index,case_id,rng_state FROM scheduler_decisions ORDER BY seq");
 std::vector<SchedulerDecision> out;
 while(sqlite3_step(stmt.p)==SQLITE_ROW){
  SchedulerDecision d;d.pool_index=sqlite3_column_int64(stmt.p,0);
  d.case_id=reinterpret_cast<const char*>(sqlite3_column_text(stmt.p,1));
  const auto* v=reinterpret_cast<const char*>(sqlite3_column_text(stmt.p,2));
  std::size_t n=0;
  try{d.next_rng_state=std::stoull(v,&n,16);}catch(const std::exception&){throw Error("invalid persisted scheduler PRNG state");}
  if(n!=std::strlen(v))throw Error("invalid persisted scheduler PRNG state suffix");
  out.push_back(d);
 }
 return out;
}
void Store::record_decision(std::size_t index,const std::string& case_id,std::uint64_t rng_state){
 Statement s(db_,"INSERT INTO scheduler_decisions(pool_index,case_id,rng_state,created_at) VALUES(?,?,?,?)");
 s.integer(1,static_cast<std::int64_t>(index));s.text(2,case_id);
 std::ostringstream hex;hex<<std::hex<<rng_state;s.text(3,hex.str());s.text(4,now_utc());s.done();
}
void Store::event(const std::string& name,const std::string& detail){
 Statement s(db_,"INSERT INTO events(at,event,detail) VALUES(?,?,?)");
 s.text(1,now_utc());s.text(2,name);s.text(3,detail);s.done();
}
void Store::refresh_aggregates(){
 Statement query(db_,"SELECT case_id,algorithm,holdout,CAST(ns AS REAL)/batch FROM samples WHERE status='ok' AND ns>0 AND batch>0 ORDER BY case_id,algorithm,holdout");
 std::map<std::tuple<std::string,std::string,int>,std::vector<double>> by_group;
 while(sqlite3_step(query.p)==SQLITE_ROW){
  const auto* id=sqlite3_column_text(query.p,0),*alg=sqlite3_column_text(query.p,1);
  auto key=std::tuple{std::string(reinterpret_cast<const char*>(id)),std::string(reinterpret_cast<const char*>(alg)),sqlite3_column_int(query.p,2)};
  by_group[key].push_back(sqlite3_column_double(query.p,3));
 }
 for(auto& [key,values]:by_group){
  std::sort(values.begin(),values.end());
  const auto middle=values.size()/2;
  const auto median=values.size()%2?values[middle]:(values[middle-1]+values[middle])*0.5;
  Statement insert(db_,"INSERT OR REPLACE INTO aggregates(case_id,algorithm,holdout,metric,completed_samples,median_value,analysis_version) VALUES(?,?,?,?,?,?,?)");
  insert.text(1,std::get<0>(key));insert.text(2,std::get<1>(key));insert.integer(3,std::get<2>(key));
  insert.text(4,"wall_ns_per_operation");insert.integer(5,static_cast<std::int64_t>(values.size()));
  insert.real(6,median);insert.integer(7,1);insert.done();
 }
}
void Store::artifact(const std::string& relative_path,const std::string& digest,const std::string& kind){
 Statement s(db_,"INSERT OR REPLACE INTO artifacts(path,role,sha256,recorded_at) VALUES(?,?,?,?)");
 s.text(1,relative_path);s.text(2,kind);s.text(3,digest);s.text(4,now_utc());s.done();
}
void Store::witness(const std::string& id,const std::string& case_id,const std::string& status,const std::string& trace){
 Statement s(db_,"INSERT INTO witnesses(hypothesis_id,case_id,status,trace) VALUES(?,?,?,?)");s.text(1,id);s.text(2,case_id);s.text(3,status);s.text(4,trace);s.done();
}
std::vector<Case> Store::cases(bool include_holdout){
 const char* sql=include_holdout?"SELECT family,params,seed,input,case_id,input_sha256 FROM cases ORDER BY rowid":"SELECT family,params,seed,input,case_id,input_sha256 FROM cases WHERE case_id IN (SELECT case_id FROM samples WHERE holdout=0) ORDER BY rowid";
 Statement s(db_,sql);std::vector<Case> v;while(sqlite3_step(s.p)==SQLITE_ROW){
  auto as_string=[&](int col){auto* t=sqlite3_column_text(s.p,col);return std::string(t?reinterpret_cast<const char*>(t):"");};
  auto data=reinterpret_cast<const std::uint8_t*>(sqlite3_column_blob(s.p,3));auto n=sqlite3_column_bytes(s.p,3);Bytes b;if(n)b.assign(data,data+n);
  if(sha256(b)!=as_string(5))throw Error("stored input digest mismatch");
  auto c=decode_case(as_string(0),as_string(1),static_cast<std::uint64_t>(sqlite3_column_int64(s.p,2)),b);
  if(c.id!=as_string(4))throw Error("stored CaseId mismatch");
  v.push_back(std::move(c));
 }return v;
}
std::vector<Observation> Store::observations(const std::string& case_id,bool holdout){
 Statement s(db_,"SELECT algorithm,status,ns,batch,work,block,output_digest,detail,cpu_ns,peak_rss_bytes,order_position,order_seed FROM samples WHERE case_id=? AND holdout=? ORDER BY id");s.text(1,case_id);s.integer(2,holdout?1:0);std::vector<Observation> v;
 while(sqlite3_step(s.p)==SQLITE_ROW){Observation o;auto str=[&](int i){auto* t=sqlite3_column_text(s.p,i);return std::string(t?reinterpret_cast<const char*>(t):"");};o.algorithm=str(0);o.status=str(1);o.ns=sqlite3_column_int64(s.p,2);o.batch=sqlite3_column_int64(s.p,3);o.work=sqlite3_column_int64(s.p,4);o.block=sqlite3_column_int64(s.p,5);o.digest=str(6);o.detail=str(7);
  o.cpu_ns=sqlite3_column_type(s.p,8)==SQLITE_NULL?-1:sqlite3_column_int64(s.p,8);
  o.peak_rss_bytes=sqlite3_column_type(s.p,9)==SQLITE_NULL?-1:sqlite3_column_int64(s.p,9);
  o.order_position=sqlite3_column_type(s.p,10)==SQLITE_NULL?-1:sqlite3_column_int64(s.p,10);
  o.order_seed=str(11);v.push_back(o);}return v;
}
std::string Store::config(){Statement s(db_,"SELECT config FROM experiments WHERE id=1");if(sqlite3_step(s.p)!=SQLITE_ROW)throw Error("experiment manifest missing");return reinterpret_cast<const char*>(sqlite3_column_text(s.p,0));}
std::string Store::environment(){Statement s(db_,"SELECT environment FROM experiments WHERE id=1");if(sqlite3_step(s.p)!=SQLITE_ROW)throw Error("stored environment missing");return reinterpret_cast<const char*>(sqlite3_column_text(s.p,0));}
Json Store::summary(){
 Json report;auto* rows=json_object_new_array();Statement st(db_,"SELECT id,status,axis,left_value,right_value,effect,detail,kind,frozen_params FROM hypotheses ORDER BY id");
 Statement schema(db_,"PRAGMA user_version");if(sqlite3_step(schema.p)!=SQLITE_ROW)throw Error("cannot read evidence schema");
 const bool links_supported=sqlite3_column_int(schema.p,0)>=7;
 while(sqlite3_step(st.p)==SQLITE_ROW){auto str=[&](int i){auto* t=sqlite3_column_text(st.p,i);return std::string(t?reinterpret_cast<const char*>(t):"");};
  const auto recorded_status=str(1);
  bool attested=true;
  if(recorded_status=="CONFIRMED"||recorded_status=="MINIMIZED_CONFIRMED"){
   attested=false;
   if(links_supported){Statement links(db_,"SELECT 1 FROM confirmation_seals WHERE hypothesis_id=?");links.text(1,str(0));attested=sqlite3_step(links.p)==SQLITE_ROW;}
  }
  Json x;x.put("id",str(0));x.put("status",attested?recorded_status:"UNVERIFIED_LEGACY_CONFIRMATION");
  if(!attested)x.put("raw_status",recorded_status);
  x.put("confirmation_evidence_attested",attested);x.put("axis",str(2));x.put("left",static_cast<std::int64_t>(sqlite3_column_int64(st.p,3)));x.put("right",static_cast<std::int64_t>(sqlite3_column_int64(st.p,4)));x.put("effect",sqlite3_column_double(st.p,5));x.put("detail",str(6));x.put("kind",str(7));x.put("frozen_params",str(8));json_object_array_add(rows,x.p);x.p=nullptr;}
 json_object_object_add(report.p,"hypotheses",rows);
 auto count=[&](const char* q){Statement s(db_,q);if(sqlite3_step(s.p)!=SQLITE_ROW)throw Error("summary query error");return static_cast<std::int64_t>(sqlite3_column_int64(s.p,0));};
 report.put("cases",count("SELECT COUNT(*) FROM cases"));report.put("samples",count("SELECT COUNT(*) FROM samples"));report.put("invalid_samples",count("SELECT COUNT(*) FROM samples WHERE status != 'ok'"));
 report.put("aggregates",count("SELECT COUNT(*) FROM aggregates"));report.put("environment_records",count("SELECT COUNT(*) FROM environments"));
 report.put("algorithm_records",count("SELECT COUNT(*) FROM algorithms"));report.put("generator_records",count("SELECT COUNT(*) FROM generators"));
 return report;
}
}
