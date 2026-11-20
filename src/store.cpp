#include "phaseshift/store.hpp"
#include "phaseshift/morphology.hpp"
#include <filesystem>
#include <cstring>
#include <sstream>
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
 void done(){check(sqlite3_step(p),db,"SQL step");}
};
Store::Store(const std::string& path,bool readonly){
 if(!readonly && !std::filesystem::path(path).parent_path().empty())std::filesystem::create_directories(std::filesystem::path(path).parent_path());
 std::string uri;
 if(readonly){
  static constexpr char hex[]="0123456789ABCDEF";
  uri="file:";
  for(unsigned char ch:path){
   if((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='/'||ch=='.'||ch=='_'||ch=='-')uri+=static_cast<char>(ch);
   else{uri+='%';uri+=hex[ch>>4];uri+=hex[ch&15];}
  }
  uri+="?immutable=1";
 }
 check(sqlite3_open_v2(readonly?uri.c_str():path.c_str(),&db_,readonly?SQLITE_OPEN_READONLY|SQLITE_OPEN_URI:SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,nullptr),db_,"SQL open");
 if(!readonly)exec("PRAGMA foreign_keys=ON;PRAGMA journal_mode=WAL;PRAGMA busy_timeout=5000;");
 auto old=sqlite3_get_autocommit(db_);(void)old;Statement st(db_,"PRAGMA user_version");auto rc=sqlite3_step(st.p);check(rc,db_,"schema version");auto version=sqlite3_column_int(st.p,0);if(version>4|| (readonly&&version!=4))throw Error("unsupported database schema version");
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
}
Store::~Store(){if(db_)sqlite3_close(db_);}
void Store::exec(const std::string& sql){char* err=nullptr;int rc=sqlite3_exec(db_,sql.c_str(),nullptr,nullptr,&err);if(rc!=SQLITE_OK){std::string msg=err?err:"unknown sqlite error";sqlite3_free(err);throw Error("SQL: "+msg);}}
void Store::experiment(const std::string& config,const std::string& env){
 Statement s(db_,"INSERT INTO experiments(id,config,config_hash,environment,created_at) VALUES (1,?,?,?,?)");s.text(1,config);s.text(2,sha256(config));s.text(3,env);s.text(4,now_utc());s.done();
}
void Store::add_case(const Case& c){
 Statement s(db_,"INSERT OR IGNORE INTO cases(case_id,family,params,seed,input,input_sha256,features) VALUES(?,?,?,?,?,?,?)");s.text(1,c.id);s.text(2,c.family);s.text(3,case_params(c));s.integer(4,static_cast<std::int64_t>(c.seed));s.blob(5,c.bytes);s.text(6,sha256(c.bytes));s.text(7,morphology_features(c).str());s.done();
}
void Store::add_observation(const Case& c,const Observation& v,bool holdout){
 add_case(c);Statement s(db_,"INSERT INTO samples(case_id,algorithm,status,ns,batch,work,block,holdout,output_digest,detail) VALUES(?,?,?,?,?,?,?,?,?,?)");
 s.text(1,c.id);s.text(2,v.algorithm);s.text(3,v.status);s.integer(4,v.ns);s.integer(5,v.batch);s.integer(6,v.work);s.integer(7,v.block);s.integer(8,holdout?1:0);s.text(9,v.digest);s.text(10,v.detail);s.done();
}
void Store::hypothesis(const std::string& id,const std::string& status,const std::string& axis,std::int64_t left,std::int64_t right,double effect,const std::string& detail,const std::string& kind){
 Statement s(db_,"INSERT INTO hypotheses(id,status,axis,left_value,right_value,effect,detail,kind) VALUES(?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET status=excluded.status,detail=excluded.detail");
 s.text(1,id);s.text(2,status);s.text(3,axis);s.integer(4,left);s.integer(5,right);s.real(6,effect);s.text(7,detail);s.text(8,kind);s.done();
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
 Statement s(db_,"SELECT algorithm,status,ns,batch,work,block,output_digest,detail FROM samples WHERE case_id=? AND holdout=? ORDER BY id");s.text(1,case_id);s.integer(2,holdout?1:0);std::vector<Observation> v;
 while(sqlite3_step(s.p)==SQLITE_ROW){Observation o;auto str=[&](int i){auto* t=sqlite3_column_text(s.p,i);return std::string(t?reinterpret_cast<const char*>(t):"");};o.algorithm=str(0);o.status=str(1);o.ns=sqlite3_column_int64(s.p,2);o.batch=sqlite3_column_int64(s.p,3);o.work=sqlite3_column_int64(s.p,4);o.block=sqlite3_column_int64(s.p,5);o.digest=str(6);o.detail=str(7);v.push_back(o);}return v;
}
std::string Store::config(){Statement s(db_,"SELECT config FROM experiments WHERE id=1");if(sqlite3_step(s.p)!=SQLITE_ROW)throw Error("experiment manifest missing");return reinterpret_cast<const char*>(sqlite3_column_text(s.p,0));}
std::string Store::environment(){Statement s(db_,"SELECT environment FROM experiments WHERE id=1");if(sqlite3_step(s.p)!=SQLITE_ROW)throw Error("stored environment missing");return reinterpret_cast<const char*>(sqlite3_column_text(s.p,0));}
Json Store::summary(){
 Json report;auto* rows=json_object_new_array();Statement st(db_,"SELECT id,status,axis,left_value,right_value,effect,detail,kind FROM hypotheses ORDER BY id");
 while(sqlite3_step(st.p)==SQLITE_ROW){auto str=[&](int i){auto* t=sqlite3_column_text(st.p,i);return std::string(t?reinterpret_cast<const char*>(t):"");};Json x;x.put("id",str(0));x.put("status",str(1));x.put("axis",str(2));x.put("left",static_cast<std::int64_t>(sqlite3_column_int64(st.p,3)));x.put("right",static_cast<std::int64_t>(sqlite3_column_int64(st.p,4)));x.put("effect",sqlite3_column_double(st.p,5));x.put("detail",str(6));x.put("kind",str(7));json_object_array_add(rows,x.p);x.p=nullptr;}
 json_object_object_add(report.p,"hypotheses",rows);
 auto count=[&](const char* q){Statement s(db_,q);if(sqlite3_step(s.p)!=SQLITE_ROW)throw Error("summary query error");return static_cast<std::int64_t>(sqlite3_column_int64(s.p,0));};
 report.put("cases",count("SELECT COUNT(*) FROM cases"));report.put("samples",count("SELECT COUNT(*) FROM samples"));report.put("invalid_samples",count("SELECT COUNT(*) FROM samples WHERE status != 'ok'"));
 return report;
}
}
