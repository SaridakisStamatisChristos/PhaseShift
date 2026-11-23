// Experimental opt-in v3 seed-matched discovery runner.
// Separate CLI, v10 run.sqlite schema, and worker-backed observations.
// No v1/v2 configuration, generator, CLI or Store migrations are modified.
#include "phaseshift/experiment.hpp"
#include "phaseshift/v3/replicate_planner.hpp"
#include "phaseshift/v3/adaptive_pair.hpp"
#include "phaseshift/v3/conditional_pair.hpp"
#include "phaseshift/v3/taxonomy.hpp"
#include "phaseshift/v3/fixed_family.hpp"
#include <bit>
#include <sqlite3.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <functional>
#include <memory>
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
#include <tuple>
#include <csignal>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <unistd.h>
#include <cctype>
#include <cerrno>
#include <fstream>
#include <sstream>
#include <sys/prctl.h>
#include <sys/wait.h>

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
 explicit Sqlite(const fs::path& path,bool readonly=false){
  const auto result=sqlite3_open_v2(path.string().c_str(),&db,readonly?SQLITE_OPEN_READONLY:SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,nullptr);
  if(result!=SQLITE_OK){const auto error=db?sqlite3_errmsg(db):"open failed";if(db)sqlite3_close(db);db=nullptr;throw Error("v3 SQLite: "+std::string(error));}
  if(readonly)exec("PRAGMA query_only=ON; PRAGMA busy_timeout=5000;");
  else exec("PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA busy_timeout=5000;");
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
// Linux accounting for opt-in discovery schema 17. Counters cover the
// proposal-to-precommit window, not the entire process tree or invocation.
struct AccountedRunLock {
 int fd=-1;
 explicit AccountedRunLock(const fs::path& path,const char* busy="accounted run already has a writer"){
  fd=::open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
  require(fd>=0,"accounted run lock open failed");
  if(::flock(fd,LOCK_EX|LOCK_NB)!=0){
   ::close(fd);fd=-1;throw Error(busy);
  }
 }
 ~AccountedRunLock(){if(fd>=0)::close(fd);}
 AccountedRunLock(const AccountedRunLock&)=delete;
 AccountedRunLock& operator=(const AccountedRunLock&)=delete;
};
// V3-09 descendant containment. main() makes this process a child subreaper,
// so a descendant that escaped a worker's process group (setsid + double
// fork) is reparented here once its parent exits. After every measurement,
// kill and reap every such stray so it cannot keep running unaccounted; its
// CPU then lands in waited-child rusage, never in any timed sample. Escapes
// while this process is itself killed, or via other namespaces, are not
// contained (no cgroups); accounting stays ACCOUNTING_INCOMPLETE.
std::size_t contain_descendants_v3(){
 std::size_t contained=0;
 const auto self=static_cast<long>(::getpid());
 for(int round=0;round<64;++round){
  std::vector<pid_t> strays;
  std::error_code ec;
  for(fs::directory_iterator it("/proc",ec),end;!ec&&it!=end;it.increment(ec)){
   const auto name=it->path().filename().string();
   if(name.empty()||!std::all_of(name.begin(),name.end(),[](unsigned char ch){return std::isdigit(ch)!=0;}))continue;
   std::ifstream stat(it->path()/"stat");std::string text;std::getline(stat,text);
   const auto close=text.rfind(')');
   if(close==std::string::npos)continue;
   std::istringstream fields(text.substr(close+1));char state=0;long ppid=0;
   if(fields>>state>>ppid&&ppid==self)strays.push_back(static_cast<pid_t>(std::stol(name)));
  }
  if(strays.empty())break;
  for(const auto p:strays)::kill(p,SIGKILL);
  for(const auto p:strays){int st=0;while(::waitpid(p,&st,0)<0&&errno==EINTR){}++contained;}
 }
 if(contained)std::cerr<<"PHASESHIFT v3 warning: contained "<<contained
  <<" reparented worker descendant(s); their CPU is in waited-child rusage, not in any sample\n";
 return contained;
}
std::string executable_digest_v3(bool worker){
 const auto executable=fs::read_symlink("/proc/self/exe");
 return sha256_file((worker?executable.parent_path()/"phaseshift_worker":executable).string());
}
void create_accounting_schema(Sqlite& db){
 db.exec(R"SQL(
 CREATE TABLE v3_accounting_origin(
  id INTEGER PRIMARY KEY CHECK(id=1),
  executable_sha256 TEXT NOT NULL CHECK(length(executable_sha256)=64),
  worker_sha256 TEXT NOT NULL CHECK(length(worker_sha256)=64),
  protocol TEXT NOT NULL CHECK(protocol='linux_rusage_precommit_v1'));
 CREATE TABLE v3_attempts(
  attempt_id INTEGER PRIMARY KEY CHECK(attempt_id BETWEEN 0 AND 9999),
  pair_number INTEGER NOT NULL CHECK(pair_number BETWEEN 0 AND 99),
  seed INTEGER NOT NULL CHECK(seed>=0),
  left_case_id TEXT NOT NULL CHECK(length(left_case_id)=64),
  right_case_id TEXT NOT NULL CHECK(length(right_case_id)=64),
  state TEXT NOT NULL CHECK(state IN ('STARTED','ABANDONED','COMMITTED')),
  committed_pair INTEGER UNIQUE REFERENCES v3_pairs(pair_number),
  elapsed_ns INTEGER,parent_cpu_ns INTEGER,waited_child_cpu_ns INTEGER,
  CHECK((state='COMMITTED' AND committed_pair IS NOT NULL AND committed_pair=pair_number
    AND elapsed_ns IS NOT NULL AND elapsed_ns>0
    AND parent_cpu_ns IS NOT NULL AND parent_cpu_ns>=0
    AND waited_child_cpu_ns IS NOT NULL AND waited_child_cpu_ns>=0)
    OR (state!='COMMITTED' AND committed_pair IS NULL AND elapsed_ns IS NULL
     AND parent_cpu_ns IS NULL AND waited_child_cpu_ns IS NULL)));
 CREATE UNIQUE INDEX v3_attempt_active ON v3_attempts(state) WHERE state='STARTED';
 PRAGMA user_version=17;
 )SQL");
 Statement origin(db,"INSERT INTO v3_accounting_origin VALUES(1,?,?,'linux_rusage_precommit_v1')");
 origin.bind(1,executable_digest_v3(false));origin.bind(2,executable_digest_v3(true));origin.done();
}
void check_accounting_origin(Sqlite& db,bool writer){
 Statement q(db,"SELECT executable_sha256,worker_sha256,protocol FROM v3_accounting_origin WHERE id=1");
 require(q.row()&&q.str(0).size()==64&&q.str(1).size()==64&&q.str(2)=="linux_rusage_precommit_v1",
         "missing accounted run provenance");
 if(writer)require(q.str(0)==executable_digest_v3(false)&&q.str(1)==executable_digest_v3(true),
                   "accounted run executable/worker digest changed");
}
struct RusageWindowV3 {
 std::chrono::steady_clock::time_point wall;
 std::int64_t parent=0,children=0;
};
std::int64_t process_cpu_ns_v3(int who){
 struct rusage u{};
 require(::getrusage(who,&u)==0,"cannot read Linux CPU counter");
 const auto seconds=static_cast<std::int64_t>(u.ru_utime.tv_sec)+static_cast<std::int64_t>(u.ru_stime.tv_sec);
 const auto micros=static_cast<std::int64_t>(u.ru_utime.tv_usec)+static_cast<std::int64_t>(u.ru_stime.tv_usec);
 require(seconds>=0&&micros>=0&&seconds<=(std::numeric_limits<std::int64_t>::max()-2000000000LL)/1000000000LL,
         "CPU counter overflow");
 return seconds*1000000000LL+micros*1000LL;
}
RusageWindowV3 start_window_v3(){
 return {std::chrono::steady_clock::now(),process_cpu_ns_v3(RUSAGE_SELF),
         process_cpu_ns_v3(RUSAGE_CHILDREN)};
}
std::int64_t reserve_attempt_v3(Sqlite& db,std::size_t pair,std::uint64_t seed,const Case& a,const Case& b){
 Statement count(db,"SELECT COUNT(*) FROM v3_attempts");
 require(count.row()&&count.number(0)<10000,"attempt ledger capacity exhausted");
 const auto id=count.number(0);
 Statement prior(db,"SELECT seed,left_case_id,right_case_id FROM v3_attempts WHERE pair_number=? ORDER BY attempt_id DESC LIMIT 1");
 prior.bind(1,static_cast<std::int64_t>(pair));
 if(prior.row())require(prior.number(0)==static_cast<std::int64_t>(seed)&&prior.str(1)==a.id&&prior.str(2)==b.id,
                        "retry changed reserved proposal");
 Statement st(db,"INSERT INTO v3_attempts(attempt_id,pair_number,seed,left_case_id,right_case_id,state) VALUES(?,?,?,?,?,'STARTED')");
 st.bind(1,id);st.bind(2,static_cast<std::int64_t>(pair));st.bind(3,static_cast<std::int64_t>(seed));
 st.bind(4,a.id);st.bind(5,b.id);st.done(); // autocommit before worker launch
 return id;
}
void commit_attempt_v3(Sqlite& db,std::int64_t id,std::size_t pair,const RusageWindowV3& start){
 const auto parent=process_cpu_ns_v3(RUSAGE_SELF)-start.parent;
 const auto child=process_cpu_ns_v3(RUSAGE_CHILDREN)-start.children;
 const auto wall=std::chrono::duration_cast<std::chrono::nanoseconds>(
  std::chrono::steady_clock::now()-start.wall).count();
 require(wall>0&&parent>=0&&child>=0,"invalid attempt counter delta");
 Statement st(db,"UPDATE v3_attempts SET state='COMMITTED',committed_pair=?,elapsed_ns=?,parent_cpu_ns=?,waited_child_cpu_ns=? WHERE attempt_id=? AND state='STARTED'");
 st.bind(1,static_cast<std::int64_t>(pair));st.bind(2,wall);st.bind(3,parent);st.bind(4,child);st.bind(5,id);st.done();
 require(sqlite3_changes(db.db)==1,"attempt completion mismatch");
}
struct AccountingSummaryV3{
 std::int64_t attempts=0,committed=0,abandoned=0,started=0;
 std::int64_t elapsed=0,parent=0,child=0,timed=0,timed_unknown=0;
};
AccountingSummaryV3 audit_accounting_v3(Sqlite& db,std::size_t completed,std::size_t cap){
 check_accounting_origin(db,false);
 AccountingSummaryV3 out;
 auto add=[](std::int64_t& total,std::int64_t value){
  require(value>=0&&value<=std::numeric_limits<std::int64_t>::max()-total,"attempt accounting overflow");
  total+=value;
 };
 Statement q(db,"SELECT a.attempt_id,a.pair_number,a.seed,a.left_case_id,a.right_case_id,a.state,a.committed_pair,a.elapsed_ns,a.parent_cpu_ns,a.waited_child_cpu_ns,p.seed,p.left_case_id,p.right_case_id,p.pair_wall_ns,p.worker_timed_cpu_ns FROM v3_attempts a LEFT JOIN v3_pairs p ON p.pair_number=a.pair_number ORDER BY a.attempt_id");
 std::optional<std::tuple<std::int64_t,std::string,std::string>> expected;
 while(q.row()){
  require(q.number(0)==out.attempts&&out.attempts<10000&&q.number(1)==out.committed&&
          q.number(1)<static_cast<std::int64_t>(cap)&&out.started==0,"attempt sequence or cap mismatch");
  ++out.attempts;
  const auto identity=std::tuple{q.number(2),q.str(3),q.str(4)};
  require(q.number(2)>=0&&q.number(2)<=0x7fffffff&&q.str(3).size()==64&&q.str(4).size()==64&&q.str(3)!=q.str(4),
         "invalid attempt identity");
  require(!expected||*expected==identity,"retry changed attempt identity");
  expected=identity;
  const auto state=q.str(5);
  if(sqlite3_column_type(q.stmt,10)!=SQLITE_NULL)
   require(q.number(2)==q.number(10)&&q.str(3)==q.str(11)&&q.str(4)==q.str(12),"attempt does not bind committed proposal");
  if(state=="COMMITTED"){
   require(sqlite3_column_type(q.stmt,6)==SQLITE_INTEGER&&
           q.number(6)==q.number(1)&&sqlite3_column_type(q.stmt,10)==SQLITE_INTEGER&&
           sqlite3_column_type(q.stmt,7)==SQLITE_INTEGER&&sqlite3_column_type(q.stmt,8)==SQLITE_INTEGER&&
           sqlite3_column_type(q.stmt,9)==SQLITE_INTEGER&&q.number(7)>=q.number(13)&&q.number(7)>0,
           "invalid committed attempt/cost linkage");
   ++out.committed;expected.reset();
   add(out.elapsed,q.number(7));add(out.parent,q.number(8));add(out.child,q.number(9));
   if(sqlite3_column_type(q.stmt,14)==SQLITE_NULL)++out.timed_unknown;
   else add(out.timed,q.number(14));
  }else{
   require(state=="STARTED"||state=="ABANDONED","unknown attempt state");
   for(int col=6;col<=9;++col)require(sqlite3_column_type(q.stmt,col)==SQLITE_NULL,"fabricated abandoned cost");
   if(state=="STARTED")++out.started;else ++out.abandoned;
  }
 }
 require(out.committed==static_cast<std::int64_t>(completed),"committed pair lacks attempt");
 return out;
}
void report_accounting_v3(Sqlite& db,std::size_t completed,std::size_t cap){
 const auto a=audit_accounting_v3(db,completed,cap);
 Json j;j.put("schema","phaseshift.v3.attempt_accounting.1");
 j.put("state","ACCOUNTING_INCOMPLETE");
 j.put("reason","Linux waited-child rusage does not prove descendant attribution; abandoned attempts and costs outside precommit windows are unmeasured");
 j.put("attempts",a.attempts);j.put("committed",a.committed);j.put("abandoned",a.abandoned);j.put("started",a.started);
 j.put("observed_precommit_wall_ns",a.elapsed);j.put("observed_parent_cpu_ns",a.parent);
 j.put("observed_waited_child_cpu_ns",a.child);j.put("observed_worker_timed_cpu_ns",a.timed);
 j.put("committed_pairs_without_worker_cpu",a.timed_unknown);
 std::cout<<j.str()<<"\n";
}

struct Plan {
 std::string source,axis,left,right;
 Config config;
 int pairs=0;
 bool adaptive=false;
 bool conditional=false;
 bool implication=false; // opt-in v13 frozen implication semantics
 bool region_tree=false; // opt-in v14, previous schedules unchanged
 bool acquisition=false; // opt-in v15 empirical cost acquisition
 bool coverage_first=false; // opt-in v18: retain cost ranking, avoid forced replication
 bool accounted=false; // opt-in v17 attempt accounting, unchanged v15 policy
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
Plan make_conditional_plan(const std::string& source,int count,bool implication=false,
                           bool region_tree=false,bool acquisition=false,bool coverage_first=false){
 const auto cfg=config_from_json(Json::parse(source));
 Plan result;result.source=source;result.config=cfg;result.axis=cfg.axis;
 result.pairs=count;result.conditional=true;result.implication=implication;
 result.region_tree=region_tree;result.acquisition=acquisition;
 result.coverage_first=coverage_first;
 require(cfg.axes.count(result.axis)!=0,"v12 primary axis not in frozen declared space");
 const auto& labels=cfg.axes.at(result.axis);
 require(labels.size()>=2,"v12 requires at least two primary labels");
 result.left=labels[0];result.right=labels[1];
 validate_plan(result);
 const auto axes=ordered_axes(cfg);
 const auto scopes=conditional_scopes(result);
 const AdmissibleSearchSpace legal(axes,scopes);
 ConditionalPairPortfolio probe(legal,axes,result.axis,scopes,
  cfg.seed,static_cast<std::size_t>(count),cfg.min_ratio,64,region_tree,acquisition,coverage_first);
 require(probe.next().has_value(),"v12 no feasible conditional pair");
 return result;
}
void enable_adaptive_schema(Sqlite& db);
void enable_conditional_schema(Sqlite& db,bool implication,bool region_tree,bool acquisition,bool coverage_first);
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
 if(plan.conditional)enable_conditional_schema(db,plan.implication,plan.region_tree,plan.acquisition,plan.coverage_first);
 if(plan.accounted)create_accounting_schema(db);
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
void enable_conditional_schema(Sqlite& db,bool implication,bool region_tree,bool acquisition,bool coverage_first){
 db.exec(R"SQL(
 CREATE TABLE v3_conditional_decisions(
  pair_number INTEGER PRIMARY KEY REFERENCES v3_pairs(pair_number),
  axis TEXT NOT NULL, left_coords TEXT NOT NULL,right_coords TEXT NOT NULL,
  arm TEXT NOT NULL CHECK(arm IN ('explore','replicate')),
  prior_evidence_digest TEXT NOT NULL CHECK(length(prior_evidence_digest)=64),
  CHECK(left_coords<>right_coords));
 )SQL");
 if(acquisition)db.exec(R"SQL(
 CREATE TABLE v3_acquisition_decisions(
  pair_number INTEGER PRIMARY KEY REFERENCES v3_pairs(pair_number),
  diagnostic TEXT NOT NULL, actual_wall_ns INTEGER NOT NULL CHECK(actual_wall_ns>0),
  actual_worker_cpu_ns INTEGER CHECK(actual_worker_cpu_ns>=0));
 )SQL");
 db.exec(coverage_first?"PRAGMA user_version=18;":acquisition?"PRAGMA user_version=15;":region_tree?"PRAGMA user_version=14;":
         implication?"PRAGMA user_version=13;":"PRAGMA user_version=12;");
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
 require(schema==10||schema==11||schema==12||schema==13||schema==14||schema==15||schema==17||schema==18,"not a supported v3 pilot SQLite database");
 Statement st(db,"SELECT source_json,source_sha256,axis,left_label,right_label,max_pairs,protocol FROM v3_plan WHERE id=1");
 require(st.row(),"missing immutable v3 plan");
 const auto source=st.str(0),digest=st.str(1),axis=st.str(2),left=st.str(3),right=st.str(4);
 const auto count=st.number(5);
 const auto protocol=st.str(6);
 require(sha256(source)==digest,"frozen plan digest mismatch");
 require(protocol=="v3_matched_discovery_pilot_1"&&count>0&&count<=100,"invalid v3 protocol/count");
 auto result=(schema>=12)?
   make_conditional_plan(source,static_cast<int>(count),
       schema==13||(schema>=14&&!config_from_json(Json::parse(source)).conditional_rules.empty()),
       schema>=14,schema==15||schema==17||schema==18,schema==18):
   make_plan(source,left,right,static_cast<int>(count));
 result.adaptive=schema==11;
 result.accounted=schema==17;
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
                   AdaptivePairPolicy* adaptive,ConditionalPairPortfolio* conditional,
                   std::vector<MatchedBoundaryObservationV3>* taxonomy=nullptr){
 Statement rows(db,"SELECT pair_number,seed,left_first,left_case_id,right_case_id,status,log_contrast,pair_wall_ns,worker_timed_cpu_ns FROM v3_pairs ORDER BY pair_number");
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
   Statement sample(db,"SELECT status,ns,case_id,batch,algorithm,ordinal,cpu_ns FROM v3_samples WHERE pair_number=? AND side=? ORDER BY ordinal");
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
    raw.cpu_ns=sqlite3_column_type(sample.stmt,6)==SQLITE_NULL?-1:sample.number(6);
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
  std::optional<PairMeasuredCost> cost;
  if(plan.acquisition){
   std::int64_t total=0;bool known=true;
   for(const auto& group:persisted)for(const auto& raw:group){
    if(raw.cpu_ns<0||raw.cpu_ns>std::numeric_limits<std::int64_t>::max()-total)known=false;
    else if(known)total+=raw.cpu_ns;
   }
   require(known==(sqlite3_column_type(rows.stmt,8)!=SQLITE_NULL)&&
           (!known||rows.number(8)==total),"v15 worker CPU contradicts raw observations");
   cost=PairMeasuredCost{rows.number(7),known?std::optional<std::int64_t>{total}:std::nullopt};
   Statement audit(db,"SELECT diagnostic,actual_wall_ns,actual_worker_cpu_ns FROM v3_acquisition_decisions WHERE pair_number=?");
   audit.bind(1,static_cast<std::int64_t>(completed));
   require(audit.row()&&audit.str(0)==selected.conditional_choice->acquisition_audit&&
           audit.number(1)==cost->wall_ns&&
           known==(sqlite3_column_type(audit.stmt,2)!=SQLITE_NULL)&&
           (!known||audit.number(2)==total),"v15 acquisition cost/diagnostic replay mismatch");
   require(!audit.row(),"duplicate v15 acquisition diagnostic");
  }
  if(taxonomy){
   require(selected.conditional_choice.has_value(),"taxonomy requires conditional run");
   const auto& chosen=*selected.conditional_choice;
   const auto& labels=plan.config.axes.at(plan.axis);
   MatchedBoundaryObservationV3 item;item.seed=seed;
   for(std::size_t i=0;i<chosen.left.size();++i){
    const auto& [axis,label]=chosen.left[i];
    if(axis==plan.axis){
     item.left=static_cast<std::size_t>(std::find(labels.begin(),labels.end(),label)-labels.begin());
     item.right=static_cast<std::size_t>(std::find(labels.begin(),labels.end(),chosen.right[i].second)-labels.begin());
    }else item.context.push_back(chosen.left[i]);
   }
   const auto side_status=[](const std::vector<Observation>& group,std::optional<double> metric){
    if(metric)return std::string("ok");
    std::set<std::string> failures;
    for(const auto& raw:group)if(raw.status!="ok")failures.insert(raw.status);
    return failures.size()==1?*failures.begin():std::string("mixed_or_invalid");
   };
   item.left_metric=left_metric;item.right_metric=right_metric;
   item.left_status=side_status(persisted[0],left_metric);
   item.right_status=side_status(persisted[1],right_metric);
   taxonomy->push_back(std::move(item));
  }
  if(adaptive)adaptive->on_committed(*selected.adaptive_choice,
                                  Cohort::Discovery,recalculated);
  if(conditional)conditional->on_committed(*selected.conditional_choice,
                                  Cohort::Discovery,recalculated,cost);
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
  std::optional<RusageWindowV3> window;
  if(plan.accounted)window=start_window_v3();
  const auto selected=select_pair(planner,plan,adaptive,conditional);
  const auto& proposal=selected.matched;
  const auto seed=proposal.first.key.replicate.seed;
  const auto a=materialize_selected(plan,selected,0,seed),
             b=materialize_selected(plan,selected,1,seed);
  std::optional<std::int64_t> attempt;
  if(plan.accounted){
   attempt=reserve_attempt_v3(db,completed,seed,a,b);
   if(const auto* fault=std::getenv("PHASESHIFT_V3_FAILPOINT");fault&&std::string(fault)=="stop_after_attempt_reservation")
    std::raise(SIGSTOP);
  }
  const auto started=std::chrono::steady_clock::now();
  auto measure_one=[&](const Case& c){
   auto rows=measure(c,algorithms,plan.config.blocks,plan.config.warmup,
    plan.config.sample_ms,plan.config.rss_mb,plan.config.minimum_batch_ms,
    plan.config.isolation_mode=="per_sample",deadline,plan.config.measurement_protocol_version);
   (void)contain_descendants_v3();
   return rows;
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
   if(plan.acquisition){
    Statement audit(db,"INSERT INTO v3_acquisition_decisions VALUES(?,?,?,?)");
    audit.bind(1,static_cast<std::int64_t>(completed));
    audit.bind(2,selected.conditional_choice->acquisition_audit);audit.bind(3,wall);
    if(cpu_known)audit.bind(4,cpu);else audit.bind_null(4);
    audit.done();
   }
   insert_samples(db,completed,0,a,left);insert_samples(db,completed,1,b,right);
   if(attempt){
    commit_attempt_v3(db,*attempt,completed,*window);
    if(const auto* fault=std::getenv("PHASESHIFT_V3_FAILPOINT");fault&&std::string(fault)=="during_pair_commit")
     std::_Exit(90);
   }
   db.exec("COMMIT");
  }catch(...){db.exec("ROLLBACK");throw;}
  if(const auto* fault=std::getenv("PHASESHIFT_V3_FAILPOINT");fault&&std::string(fault)=="after_pair_commit")
   std::_Exit(89);
  if(adaptive)adaptive->on_committed(*selected.adaptive_choice,
                                  Cohort::Discovery,contrast);
  if(conditional)conditional->on_committed(*selected.conditional_choice,
                                  Cohort::Discovery,contrast,
     plan.acquisition?std::optional<PairMeasuredCost>{PairMeasuredCost{wall,
         cpu_known?std::optional<std::int64_t>{cpu}:std::nullopt}}:std::nullopt);
  ++completed;++this_call;
 }
 std::cout<<"v3 matched discovery committed "<<completed<<"/"<<plan.pairs
          <<" pairs; cohort=discovery; significance=not_established\n";
}
Json context_json_v3(const std::vector<AxisValue>& context){
 Json out;
 for(const auto& [axis,label]:context)out.put(axis.c_str(),label);
 return out;
}
void taxonomy_json(const Plan& plan,const std::vector<MatchedBoundaryObservationV3>& rows,
                   std::size_t completed){
 std::vector<double> axis;
 const auto& labels=plan.config.axes.at(plan.axis);
 for(const auto& label:labels)axis.push_back(static_cast<double>(integer(label)));
 const auto report=classify_matched_v3(axis,rows,std::log(plan.config.min_ratio),!plan.config.b.empty());
 Json output;output.put("schema","phaseshift.v3.taxonomy.1");
 output.put("cohort","discovery");output.put("confirmed",false);
 output.put("localization","descriptive_not_confidence_interval");
 output.put("source_config_sha256",sha256(plan.source));output.put("axis",plan.axis);
 output.put("minimum_log_effect",std::log(plan.config.min_ratio));
 output.put("observations",static_cast<std::int64_t>(report.observations));
 output.put("committed_pairs",static_cast<std::int64_t>(completed));
 output.put("planned_pairs",static_cast<std::int64_t>(plan.pairs));
 output.put("missing_or_unsupported_edges",static_cast<std::int64_t>(report.missing_or_unsupported_edges));
 Json candidates(json_object_new_array());
 for(const auto& c:report.candidates){
  Json row;row.put("kind",c.kind);row.put("detail",c.detail);
  row.put("context",context_json_v3(c.context));row.put("other_context",context_json_v3(c.other_context));
  row.put("left",labels.at(c.left));row.put("right",labels.at(c.right));
  row.put("min_matched_support",static_cast<std::int64_t>(c.support));
  if(c.effect)row.put("descriptive_effect",*c.effect);
  json_object_array_add(candidates.p,json_object_get(row.p));
 }
 output.put("candidates",std::move(candidates));
 Json profiles(json_object_new_array());
 for(const auto& p:report.profiles){
  Json row;row.put("context",context_json_v3(p.context));
  row.put("left",labels.at(p.begin));row.put("right",labels.at(p.end));
  row.put("kind",p.shape.kind);row.put("best_rival",p.shape.rival);
  Json scores;
  for(const auto& [name,value]:p.shape.scores)scores.put(name.c_str(),value);
  row.put("penalized_scores",std::move(scores));
  row.put("constant_residual_variance_ratio",p.shape.residual_variance_ratio);
  json_object_array_add(profiles.p,json_object_get(row.p));
 }
 output.put("profiles",std::move(profiles));
 std::cout<<output.str()<<"\n";
}
// A separate schema-16 sidecar owns holdout data. Never feed it into discovery.
struct FamilySpecV3 {
 std::string manifest,canonical,digest,source_digest,revision;
 std::size_t n=0;
 double alpha=0.;
 bool assumptions=false;
 std::vector<FrozenEdgeMemberV3> members;
 std::vector<std::uint64_t> seeds;
};
std::string source_digest_v3(Sqlite& db,const Plan& plan){
 std::string bytes="phaseshift.v3.family-source.1";
 auto append=[&](const std::string& value){bytes+=std::to_string(value.size())+":"+value;};
 auto record=[&](const char* query){
  append(query);
  Statement rows(db,query);
  while(rows.row()){
   append("row");
   for(int i=0;i<sqlite3_column_count(rows.stmt);++i){
    const auto type=sqlite3_column_type(rows.stmt,i);
    append(std::to_string(type));
    if(type==SQLITE_INTEGER)append(std::to_string(rows.number(i)));
    else if(type==SQLITE_FLOAT)append(std::to_string(std::bit_cast<std::uint64_t>(sqlite3_column_double(rows.stmt,i))));
    else if(type==SQLITE_TEXT){
     const auto* p=sqlite3_column_text(rows.stmt,i);
     append(std::string(reinterpret_cast<const char*>(p),static_cast<std::size_t>(sqlite3_column_bytes(rows.stmt,i))));
    }else require(type==SQLITE_NULL,"unexpected family fingerprint type");
   }
  }
 };
 record("PRAGMA user_version");
 record("SELECT * FROM v3_plan ORDER BY id");
 record("SELECT case_id,family,params,seed,input_sha256 FROM v3_cases ORDER BY case_id");
 record("SELECT * FROM v3_pairs ORDER BY pair_number");
 record("SELECT * FROM v3_samples ORDER BY pair_number,side,ordinal");
 record("SELECT * FROM v3_conditional_decisions ORDER BY pair_number");
 if(plan.acquisition)record("SELECT * FROM v3_acquisition_decisions ORDER BY pair_number");
 if(plan.accounted){
  record("SELECT * FROM v3_accounting_origin ORDER BY id");
  record("SELECT * FROM v3_attempts ORDER BY attempt_id");
 }
 return sha256(bytes);
}
FamilySpecV3 make_family_spec(const Plan& plan,const std::vector<MatchedBoundaryObservationV3>& discovery,
 const std::string& manifest,const std::string& source_digest,const std::string& revision){
 require(discovery.size()==static_cast<std::size_t>(plan.pairs),"family requires completed discovery");
 require(revision.size()==40&&std::all_of(revision.begin(),revision.end(),[](char c){
  return (c>='0'&&c<='9')||(c>='a'&&c<='f');}),"family requires source revision");
 const auto doc=Json::parse(manifest);
 require(doc.p&&json_object_get_type(doc.p)==json_type_object&&json_object_object_length(doc.p)==5,
         "family manifest must have five fields");
 auto typed=[&](const char* field,json_type type){
  auto* value=doc.get(field);
  require(value&&json_object_get_type(value)==type,std::string("family invalid field: ")+field);
 };
 typed("family_id",json_type_string);typed("seed_pairs",json_type_int);
 typed("sign_model",json_type_string);typed("members",json_type_array);
 require(doc.get("alpha")&&(json_object_get_type(doc.get("alpha"))==json_type_double||
          json_object_get_type(doc.get("alpha"))==json_type_int),"invalid family alpha type");
 const auto id=doc.s("family_id"),model=doc.s("sign_model");
 require(!id.empty()&&id.size()<=64&&std::all_of(id.begin(),id.end(),[](unsigned char c){
  return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_';
 }),"invalid family ID");
 require(model=="independent_seed_signs_declared"||model=="unverified","unknown sign model");
 const auto n=doc.i("seed_pairs");const auto alpha=doc.d("alpha");
 require(n>=20&&n<=100&&n==plan.config.confirm_blocks&&std::isfinite(alpha)&&alpha>=.001&&alpha<=.25,
         "family seed count/alpha outside frozen bounds");
 FamilySpecV3 f;f.manifest=manifest;f.source_digest=source_digest;f.revision=revision;
 f.n=static_cast<std::size_t>(n);f.alpha=alpha;f.assumptions=model=="independent_seed_signs_declared";
 auto* list=doc.get("members");const auto size=json_object_array_length(list);
 require(size>=1&&size<=16,"family must contain 1..16 members");
 require(size*f.n<=1600,"family holdout cap");
 std::set<std::string> edge_keys;
 Json frozen;frozen.put("protocol","phaseshift.v3.fixed_edge_family.1");
 frozen.put("source_revision",revision);frozen.put("source_snapshot_sha256",source_digest);
 frozen.put("source_config_sha256",sha256(plan.source));frozen.put("family_id",id);
 frozen.put("sign_model",model);frozen.put("test","one_sided_exact_binomial_sign_tail");
 frozen.put("null","P(direction * matched_log_contrast > minimum_log_effect) <= 0.5 per independent seed");
 frozen.put("multiplicity","holm_all_members");frozen.put("alpha",alpha);
 frozen.put("seed_pairs",n);frozen.put("minimum_log_effect",std::log(plan.config.min_ratio));
 frozen.put("metric",plan.config.b.empty()?"right_minus_left_log_time":"right_minus_left_log_ratio");
 Json fm(json_object_new_array());
 const auto& labels=plan.config.axes.at(plan.axis);
 for(std::size_t i=0;i<size;++i){
  auto* item=json_object_array_get_idx(list,i);
  require(item&&json_object_get_type(item)==json_type_object&&json_object_object_length(item)==2,
          "invalid family member");
  const Json member(json_object_get(item));
  require(member.get("discovery_pair")&&json_object_get_type(member.get("discovery_pair"))==json_type_int&&
          member.get("direction")&&json_object_get_type(member.get("direction"))==json_type_int,
          "family member must contain integer pair and direction");
  const auto ref=member.i("discovery_pair"),direction=member.i("direction");
  require(ref>=0&&static_cast<std::size_t>(ref)<discovery.size()&&(direction==1||direction==-1),
          "family selection outside discovery or invalid direction");
  const auto& observed=discovery[static_cast<std::size_t>(ref)];
  require(observed.left_metric&&observed.right_metric,"cannot nominate censored discovery edge");
  const auto edge=encode_coords(observed.context)+":"+std::to_string(observed.left)+":"+std::to_string(observed.right);
  require(edge_keys.insert(edge).second,"duplicate structural edge in family");
  f.members.push_back({static_cast<std::size_t>(ref),static_cast<int>(direction)});
  Json row;row.put("member_index",static_cast<std::int64_t>(i));row.put("discovery_pair",ref);
  row.put("direction",direction);row.put("kind","MATCHED_EDGE_CONTRAST");
  row.put("context",encode_coords(observed.context));
  row.put("axis",plan.axis);row.put("left",labels.at(observed.left));row.put("right",labels.at(observed.right));
  json_object_array_add(fm.p,json_object_get(row.p));
 }
 frozen.put("members",std::move(fm));
 // Discovery planner uses only 31-bit seeds; holdout uses a fixed disjoint
 // high-range stream keyed ONLY by the frozen experiment seed. Membership
 // and observed outcomes cannot tune the seed stream. Uniqueness is checked.
 std::uint64_t state=plan.config.seed^0xb3375e6d19f1bb91ULL;
 std::set<std::uint64_t> reserved;
 for(const auto& row:discovery)reserved.insert(row.seed);
 Json seed_plan(json_object_new_array());
 for(std::size_t i=0;i<size*f.n;++i){
  std::uint64_t seed=0;
  for(int tries=0;tries<1024;++tries){
   const auto candidate=(splitmix64(state)&0x3fffffffffffffffULL)|0x4000000000000000ULL;
   if(reserved.insert(candidate).second){seed=candidate;break;}
  }
  require(seed!=0,"holdout seed collision exhaustion");
  f.seeds.push_back(seed);
  json_object_array_add(seed_plan.p,json_object_new_int64(static_cast<std::int64_t>(seed)));
 }
 frozen.put("holdout_seeds",std::move(seed_plan));
 f.canonical=frozen.str();f.digest=sha256(f.canonical);
 return f;
}
Case family_case(const Plan& plan,const MatchedBoundaryObservationV3& edge,
                 bool right,std::uint64_t seed){
 auto c=plan.config.base;c.seed=seed;
 for(const auto& [axis,label]:edge.context)apply_axis(c,axis,label);
 apply_axis(c,plan.axis,plan.config.axes.at(plan.axis).at(right?edge.right:edge.left));
 return make_case(c,plan.config.max_bytes);
}
void freeze_family_sidecar(Sqlite& db,const FamilySpecV3& f,bool journaled){
 db.exec("BEGIN IMMEDIATE");
 try{
  db.exec(R"SQL(
 CREATE TABLE v3_family(
  id INTEGER PRIMARY KEY CHECK(id=1),manifest TEXT NOT NULL,
  specification TEXT NOT NULL,spec_sha256 TEXT NOT NULL,source_sha256 TEXT NOT NULL,
  source_revision TEXT NOT NULL);
 CREATE TABLE v3_cases(
  case_id TEXT PRIMARY KEY, family TEXT NOT NULL, params TEXT NOT NULL,
  seed INTEGER NOT NULL, input BLOB NOT NULL,input_sha256 TEXT NOT NULL);
 CREATE TABLE v3_pairs(
  pair_number INTEGER PRIMARY KEY,seed INTEGER NOT NULL UNIQUE,
  left_first INTEGER NOT NULL CHECK(left_first IN (0,1)),
  left_case_id TEXT NOT NULL REFERENCES v3_cases(case_id),
  right_case_id TEXT NOT NULL REFERENCES v3_cases(case_id),
  status TEXT NOT NULL CHECK(status IN ('ok','censored')),
  log_contrast REAL,worker_timed_cpu_ns INTEGER,pair_wall_ns INTEGER NOT NULL CHECK(pair_wall_ns>0),
  CHECK((status='ok' AND log_contrast IS NOT NULL) OR (status='censored' AND log_contrast IS NULL)));
 CREATE TABLE v3_samples(
  pair_number INTEGER NOT NULL REFERENCES v3_pairs(pair_number),
  side INTEGER NOT NULL CHECK(side IN (0,1)),
  ordinal INTEGER NOT NULL,case_id TEXT NOT NULL REFERENCES v3_cases(case_id),
  algorithm TEXT NOT NULL,status TEXT NOT NULL,ns INTEGER,batch INTEGER,work INTEGER,cpu_ns INTEGER,
  peak_rss_bytes INTEGER,pilot_ns INTEGER,pilot_iterations INTEGER,protocol TEXT NOT NULL,
  block INTEGER NOT NULL,output_digest TEXT NOT NULL,detail TEXT NOT NULL,
  order_position INTEGER,order_seed TEXT NOT NULL,
  PRIMARY KEY(pair_number,side,ordinal),
  CHECK((status='ok' AND ns>0) OR (status!='ok' AND ns IS NULL)));
 )SQL");
  if(journaled){
   // Opt-in schema 19: every holdout worker launch is durably journaled
   // before measurement, so an interrupted and re-measured pair is visible.
   db.exec(R"SQL(
 CREATE TABLE v3_holdout_journal_origin(
  id INTEGER PRIMARY KEY CHECK(id=1),
  protocol TEXT NOT NULL CHECK(protocol='phaseshift.v3.holdout_attempt_journal.1'));
 CREATE TABLE v3_holdout_attempts(
  attempt_id INTEGER PRIMARY KEY CHECK(attempt_id BETWEEN 0 AND 99999),
  pair_number INTEGER NOT NULL CHECK(pair_number BETWEEN 0 AND 1599),
  seed INTEGER NOT NULL,
  left_first INTEGER NOT NULL CHECK(left_first IN (0,1)),
  left_case_id TEXT NOT NULL CHECK(length(left_case_id)=64),
  right_case_id TEXT NOT NULL CHECK(length(right_case_id)=64),
  state TEXT NOT NULL CHECK(state IN ('STARTED','ABANDONED','COMMITTED')),
  committed_pair INTEGER UNIQUE REFERENCES v3_pairs(pair_number),
  CHECK((state='COMMITTED' AND committed_pair IS NOT NULL AND committed_pair=pair_number)
    OR (state!='COMMITTED' AND committed_pair IS NULL)));
 CREATE UNIQUE INDEX v3_holdout_attempt_active ON v3_holdout_attempts(state) WHERE state='STARTED';
 INSERT INTO v3_holdout_journal_origin VALUES(1,'phaseshift.v3.holdout_attempt_journal.1');
 PRAGMA user_version=19;
 )SQL");
  }else db.exec("PRAGMA user_version=16;");
  Statement st(db,"INSERT INTO v3_family VALUES(1,?,?,?,?,?)");
  st.bind(1,f.manifest);st.bind(2,f.canonical);st.bind(3,f.digest);
  st.bind(4,f.source_digest);st.bind(5,f.revision);st.done();
  db.exec("COMMIT");
 }catch(...){db.exec("ROLLBACK");throw;}
}
int family_sidecar_version(Sqlite& db){
 Statement ver(db,"PRAGMA user_version");
 require(ver.row()&&(ver.number(0)==16||ver.number(0)==19),"not v16/v19 family sidecar");
 const auto version=static_cast<int>(ver.number(0));
 // A genuine v16 sidecar never contains a journal; reject a downgraded v19
 // sidecar that would otherwise hide abandoned holdout attempts.
 Statement journal(db,"SELECT COUNT(*) FROM sqlite_master WHERE name IN ('v3_holdout_attempts','v3_holdout_journal_origin')");
 require(journal.row()&&journal.number(0)==(version==19?2:0),"family sidecar journal/version mismatch");
 return version;
}
FamilySpecV3 read_family_sidecar(Sqlite& db,const Plan& plan,
 const std::vector<MatchedBoundaryObservationV3>& discovery,const std::string& source_digest){
 (void)family_sidecar_version(db);
 Statement st(db,"SELECT manifest,specification,spec_sha256,source_sha256,source_revision FROM v3_family WHERE id=1");
 require(st.row()&&st.str(3)==source_digest,"changed discovery snapshot");
 const auto f=make_family_spec(plan,discovery,st.str(0),source_digest,st.str(4));
 require(f.canonical==st.str(1)&&f.digest==st.str(2),"altered frozen family specification");
 return f;
}
using HoldoutEffectsV3=std::vector<std::vector<std::optional<double>>>;
std::size_t replay_family_sidecar(Sqlite& db,const Plan& plan,const FamilySpecV3& f,
 const std::vector<MatchedBoundaryObservationV3>& discovery,HoldoutEffectsV3& effects){
 effects.assign(f.members.size(),{});
 Statement rows(db,"SELECT pair_number,seed,left_first,left_case_id,right_case_id,status,log_contrast,worker_timed_cpu_ns,pair_wall_ns FROM v3_pairs ORDER BY pair_number");
 std::size_t index=0;
 while(rows.row()){
  require(index<f.seeds.size()&&rows.number(0)==static_cast<std::int64_t>(index),"family pair sequence/cap mismatch");
  const auto member=index%f.members.size();const auto seed=f.seeds[index];
  const auto& edge=discovery.at(f.members[member].discovery_pair);
  const auto a=family_case(plan,edge,false,seed),b=family_case(plan,edge,true,seed);
  const auto left_first=((seed^index)&1ULL)==0;
  require(rows.number(1)==static_cast<std::int64_t>(seed)&&rows.number(2)==(left_first?1:0)&&
          rows.str(3)==a.id&&rows.str(4)==b.id&&rows.number(8)>0,"family seed/structure/order mismatch");
  std::vector<Observation> raw[2];
  std::int64_t cpu=0;bool cpu_known=true;
  for(int side=0;side<2;++side){
   const auto& c=side==0?a:b;
   Statement source(db,"SELECT family,params,seed,input_sha256,input FROM v3_cases WHERE case_id=?");
   source.bind(1,c.id);
   require(source.row()&&source.str(0)==c.family&&source.str(1)==case_params(c)&&
    source.number(2)==static_cast<std::int64_t>(seed)&&source.str(3)==sha256(c.bytes)&&
    sqlite3_column_bytes(source.stmt,4)==static_cast<int>(c.bytes.size())&&
    (c.bytes.empty()||std::memcmp(sqlite3_column_blob(source.stmt,4),c.bytes.data(),c.bytes.size())==0),
    "holdout raw case provenance mismatch");
   Statement sample(db,"SELECT ordinal,case_id,algorithm,status,ns,batch,cpu_ns FROM v3_samples WHERE pair_number=? AND side=? ORDER BY ordinal");
   sample.bind(1,static_cast<std::int64_t>(index));sample.bind(2,static_cast<std::int64_t>(side));
   std::map<std::string,std::size_t> counts;
   while(sample.row()){
    require(sample.number(0)==static_cast<std::int64_t>(raw[side].size())&&sample.str(1)==c.id,
      "holdout sample provenance mismatch");
    Observation obs;obs.algorithm=sample.str(2);obs.status=sample.str(3);
    require(obs.algorithm==plan.config.a||(!plan.config.b.empty()&&obs.algorithm==plan.config.b),"holdout algorithm mismatch");
    ++counts[obs.algorithm];
    const bool ok=obs.status=="ok";
    require(ok==(sqlite3_column_type(sample.stmt,4)!=SQLITE_NULL)&&
            ok==(sqlite3_column_type(sample.stmt,5)!=SQLITE_NULL),"holdout failed timing/batch promotion");
    obs.ns=ok?sample.number(4):0;obs.batch=ok?sample.number(5):0;
    require(!ok||(obs.ns>0&&obs.batch>0),"nonpositive holdout measurement");
    obs.cpu_ns=sqlite3_column_type(sample.stmt,6)==SQLITE_NULL?-1:sample.number(6);
    if(obs.cpu_ns<0||obs.cpu_ns>std::numeric_limits<std::int64_t>::max()-cpu)cpu_known=false;
    else if(cpu_known)cpu+=obs.cpu_ns;
    raw[side].push_back(std::move(obs));
   }
   const auto blocks=static_cast<std::size_t>(plan.config.blocks);
   require(counts[plan.config.a]==blocks*(plan.config.b.empty()?1U:2U)&&
           (plan.config.b.empty()||counts[plan.config.b]==2U*blocks)&&
           raw[side].size()==blocks*(plan.config.b.empty()?1U:4U),"holdout incomplete raw measurements");
  }
  const auto l=per_side_metric(raw[0],plan.config.a,plan.config.b);
  const auto r=per_side_metric(raw[1],plan.config.a,plan.config.b);
  const auto effect=l&&r?std::optional<double>{*r-*l}:std::nullopt;
  require(rows.str(5)==(effect?"ok":"censored")&&
    effect.has_value()==(sqlite3_column_type(rows.stmt,6)!=SQLITE_NULL)&&
    (!effect||std::abs(sqlite3_column_double(rows.stmt,6)-*effect)<=1e-12*(1.+std::abs(*effect)))&&
    cpu_known==(sqlite3_column_type(rows.stmt,7)!=SQLITE_NULL)&&(!cpu_known||rows.number(7)==cpu),
    "holdout effect/status/CPU replay mismatch");
  effects[member].push_back(effect);++index;
 }
 Statement sample_count(db,"SELECT COUNT(*) FROM v3_cases");
 require(sample_count.row()&&sample_count.number(0)==static_cast<std::int64_t>(index*2),"orphan holdout cases");
 Statement raw_count(db,"SELECT COUNT(*) FROM v3_samples");
 const auto expected=index*2*static_cast<std::size_t>(plan.config.blocks)*(plan.config.b.empty()?1U:4U);
 require(raw_count.row()&&raw_count.number(0)==static_cast<std::int64_t>(expected),"orphan holdout samples");
 return index;
}
// Frozen identity of holdout pair `index`: seed, presentation order and both
// regenerated cases. Shared by v16 execution, v19 journaling and v19 audit.
struct HoldoutProposalV3 {
 std::uint64_t seed=0;
 bool left_first=false;
 Case a,b;
};
HoldoutProposalV3 holdout_proposal_v3(const Plan& plan,const FamilySpecV3& f,
 const std::vector<MatchedBoundaryObservationV3>& discovery,std::size_t index){
 const auto seed=f.seeds.at(index);
 const auto& edge=discovery.at(f.members.at(index%f.members.size()).discovery_pair);
 return {seed,((seed^index)&1ULL)==0,family_case(plan,edge,false,seed),family_case(plan,edge,true,seed)};
}
struct HoldoutMeasuredV3 {
 std::vector<Observation> l,r;
 std::optional<double> effect;
 std::int64_t cpu=0,wall=0;
 bool known=true;
};
HoldoutMeasuredV3 measure_holdout_pair_v3(const Plan& plan,const HoldoutProposalV3& p,
 std::chrono::steady_clock::time_point deadline){
 const std::vector<std::string> algorithms=plan.config.b.empty()?std::vector<std::string>{plan.config.a}:
   std::vector<std::string>{plan.config.a,plan.config.b};
 const auto start=std::chrono::steady_clock::now();
 auto collect=[&](const Case& c){
  auto rows=measure(c,algorithms,plan.config.blocks,plan.config.warmup,
     plan.config.sample_ms,plan.config.rss_mb,plan.config.minimum_batch_ms,
     plan.config.isolation_mode=="per_sample",deadline,plan.config.measurement_protocol_version);
  (void)contain_descendants_v3();
  return rows;
 };
 HoldoutMeasuredV3 m;
 if(p.left_first){m.l=collect(p.a);m.r=collect(p.b);}else{m.r=collect(p.b);m.l=collect(p.a);}
 if(const auto* fault=std::getenv("PHASESHIFT_V3_FAILPOINT");fault&&std::string(fault)=="before_holdout_commit")std::_Exit(88);
 m.wall=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
 const auto lm=per_side_metric(m.l,plan.config.a,plan.config.b),rm=per_side_metric(m.r,plan.config.a,plan.config.b);
 m.effect=lm&&rm?std::optional<double>{*rm-*lm}:std::nullopt;
 require(m.wall>0&&(!m.effect||std::isfinite(*m.effect)),"invalid holdout worker effect/cost");
 for(const auto& group:{m.l,m.r})for(const auto& row:group){
  if(row.cpu_ns<0||row.cpu_ns>std::numeric_limits<std::int64_t>::max()-m.cpu)m.known=false;
  else if(m.known)m.cpu+=row.cpu_ns;
 }
 return m;
}
// Caller owns the open write transaction.
void insert_holdout_pair_v3(Sqlite& db,std::size_t index,const HoldoutProposalV3& p,const HoldoutMeasuredV3& m){
 {
  Statement count(db,"SELECT COUNT(*) FROM v3_pairs");
  require(count.row()&&count.number(0)==static_cast<std::int64_t>(index),"concurrent family progress; retry from fresh snapshot");
 }
 insert_case(db,p.a);insert_case(db,p.b);
 Statement st(db,"INSERT INTO v3_pairs VALUES(?,?,?,?,?,?,?,?,?)");
 st.bind(1,static_cast<std::int64_t>(index));st.bind(2,static_cast<std::int64_t>(p.seed));
 st.bind(3,static_cast<std::int64_t>(p.left_first?1:0));st.bind(4,p.a.id);st.bind(5,p.b.id);
 st.bind(6,m.effect?std::string("ok"):std::string("censored"));
 if(m.effect)require(sqlite3_bind_double(st.stmt,7,*m.effect)==SQLITE_OK,"holdout SQL effect");
 else st.bind_null(7);
 if(m.known)st.bind(8,m.cpu);else st.bind_null(8);
 st.bind(9,m.wall);st.done();
 insert_samples(db,index,0,p.a,m.l);insert_samples(db,index,1,p.b,m.r);
}
void run_family_pair(Sqlite& db,const Plan& plan,const FamilySpecV3& f,
 const std::vector<MatchedBoundaryObservationV3>& discovery,std::size_t index,
 std::chrono::steady_clock::time_point deadline){
 // Schema 16: atomic holdout pair. Acquire the write lock before the worker
 // call so competing resumptions cannot collect a second measurement.
 db.exec("BEGIN IMMEDIATE");
 try{
  {
   Statement count(db,"SELECT COUNT(*) FROM v3_pairs");
   require(count.row()&&count.number(0)==static_cast<std::int64_t>(index),"concurrent family progress; retry from fresh snapshot");
  }
  const auto p=holdout_proposal_v3(plan,f,discovery,index);
  const auto m=measure_holdout_pair_v3(plan,p,deadline);
  insert_holdout_pair_v3(db,index,p,m);
  db.exec("COMMIT");
 }catch(...){db.exec("ROLLBACK");throw;}
 if(const auto* fault=std::getenv("PHASESHIFT_V3_FAILPOINT");fault&&std::string(fault)=="after_holdout_commit")std::_Exit(89);
}

// Schema 19 holdout attempt journal. Limits: rows are not hash-chained, so a
// deliberate SQLite rewrite (delete and renumber) is not detectable; this
// guards against silent re-measurement by cooperating CLI writers only.
constexpr std::int64_t kHoldoutAttemptCapV3=100000;
struct HoldoutJournalV3 {
 std::int64_t attempts=0,committed=0,abandoned=0,started=0,interrupted_pairs=0;
};
HoldoutJournalV3 audit_holdout_journal_v3(Sqlite& db,const Plan& plan,const FamilySpecV3& f,
 const std::vector<MatchedBoundaryObservationV3>& discovery,std::size_t committed){
 {
  Statement origin(db,"SELECT protocol FROM v3_holdout_journal_origin WHERE id=1");
  require(origin.row()&&origin.str(0)=="phaseshift.v3.holdout_attempt_journal.1","missing holdout journal provenance");
 }
 Statement q(db,"SELECT a.attempt_id,a.pair_number,a.seed,a.left_first,a.left_case_id,a.right_case_id,a.state,a.committed_pair,"
                "p.seed,p.left_first,p.left_case_id,p.right_case_id FROM v3_holdout_attempts a "
                "LEFT JOIN v3_pairs p ON p.pair_number=a.committed_pair ORDER BY a.attempt_id");
 HoldoutJournalV3 out;
 std::optional<std::size_t> expected_index;
 HoldoutProposalV3 expected;
 bool interrupted=false;
 while(q.row()){
  require(out.started==0,"holdout attempt follows an unresolved STARTED attempt");
  require(q.number(0)==out.attempts&&out.attempts<kHoldoutAttemptCapV3,"holdout attempt sequence/cap mismatch");
  ++out.attempts;
  const auto index=static_cast<std::size_t>(out.committed);
  require(index<f.seeds.size()&&q.number(1)==static_cast<std::int64_t>(index),"holdout attempt outside frozen order");
  if(expected_index!=index){expected=holdout_proposal_v3(plan,f,discovery,index);expected_index=index;interrupted=false;}
  require(q.number(2)==static_cast<std::int64_t>(expected.seed)&&q.number(3)==(expected.left_first?1:0)&&
          q.str(4)==expected.a.id&&q.str(5)==expected.b.id,"holdout attempt differs from frozen schedule");
  const auto state=q.str(6);
  if(state=="COMMITTED"){
   require(sqlite3_column_type(q.stmt,7)==SQLITE_INTEGER&&q.number(7)==q.number(1)&&
           sqlite3_column_type(q.stmt,8)==SQLITE_INTEGER&&q.number(8)==q.number(2)&&q.number(9)==q.number(3)&&
           q.str(10)==q.str(4)&&q.str(11)==q.str(5),"committed holdout attempt does not bind its pair");
   ++out.committed;
  }else{
   require(state=="STARTED"||state=="ABANDONED","unknown holdout attempt state");
   require(sqlite3_column_type(q.stmt,7)==SQLITE_NULL,"uncommitted holdout attempt claims a pair");
   if(state=="STARTED")++out.started;
   else{
    ++out.abandoned;
    if(!interrupted){interrupted=true;++out.interrupted_pairs;}
   }
  }
 }
 require(out.committed==static_cast<std::int64_t>(committed),"committed holdout pair lacks journaled attempt");
 return out;
}
std::int64_t reserve_holdout_attempt_v3(Sqlite& db,std::size_t index,const HoldoutProposalV3& p){
 std::int64_t id=0;
 {
  Statement count(db,"SELECT COUNT(*) FROM v3_holdout_attempts");
  require(count.row()&&count.number(0)<kHoldoutAttemptCapV3,"holdout attempt journal capacity exhausted");
  id=count.number(0);
 }
 // Autocommit with synchronous=FULL: durable before any worker launch.
 Statement st(db,"INSERT INTO v3_holdout_attempts(attempt_id,pair_number,seed,left_first,left_case_id,right_case_id,state) VALUES(?,?,?,?,?,?,'STARTED')");
 st.bind(1,id);st.bind(2,static_cast<std::int64_t>(index));st.bind(3,static_cast<std::int64_t>(p.seed));
 st.bind(4,static_cast<std::int64_t>(p.left_first?1:0));st.bind(5,p.a.id);st.bind(6,p.b.id);st.done();
 return id;
}
void abandon_holdout_attempt_v3(Sqlite& db,std::int64_t id) noexcept{
 // Best effort while unwinding; a later writer abandons any STARTED row anyway.
 try{
  Statement st(db,"UPDATE v3_holdout_attempts SET state='ABANDONED' WHERE attempt_id=? AND state='STARTED'");
  st.bind(1,id);st.done();
 }catch(...){}
}
void run_journaled_family_pair(Sqlite& db,const Plan& plan,const FamilySpecV3& f,
 const std::vector<MatchedBoundaryObservationV3>& discovery,std::size_t index,
 std::chrono::steady_clock::time_point deadline){
 const auto p=holdout_proposal_v3(plan,f,discovery,index);
 const auto attempt=reserve_holdout_attempt_v3(db,index,p);
 if(const auto* fault=std::getenv("PHASESHIFT_V3_FAILPOINT");fault&&std::string(fault)=="stop_after_holdout_reservation")
  std::raise(SIGSTOP);
 HoldoutMeasuredV3 m;
 try{m=measure_holdout_pair_v3(plan,p,deadline);}
 catch(...){abandon_holdout_attempt_v3(db,attempt);throw;}
 db.exec("BEGIN IMMEDIATE");
 try{
  insert_holdout_pair_v3(db,index,p,m);
  Statement st(db,"UPDATE v3_holdout_attempts SET state='COMMITTED',committed_pair=? WHERE attempt_id=? AND state='STARTED'");
  st.bind(1,static_cast<std::int64_t>(index));st.bind(2,attempt);st.done();
  require(sqlite3_changes(db.db)==1,"holdout attempt completion mismatch");
  if(const auto* fault=std::getenv("PHASESHIFT_V3_FAILPOINT");fault&&std::string(fault)=="during_holdout_commit")
   std::_Exit(90);
  db.exec("COMMIT");
 }catch(...){
  db.exec("ROLLBACK");abandon_holdout_attempt_v3(db,attempt);throw;
 }
 if(const auto* fault=std::getenv("PHASESHIFT_V3_FAILPOINT");fault&&std::string(fault)=="after_holdout_commit")std::_Exit(89);
}
Json family_result_v3(const Plan& plan,const FamilySpecV3& f,const HoldoutEffectsV3& effects,
                      std::size_t count,const std::optional<HoldoutJournalV3>& journal=std::nullopt){
 const auto outcome=fixed_family_v3(f.members,effects,f.n,std::log(plan.config.min_ratio),f.alpha,f.assumptions);
 Json result;
 result.put("schema",journal?"phaseshift.v3.fixed_edge_family_journaled.1":"phaseshift.v3.fixed_edge_family.1");
 if(journal){
  // Disclosure only: retries re-measure the identical frozen seed and order,
  // but an interrupted measurement may have differed and is never recorded.
  result.put("holdout_attempt_protocol","phaseshift.v3.holdout_attempt_journal.1");
  result.put("holdout_attempts",journal->attempts);
  result.put("committed_attempts",journal->committed);
  result.put("abandoned_attempts",journal->abandoned);
  result.put("started_attempts",journal->started);
  result.put("pairs_with_abandoned_attempts",journal->interrupted_pairs);
  result.put("attempt_integrity",journal->started?"INTERRUPTED_ATTEMPT_PENDING":
             journal->abandoned?"RETRIED_AFTER_INTERRUPTION":"CLEAN");
 }
 result.put("cohort","confirmation");result.put("family_sha256",f.digest);
 result.put("source_snapshot_sha256",f.source_digest);
 result.put("complete",outcome.complete);result.put("committed_pairs",static_cast<std::int64_t>(count));
 result.put("planned_pairs",static_cast<std::int64_t>(f.seeds.size()));
 result.put("assumptions_empirically_validated",false);result.put("confirmed",false);
 // While incomplete, expose counts only at the family level, not signs/p-values.
 if(outcome.complete){
  Json rows(json_object_new_array());
  for(std::size_t i=0;i<outcome.members.size();++i){
   const auto& x=outcome.members[i];Json row;
   row.put("member_index",static_cast<std::int64_t>(i));row.put("state",x.state);
   row.put("valid_pairs",static_cast<std::int64_t>(x.valid));
   row.put("positive_pairs",static_cast<std::int64_t>(x.positive));
   row.put("raw_p",x.raw_p);row.put("holm_p",x.holm_p);
   json_object_array_add(rows.p,json_object_get(row.p));
  }
  result.put("members",std::move(rows));
 }
 return result;
}
void report_family_v3(const Plan& plan,const FamilySpecV3& f,const HoldoutEffectsV3& effects,
                     std::size_t count,const std::optional<HoldoutJournalV3>& journal=std::nullopt){
 std::cout<<family_result_v3(plan,f,effects,count,journal).str()<<"\n";
}
void family_command_v3(const std::string& command,const fs::path& run,const std::string& manifest_path,
 const Plan& plan,const std::vector<MatchedBoundaryObservationV3>& discovery,
 const std::string& fingerprint,std::size_t pause){
 const auto directory=run/"confirmation";const auto path=directory/"family.sqlite";
 const auto revision=Json::parse(environment()).s("source_revision");
 if(command=="family-freeze"||command=="family-freeze-journaled"){
  require(!manifest_path.empty(),"family-freeze needs manifest");
  const auto f=make_family_spec(plan,discovery,read_file(manifest_path),fingerprint,revision);
  require(fs::create_directory(directory),"family already reserved; no refreeze or replacement");
  const bool journaled=command=="family-freeze-journaled";
  Sqlite ledger(path);freeze_family_sidecar(ledger,f,journaled);
  report_family_v3(plan,f,HoldoutEffectsV3(f.members.size()),0,
                   journaled?std::optional<HoldoutJournalV3>{HoldoutJournalV3{}}:std::nullopt);
  return;
 }
 require(fs::is_regular_file(path),"family must be frozen before holdout");
 const bool read_only=command=="family-report";
 Sqlite ledger(path,read_only);
 const bool journaled=family_sidecar_version(ledger)==19;
 std::optional<AccountedRunLock> lock;
 if(journaled&&!read_only){
  // Exclusive cooperating writer before replay, so no live attempt is abandoned.
  lock.emplace(path,"journaled holdout family already has a writer");
  ledger.exec("PRAGMA synchronous=FULL");
 }
 std::optional<HoldoutJournalV3> journal;
 ledger.exec("BEGIN");
 const auto f=read_family_sidecar(ledger,plan,discovery,fingerprint);
 HoldoutEffectsV3 effects;auto count=replay_family_sidecar(ledger,plan,f,discovery,effects);
 if(journaled)journal=audit_holdout_journal_v3(ledger,plan,f,discovery,count);
 ledger.exec("COMMIT");
 if(!read_only){
  require(revision==f.revision,"holdout executable source revision differs from frozen family");
  if(journaled)ledger.exec("UPDATE v3_holdout_attempts SET state='ABANDONED' WHERE state='STARTED'");
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(plan.config.total_wall_s);
  const auto stop=pause?std::min(f.seeds.size(),count+pause):f.seeds.size();
  while(count<stop&&std::chrono::steady_clock::now()<deadline){
   if(journaled)run_journaled_family_pair(ledger,plan,f,discovery,count,deadline);
   else run_family_pair(ledger,plan,f,discovery,count,deadline);
   ++count;
  }
  ledger.exec("BEGIN");count=replay_family_sidecar(ledger,plan,f,discovery,effects);
  if(journaled)journal=audit_holdout_journal_v3(ledger,plan,f,discovery,count);
  ledger.exec("COMMIT");
 }
 report_family_v3(plan,f,effects,count,journal);
}

// ---------------------------------------------------------------------------
// V3-09 signed boundary witness. The witness binds a complete frozen family
// outcome to logical digests of the replayed discovery and holdout evidence.
// It is an offline Ed25519 detached signature; key custody, revocation and
// trusted timestamps are the operator's responsibility and are not provided.
using PkeyV3=std::unique_ptr<EVP_PKEY,decltype(&EVP_PKEY_free)>;
PkeyV3 read_ed25519_key_v3(const std::string& path,bool is_private){
 std::unique_ptr<BIO,decltype(&BIO_free)> bio(BIO_new_file(path.c_str(),"rb"),&BIO_free);
 require(bio!=nullptr,"cannot open Ed25519 key file");
 PkeyV3 key(is_private?PEM_read_bio_PrivateKey(bio.get(),nullptr,nullptr,nullptr):
                       PEM_read_bio_PUBKEY(bio.get(),nullptr,nullptr,nullptr),&EVP_PKEY_free);
 require(key!=nullptr,is_private?"invalid private key PEM":"invalid public key PEM");
 require(EVP_PKEY_get_base_id(key.get())==EVP_PKEY_ED25519,"witness key must be Ed25519");
 return key;
}
std::string public_key_der_v3(EVP_PKEY* key){
 unsigned char* der=nullptr;
 const int size=i2d_PUBKEY(key,&der);
 require(size>0&&der!=nullptr,"cannot encode Ed25519 public key");
 std::string out(reinterpret_cast<const char*>(der),static_cast<std::size_t>(size));
 OPENSSL_free(der);
 return out;
}
std::string public_key_pem_v3(EVP_PKEY* key){
 std::unique_ptr<BIO,decltype(&BIO_free)> bio(BIO_new(BIO_s_mem()),&BIO_free);
 require(bio!=nullptr&&PEM_write_bio_PUBKEY(bio.get(),key)==1,"cannot encode public key PEM");
 char* data=nullptr;
 const auto size=BIO_get_mem_data(bio.get(),&data);
 require(size>0&&data!=nullptr,"empty public key PEM");
 return {data,static_cast<std::size_t>(size)};
}
std::string ed25519_sign_v3(EVP_PKEY* key,const std::string& message){
 std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(),&EVP_MD_CTX_free);
 std::string sig(64,'\0');std::size_t size=sig.size();
 require(ctx&&EVP_DigestSignInit(ctx.get(),nullptr,nullptr,nullptr,key)==1&&
         EVP_DigestSign(ctx.get(),reinterpret_cast<unsigned char*>(sig.data()),&size,
           reinterpret_cast<const unsigned char*>(message.data()),message.size())==1&&size==64,
         "Ed25519 signing failed");
 return sig;
}
bool ed25519_verify_v3(EVP_PKEY* key,const std::string& message,const std::string& sig){
 if(sig.size()!=64)return false;
 std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(),&EVP_MD_CTX_free);
 require(ctx&&EVP_DigestVerifyInit(ctx.get(),nullptr,nullptr,nullptr,key)==1,"Ed25519 verifier setup failed");
 return EVP_DigestVerify(ctx.get(),reinterpret_cast<const unsigned char*>(sig.data()),sig.size(),
          reinterpret_cast<const unsigned char*>(message.data()),message.size())==1;
}
// Documented logical digest (reproduced independently by the witness test):
// domain, then for each query: len:query, and per row "3:row" plus for each
// column len:type-code and len:value (integer decimal, REAL as IEEE-754 bit
// pattern decimal, TEXT bytes, NULL no value). BLOBs are excluded by query.
std::string logical_digest_v3(Sqlite& db,const std::string& domain,const std::vector<std::string>& queries){
 std::string bytes=domain;
 auto append=[&](const std::string& value){bytes+=std::to_string(value.size())+":"+value;};
 for(const auto& query:queries){
  append(query);
  Statement rows(db,query.c_str());
  while(rows.row()){
   append("row");
   for(int i=0;i<sqlite3_column_count(rows.stmt);++i){
    const auto type=sqlite3_column_type(rows.stmt,i);
    append(std::to_string(type));
    if(type==SQLITE_INTEGER)append(std::to_string(rows.number(i)));
    else if(type==SQLITE_FLOAT)append(std::to_string(std::bit_cast<std::uint64_t>(sqlite3_column_double(rows.stmt,i))));
    else if(type==SQLITE_TEXT)
     append(std::string(reinterpret_cast<const char*>(sqlite3_column_text(rows.stmt,i)),
                        static_cast<std::size_t>(sqlite3_column_bytes(rows.stmt,i))));
    else require(type==SQLITE_NULL,"unexpected witness digest column type");
   }
  }
 }
 return sha256(bytes);
}
std::vector<std::string> sidecar_digest_queries_v3(int version){
 std::vector<std::string> q{
  "PRAGMA user_version",
  "SELECT type,name,tbl_name,sql FROM sqlite_master ORDER BY type,name",
  "SELECT * FROM v3_family ORDER BY id",
  "SELECT case_id,family,params,seed,input_sha256 FROM v3_cases ORDER BY case_id",
  "SELECT * FROM v3_pairs ORDER BY pair_number",
  "SELECT * FROM v3_samples ORDER BY pair_number,side,ordinal"};
 if(version==19){
  q.emplace_back("SELECT * FROM v3_holdout_journal_origin ORDER BY id");
  q.emplace_back("SELECT * FROM v3_holdout_attempts ORDER BY attempt_id");
 }
 return q;
}
Json json_array_v3(){return Json(json_object_new_array());}
void json_push_v3(Json& array,const Json& item){json_object_array_add(array.p,json_object_get(item.p));}
// Rebuild the signed evidence purely from replayed raw run + sidecar state.
Json witness_evidence_v3(const fs::path& run,const Plan& plan,
 const std::vector<MatchedBoundaryObservationV3>& discovery,const std::string& fingerprint){
 const auto path=run/"confirmation"/"family.sqlite";
 require(fs::is_regular_file(path),"witness requires a frozen family sidecar");
 Sqlite ledger(path,true);
 ledger.exec("BEGIN");
 const auto version=family_sidecar_version(ledger);
 const auto f=read_family_sidecar(ledger,plan,discovery,fingerprint);
 HoldoutEffectsV3 effects;
 const auto count=replay_family_sidecar(ledger,plan,f,discovery,effects);
 std::optional<HoldoutJournalV3> journal;
 if(version==19)journal=audit_holdout_journal_v3(ledger,plan,f,discovery,count);
 const auto sidecar_digest=logical_digest_v3(ledger,"phaseshift.v3.family-sidecar.1",sidecar_digest_queries_v3(version));
 ledger.exec("COMMIT");
 require(count==f.seeds.size(),"witness requires a complete frozen holdout family");
 require(!journal||journal->started==0,"witness refuses an unresolved STARTED holdout attempt");
 auto report=family_result_v3(plan,f,effects,count,journal);
 require(report.b("complete"),"witness requires complete family outcome");

 Json evidence;
 evidence.put("protocol","phaseshift.v3.boundary_witness_evidence.1");
 Json source;
 source.put("source_config_sha256",sha256(plan.source));
 source.put("source_snapshot_sha256",fingerprint);
 source.put("committed_discovery_pairs",static_cast<std::int64_t>(discovery.size()));
 source.put("primary_axis",plan.axis);
 source.put("algorithm_a",plan.config.a);source.put("algorithm_b",plan.config.b);
 evidence.put("discovery",std::move(source));
 Json family;
 family.put("sidecar_schema",static_cast<std::int64_t>(version));
 family.put("family_sha256",f.digest);
 family.put("specification",f.canonical);
 family.put("frozen_source_revision",f.revision);
 family.put("sidecar_logical_sha256",sidecar_digest);
 evidence.put("family",std::move(family));
 Json attempts;
 if(journal){
  attempts.put("state",journal->abandoned?"RETRIED_AFTER_INTERRUPTION":"CLEAN");
  attempts.put("retries_observable",true);
  attempts.put("holdout_attempts",journal->attempts);
  attempts.put("abandoned_attempts",journal->abandoned);
  attempts.put("pairs_with_abandoned_attempts",journal->interrupted_pairs);
 }else{
  attempts.put("state","ABSENT_SCHEMA_16");
  attempts.put("retries_observable",false);
 }
 evidence.put("holdout_attempt_journal",std::move(attempts));
 auto edges=json_array_v3();
 const auto& labels=plan.config.axes.at(plan.axis);
 for(std::size_t i=0;i<f.members.size();++i){
  const auto& member=f.members[i];
  const auto& edge=discovery.at(member.discovery_pair);
  Json row;Json context;
  row.put("member_index",static_cast<std::int64_t>(i));
  row.put("discovery_pair",static_cast<std::int64_t>(member.discovery_pair));
  row.put("direction",static_cast<std::int64_t>(member.direction));
  row.put("left",labels.at(edge.left));row.put("right",labels.at(edge.right));
  for(const auto& [axis,label]:edge.context)context.put(axis.c_str(),label);
  row.put("context",std::move(context));
  json_push_v3(edges,row);
 }
 evidence.put("members",std::move(edges));
 evidence.put("family_report",std::move(report));
 Json limits;
 limits.put("confirmed",false);
 limits.put("assumptions_empirically_validated",false);
 limits.put("hardware_general",false);
 limits.put("scope","frozen fixed-edge family under the declared sign model on the recorded host; not a SOTA claim");
 evidence.put("claim_limits",std::move(limits));
 return evidence;
}
std::string option(int argc,char** argv,const std::string& name);
void witness_command_v3(const std::string& command,const fs::path& run,int argc,char** argv,
 const Plan& plan,const std::vector<MatchedBoundaryObservationV3>& discovery,const std::string& fingerprint){
 if(command=="witness-export"){
  const auto key_path=option(argc,argv,"--key"),out=option(argc,argv,"--out");
  require(!key_path.empty()&&!out.empty(),"witness-export needs --key and --out");
  const auto key=read_ed25519_key_v3(key_path,true);
  // Build and sign before reserving the output, so failure leaves nothing.
  auto evidence=witness_evidence_v3(run,plan,discovery,fingerprint);
  Json exporter;
  exporter.put("source_revision",Json::parse(environment()).s("source_revision"));
  exporter.put("executable_sha256",executable_digest_v3(false));
  exporter.put("worker_sha256",executable_digest_v3(true));
  Json witness;
  witness.put("schema","phaseshift.v3.signed_boundary_witness.1");
  witness.put("signature_algorithm","ed25519");
  witness.put("public_key_sha256",sha256(public_key_der_v3(key.get())));
  witness.put("exporter",std::move(exporter));
  witness.put("evidence",std::move(evidence));
  const auto bytes=witness.str()+"\n";
  const auto signature=ed25519_sign_v3(key.get(),bytes);
  require(ed25519_verify_v3(key.get(),bytes,signature),"self-verification of new witness failed");
  require(!fs::exists(out)&&fs::create_directories(out),"witness directory exists; immutable export");
  write_file((fs::path(out)/"witness.json").string(),bytes);
  write_file((fs::path(out)/"witness.sig").string(),signature);
  write_file((fs::path(out)/"public_key.pem").string(),public_key_pem_v3(key.get()));
  Json summary;
  summary.put("schema","phaseshift.v3.signed_boundary_witness.1");
  summary.put("witness_sha256",sha256(bytes));
  summary.put("public_key_sha256",witness.s("public_key_sha256"));
  std::cout<<summary.str()<<"\n";
  return;
 }
 const auto dir=option(argc,argv,"--witness"),trusted_path=option(argc,argv,"--public-key");
 require(!dir.empty()&&!trusted_path.empty(),"witness-verify needs --witness and a separately trusted --public-key");
 const auto trusted=read_ed25519_key_v3(trusted_path,false);
 const auto trusted_der=public_key_der_v3(trusted.get());
 const auto bytes=read_file((fs::path(dir)/"witness.json").string());
 const auto signature=read_file((fs::path(dir)/"witness.sig").string());
 require(ed25519_verify_v3(trusted.get(),bytes,signature),"witness signature invalid for trusted key");
 const auto bundled=read_ed25519_key_v3((fs::path(dir)/"public_key.pem").string(),false);
 require(public_key_der_v3(bundled.get())==trusted_der,"bundled public key differs from trusted key");
 const auto witness=Json::parse(bytes);
 require(witness.s("schema")=="phaseshift.v3.signed_boundary_witness.1"&&
         witness.s("signature_algorithm")=="ed25519"&&
         witness.s("public_key_sha256")==sha256(trusted_der),"unsupported or mismatched witness header");
 json_object* signed_evidence=witness.get("evidence");
 require(signed_evidence!=nullptr,"witness lacks evidence");
 // Independent full replay; any evidence change since signing fails closed.
 const auto rebuilt=Json::parse(witness_evidence_v3(run,plan,discovery,fingerprint).str());
 require(json_object_equal(signed_evidence,rebuilt.p)==1,"replayed evidence differs from signed witness");
 json_object* exporter=witness.get("exporter");
 json_object* exe=exporter?json_object_object_get(exporter,"executable_sha256"):nullptr;
 Json result;
 result.put("schema","phaseshift.v3.signed_boundary_witness_verification.1");
 result.put("verified",true);
 result.put("witness_sha256",sha256(bytes));
 result.put("exporter_executable_matches_verifier",
            exe!=nullptr&&json_object_get_string(exe)==executable_digest_v3(false));
 std::cout<<result.str()<<"\n";
}

// ---------------------------------------------------------------------------
// V3-09 scope-preserving boundary witness minimization on fresh seeds.
// The scope (every declared axis: primary edge labels and context) is frozen;
// only non-scope structural parameters of the shared base case may shrink.
// Every candidate is tested on its own fresh, disjoint seed block with the
// frozen family's exact sign-tail predicate, and requires oracle-correct
// output on both endpoints. Candidate acceptance is adaptive and uncorrected;
// the final witness gets a separate fresh-block reproduction. The driver is a
// pure function of a measurement callback, so verification replays it over the
// signed raw observations and demands identical evidence.
constexpr std::uint64_t kMinimizeSeedBaseV3=1ULL<<60; // [2^60,2^61): disjoint from discovery (<2^31) and holdout ([2^62,2^63))
constexpr std::size_t kVerifierReproduceBlockV3=8192;
constexpr std::size_t kMinimizeBudgetCapV3=32;
struct MinPairV3 {
 std::uint64_t seed=0;
 bool left_first=false;
 std::string left_id,right_id;
 std::vector<Observation> l,r;
 std::optional<double> contrast;
};
struct MinEvalV3 {
 std::string role,action;
 std::size_t block=0;
 Case base;
 std::string left_params,right_params;
 std::vector<MinPairV3> pairs;
 std::size_t valid=0,positive=0;
 double p=1.;
 bool holds=false;
};
struct MinContextV3 {
 const Plan& plan;
 const FamilySpecV3& f;
 const MatchedBoundaryObservationV3& edge;
 std::size_t member=0;
 int direction=0;
 std::set<std::string> scope;
};
using MinMeasureV3=std::function<HoldoutMeasuredV3(std::size_t block,std::size_t pair,const HoldoutProposalV3&)>;
std::vector<std::uint64_t> minimization_seeds_v3(const FamilySpecV3& f,std::size_t member,std::size_t block,std::size_t count){
 const auto h=sha256(f.digest+"/phaseshift.v3.boundary_minimization.1/"+std::to_string(member)+"/"+std::to_string(block));
 std::uint64_t state=std::stoull(h.substr(0,16),nullptr,16);
 std::set<std::uint64_t> seen;std::vector<std::uint64_t> out;
 while(out.size()<count){
  const auto seed=kMinimizeSeedBaseV3|(splitmix64(state)&(kMinimizeSeedBaseV3-1));
  if(seen.insert(seed).second)out.push_back(seed);
 }
 return out;
}
Case witness_endpoint_v3(const MinContextV3& ctx,const Case& base,bool right,std::uint64_t seed){
 auto c=base;c.seed=seed;
 for(const auto& [axis,label]:ctx.edge.context)apply_axis(c,axis,label);
 apply_axis(c,ctx.plan.axis,ctx.plan.config.axes.at(ctx.plan.axis).at(right?ctx.edge.right:ctx.edge.left));
 return make_case(c,ctx.plan.config.max_bytes);
}
std::string scope_field_v3(const Case& c,const std::string& name){
 if(name=="x")return std::to_string(c.x);
 if(name=="n")return std::to_string(c.n);
 if(name=="k")return std::to_string(c.k);
 if(name=="vertices")return std::to_string(c.vertices);
 if(name=="edges")return std::to_string(c.edges);
 if(name=="threshold")return std::to_string(c.threshold);
 if(name=="value_range")return std::to_string(c.value_range);
 if(name=="unique_count")return std::to_string(c.unique_count);
 if(name=="distribution")return c.distribution;
 if(name=="weight_class")return c.weight_class;
 if(name=="negative")return c.negative?"true":"false";
 if(name=="directed")return c.directed?"true":"false";
 throw Error("v3 pairs: unsupported scope axis for minimization: "+name);
}
// Deterministic, ordered non-scope shrink proposals. Any proposal that would
// change a scope field (directly or through a coupled field) is dropped.
std::vector<std::pair<Case,std::string>> boundary_shrinks_v3(const Case& b,const std::set<std::string>& scope){
 std::vector<std::pair<Case,std::string>> out;
 auto edit=[&](const char* why,auto fn){
  auto c=b;fn(c);
  for(const auto& name:scope)if(scope_field_v3(c,name)!=scope_field_v3(b,name))return;
  out.emplace_back(std::move(c),why);
 };
 if(b.family=="calibration"){
  for(const auto t:{b.threshold/2,b.threshold-1})if(t>=1&&t<b.threshold)
   edit("shrink_threshold",[t](Case& c){c.threshold=t;});
 }else if(b.family=="integer_array"||b.family=="topk"){
  for(const auto n:{b.n/2,b.n-1})if(n>=1&&n<b.n)
   edit("shrink_length",[n](Case& c){c.n=n;c.k=std::min(c.k,n);c.unique_count=std::min(c.unique_count,n);});
  for(const auto v:{b.value_range/2,b.value_range-1})if(v>=1&&v<b.value_range)
   edit("shrink_value_range",[v](Case& c){c.value_range=v;c.unique_count=std::min(c.unique_count,2*v+1);});
  if(b.run_length>1)edit("shrink_run_length",[](Case& c){c.run_length=1;});
  if(b.unique_count>1)edit("shrink_unique_values",[](Case& c){c.unique_count=1;});
  if(b.family=="topk")for(const auto k:{b.k/2,b.k-1})if(k>=1&&k<b.k)
   edit("shrink_topk_output",[k](Case& c){c.k=k;});
 }else if(b.family=="graph"){
  for(const auto v:{b.vertices/2,b.vertices-1})if(v>=1&&v<b.vertices&&b.source<v)
   edit("shrink_vertices",[v](Case& c){
    c.vertices=v;c.community_count=std::min(c.community_count,v);
    const auto cap=c.directed?v*(v-1):v*(v-1)/2;
    c.edges=std::min(c.edges,cap);
    if(c.connectivity=="strong")c.edges=std::max(c.edges,v);
    if(c.connectivity=="weak")c.edges=std::max(c.edges,v-1);
    c.density_ppm=-1;
   });
  for(const auto e:{b.edges/2,b.edges-1})if(e>=0&&e<b.edges)
   edit("shrink_edges",[e](Case& c){c.edges=e;c.density_ppm=-1;});
  for(const auto w:{b.max_weight/2,b.max_weight-1})if(w>=0&&w<b.max_weight)
   edit("shrink_weight_range",[w](Case& c){c.max_weight=w;});
  if(b.community_count>1)edit("shrink_communities",[](Case& c){c.community_count=1;});
 }
 return out;
}
std::uint64_t witness_complexity_v3(const Case& c){
 std::uint64_t total=0;
 for(const auto v:{c.n,c.k,c.value_range,c.run_length,c.unique_count,c.vertices,c.edges,
                   c.max_weight,c.community_count,c.threshold})
  if(v>0)total+=static_cast<std::uint64_t>(v);
 return total;
}
std::optional<std::pair<std::size_t,std::uint64_t>> witness_size_v3(const MinContextV3& ctx,const Case& base){
 try{
  return std::pair{witness_endpoint_v3(ctx,base,false,1).bytes.size()+witness_endpoint_v3(ctx,base,true,1).bytes.size(),
                   witness_complexity_v3(base)};
 }catch(const Error&){return std::nullopt;} // generator-invalid proposal: deterministically skipped
}
MinEvalV3 evaluate_boundary_v3(const MinContextV3& ctx,const Case& base,const std::string& role,
 const std::string& action,std::size_t block,const MinMeasureV3& measure,std::set<std::uint64_t>& used){
 MinEvalV3 e;e.role=role;e.action=action;e.block=block;e.base=base;
 e.left_params=case_params(witness_endpoint_v3(ctx,base,false,1));
 e.right_params=case_params(witness_endpoint_v3(ctx,base,true,1));
 const auto minimum=std::log(ctx.plan.config.min_ratio);
 const auto seeds=minimization_seeds_v3(ctx.f,ctx.member,block,ctx.f.n);
 for(std::size_t j=0;j<seeds.size();++j){
  require(used.insert(seeds[j]).second,"minimization seed reused across blocks");
  HoldoutProposalV3 p{seeds[j],((seeds[j]^j)&1ULL)==0,witness_endpoint_v3(ctx,base,false,seeds[j]),
                      witness_endpoint_v3(ctx,base,true,seeds[j])};
  const auto m=measure(block,j,p);
  MinPairV3 pair;pair.seed=p.seed;pair.left_first=p.left_first;pair.left_id=p.a.id;pair.right_id=p.b.id;
  pair.l=m.l;pair.r=m.r;
  const auto lm=per_side_metric(pair.l,ctx.plan.config.a,ctx.plan.config.b);
  const auto rm=per_side_metric(pair.r,ctx.plan.config.a,ctx.plan.config.b);
  if(lm&&rm){
   pair.contrast=*rm-*lm;
   require(std::isfinite(*pair.contrast),"nonfinite minimization contrast");
   ++e.valid;
   if(static_cast<double>(ctx.direction)*(*pair.contrast)>minimum)++e.positive;
  }
  e.pairs.push_back(std::move(pair));
 }
 if(e.valid==seeds.size())e.p=exact_upper_sign_tail_v3(seeds.size(),e.positive);
 e.holds=e.valid==seeds.size()&&e.p<=ctx.f.alpha;
 return e;
}
struct MinOutcomeV3 {
 std::vector<MinEvalV3> evals;
 std::optional<std::size_t> final_eval;
 bool reduced=false,reduced_failed_reproduction=false;
 std::string state;
};
MinOutcomeV3 minimize_boundary_v3(const MinContextV3& ctx,std::size_t budget,const MinMeasureV3& measure){
 require(budget<=kMinimizeBudgetCapV3,"minimization budget above cap");
 MinOutcomeV3 out;
 std::set<std::uint64_t> used;
 const auto original=ctx.plan.config.base;
 auto current=original;
 auto current_size=witness_size_v3(ctx,current);
 require(current_size.has_value(),"original witness endpoints do not regenerate");
 std::set<std::string> visited{case_params(current)};
 std::size_t shrinks=0;
 bool improved=true;
 while(improved&&shrinks<budget){
  improved=false;
  for(const auto& [candidate,action]:boundary_shrinks_v3(current,ctx.scope)){
   if(!visited.insert(case_params(candidate)).second)continue;
   const auto size=witness_size_v3(ctx,candidate);
   if(!size||*size>=*current_size)continue;
   if(shrinks>=budget)break;
   out.evals.push_back(evaluate_boundary_v3(ctx,candidate,"SHRINK",action,out.evals.size(),measure,used));
   ++shrinks;
   if(out.evals.back().holds){current=candidate;current_size=size;out.reduced=true;improved=true;break;}
  }
 }
 std::vector<Case> finals;
 if(out.reduced)finals.push_back(current);
 finals.push_back(original);
 for(std::size_t i=0;i<finals.size();++i){
  out.evals.push_back(evaluate_boundary_v3(ctx,finals[i],"REPRODUCE",i==0&&out.reduced?"reproduce_reduced":"reproduce_original",
                                            out.evals.size(),measure,used));
  if(out.evals.back().holds){out.final_eval=out.evals.size()-1;break;}
  if(i==0&&out.reduced)out.reduced_failed_reproduction=true;
 }
 out.state=!out.final_eval?"NOT_REPRODUCED":
           (out.reduced&&!out.reduced_failed_reproduction)?"REDUCED_REPRODUCED":"ORIGINAL_REPRODUCED";
 return out;
}
Json observations_json_v3(const std::vector<Observation>& rows){
 auto out=json_array_v3();
 for(const auto& r:rows){
  Json o;o.put("algorithm",r.algorithm);o.put("status",r.status);o.put("ns",r.ns);o.put("batch",r.batch);
  o.put("work",r.work);o.put("cpu_ns",r.cpu_ns);
  json_push_v3(out,o);
 }
 return out;
}
std::vector<Observation> observations_from_json_v3(json_object* array){
 require(array&&json_object_is_type(array,json_type_array),"signed observations must be an array");
 std::vector<Observation> rows;
 for(std::size_t i=0;i<json_object_array_length(array);++i){
  Json o(json_object_get(json_object_array_get_idx(array,i)));
  Observation r;r.algorithm=o.s("algorithm");r.status=o.s("status");r.ns=o.i("ns");r.batch=o.i("batch");
  r.work=o.i("work");r.cpu_ns=o.i("cpu_ns");
  rows.push_back(std::move(r));
 }
 return rows;
}
Json minimization_evidence_v3(const MinContextV3& ctx,const Json& parent,std::size_t budget,const MinOutcomeV3& out){
 Json e;
 e.put("protocol","phaseshift.v3.boundary_minimization.1");
 e.put("parent_evidence_sha256",sha256(parent.str()));
 e.put("member",Json(json_object_get(json_object_array_get_idx(parent.get("members"),ctx.member))));
 Json predicate;
 predicate.put("seed_pairs_per_evaluation",static_cast<std::int64_t>(ctx.f.n));
 predicate.put("alpha",ctx.f.alpha);
 predicate.put("minimum_log_effect",std::log(ctx.plan.config.min_ratio));
 predicate.put("test","one_sided_exact_binomial_sign_tail");
 predicate.put("requires_all_endpoints_oracle_ok",true);
 predicate.put("seed_domain","[2^60,2^61), sha256(family_sha256/protocol/member/block)-keyed splitmix64");
 e.put("predicate",std::move(predicate));
 e.put("shrink_budget",static_cast<std::int64_t>(budget));
 auto scope=json_array_v3();
 for(const auto& name:ctx.scope){Json s(json_object_new_string(name.c_str()));json_push_v3(scope,s);}
 e.put("scope_axes",std::move(scope));
 auto evals=json_array_v3();
 for(const auto& ev:out.evals){
  Json row;
  row.put("block",static_cast<std::int64_t>(ev.block));row.put("role",ev.role);row.put("action",ev.action);
  row.put("left_params",ev.left_params);row.put("right_params",ev.right_params);
  row.put("valid_pairs",static_cast<std::int64_t>(ev.valid));row.put("positive_pairs",static_cast<std::int64_t>(ev.positive));
  row.put("p",ev.p);row.put("holds",ev.holds);
  auto pairs=json_array_v3();
  for(const auto& p:ev.pairs){
   Json pr;
   pr.put("seed",static_cast<std::int64_t>(p.seed));pr.put("left_first",p.left_first);
   pr.put("left_case_id",p.left_id);pr.put("right_case_id",p.right_id);
   if(p.contrast)pr.put("log_contrast",*p.contrast);else pr.put("log_contrast",Json(nullptr));
   pr.put("left",observations_json_v3(p.l));pr.put("right",observations_json_v3(p.r));
   json_push_v3(pairs,pr);
  }
  row.put("pairs",std::move(pairs));
  json_push_v3(evals,row);
 }
 e.put("evaluations",std::move(evals));
 Json outcome;
 outcome.put("state",out.state);
 outcome.put("reduced",out.reduced);
 outcome.put("reduced_candidate_failed_reproduction",out.reduced_failed_reproduction);
 if(out.final_eval){
  const auto& fin=out.evals.at(*out.final_eval);
  outcome.put("final_evaluation",static_cast<std::int64_t>(*out.final_eval));
  outcome.put("final_left_params",fin.left_params);outcome.put("final_right_params",fin.right_params);
 }else outcome.put("final_evaluation",Json(nullptr));
 e.put("outcome",std::move(outcome));
 Json limits;
 limits.put("confirmed",false);limits.put("hardware_general",false);
 limits.put("selection","shrink acceptance tests are adaptive and uncorrected; only the final fresh-block reproduction tests the final witness, and a failed reduced reproduction is followed by an original reproduction");
 e.put("claim_limits",std::move(limits));
 return e;
}
MinContextV3 minimization_context_v3(const Plan& plan,const FamilySpecV3& f,
 const std::vector<MatchedBoundaryObservationV3>& discovery,const Json& parent,std::size_t member){
 require(member<f.members.size(),"minimization member outside frozen family");
 json_object* rows=json_object_object_get(parent.get("family_report"),"members");
 require(rows&&member<json_object_array_length(rows),"parent family report lacks member");
 Json row(json_object_get(json_object_array_get_idx(rows,member)));
 require(row.s("state")=="REJECTED_UNDER_DECLARED_MODEL","minimization requires a holdout-rejected family member");
 MinContextV3 ctx{plan,f,discovery.at(f.members[member].discovery_pair),member,f.members[member].direction,{}};
 for(const auto& [axis,values]:plan.config.axes){(void)values;ctx.scope.insert(axis);}
 return ctx;
}
FamilySpecV3 frozen_family_for_witness_v3(const fs::path& run,const Plan& plan,
 const std::vector<MatchedBoundaryObservationV3>& discovery,const std::string& fingerprint){
 Sqlite ledger(run/"confirmation"/"family.sqlite",true);
 ledger.exec("BEGIN");
 auto f=read_family_sidecar(ledger,plan,discovery,fingerprint);
 ledger.exec("COMMIT");
 return f;
}
void minimize_command_v3(const std::string& command,const fs::path& run,int argc,char** argv,
 const Plan& plan,const std::vector<MatchedBoundaryObservationV3>& discovery,const std::string& fingerprint){
 const auto parent=witness_evidence_v3(run,plan,discovery,fingerprint); // full replay, complete family
 const auto f=frozen_family_for_witness_v3(run,plan,discovery,fingerprint);
 const auto deadline_s=std::chrono::seconds(plan.config.total_wall_s);
 auto live=[&](std::size_t,std::size_t,const HoldoutProposalV3& p){
  return measure_holdout_pair_v3(plan,p,std::chrono::steady_clock::now()+deadline_s);
 };
 if(command=="witness-minimize"){
  const auto key_path=option(argc,argv,"--key"),out=option(argc,argv,"--out"),member_text=option(argc,argv,"--member");
  require(!key_path.empty()&&!out.empty()&&!member_text.empty(),"witness-minimize needs --member, --key and --out");
  const auto member=integer(member_text);
  require(member>=0,"invalid --member");
  const auto budget_text=option(argc,argv,"--budget");
  const auto budget=budget_text.empty()?std::int64_t{8}:integer(budget_text);
  require(budget>=0&&budget<=static_cast<std::int64_t>(kMinimizeBudgetCapV3),"--budget expected 0..32");
  const auto key=read_ed25519_key_v3(key_path,true);
  const auto revision=Json::parse(environment()).s("source_revision");
  require(revision==f.revision,"minimizing executable source revision differs from frozen family");
  const auto ctx=minimization_context_v3(plan,f,discovery,parent,static_cast<std::size_t>(member));
  // Reserve the output first: an interrupted run leaves a visible, unusable
  // directory rather than silently permitting a re-rolled minimization.
  require(!fs::exists(out)&&fs::create_directories(out),"minimization directory exists; immutable export");
  write_file((fs::path(out)/"STARTED").string(),"phaseshift.v3.boundary_minimization.1\n");
  const auto outcome=minimize_boundary_v3(ctx,static_cast<std::size_t>(budget),live);
  Json exporter;
  exporter.put("source_revision",revision);
  exporter.put("executable_sha256",executable_digest_v3(false));
  exporter.put("worker_sha256",executable_digest_v3(true));
  Json doc;
  doc.put("schema","phaseshift.v3.minimized_boundary_witness.1");
  doc.put("signature_algorithm","ed25519");
  doc.put("public_key_sha256",sha256(public_key_der_v3(key.get())));
  doc.put("exporter",std::move(exporter));
  doc.put("evidence",minimization_evidence_v3(ctx,parent,static_cast<std::size_t>(budget),outcome));
  const auto bytes=doc.str()+"\n";
  const auto signature=ed25519_sign_v3(key.get(),bytes);
  write_file((fs::path(out)/"minimized_witness.json").string(),bytes);
  write_file((fs::path(out)/"minimized_witness.sig").string(),signature);
  write_file((fs::path(out)/"public_key.pem").string(),public_key_pem_v3(key.get()));
  fs::remove(fs::path(out)/"STARTED");
  Json summary;
  summary.put("schema","phaseshift.v3.minimized_boundary_witness.1");
  summary.put("state",outcome.state);summary.put("reduced",outcome.reduced);
  summary.put("evaluations",static_cast<std::int64_t>(outcome.evals.size()));
  summary.put("witness_sha256",sha256(bytes));
  std::cout<<summary.str()<<"\n";
  return;
 }
 // witness-verify-minimized
 const auto dir=option(argc,argv,"--witness"),trusted_path=option(argc,argv,"--public-key");
 require(!dir.empty()&&!trusted_path.empty(),"witness-verify-minimized needs --witness and a separately trusted --public-key");
 const auto reproduce_text=option(argc,argv,"--reproduce");
 require(reproduce_text.empty()||reproduce_text=="true"||reproduce_text=="false","--reproduce expects true or false");
 require(!fs::exists(fs::path(dir)/"STARTED"),"minimization was interrupted; record is incomplete");
 const auto trusted=read_ed25519_key_v3(trusted_path,false);
 const auto trusted_der=public_key_der_v3(trusted.get());
 const auto bytes=read_file((fs::path(dir)/"minimized_witness.json").string());
 const auto signature=read_file((fs::path(dir)/"minimized_witness.sig").string());
 require(ed25519_verify_v3(trusted.get(),bytes,signature),"minimized witness signature invalid for trusted key");
 const auto bundled=read_ed25519_key_v3((fs::path(dir)/"public_key.pem").string(),false);
 require(public_key_der_v3(bundled.get())==trusted_der,"bundled public key differs from trusted key");
 const auto doc=Json::parse(bytes);
 require(doc.s("schema")=="phaseshift.v3.minimized_boundary_witness.1"&&doc.s("signature_algorithm")=="ed25519"&&
         doc.s("public_key_sha256")==sha256(trusted_der),"unsupported or mismatched minimized witness header");
 json_object* signed_evidence=doc.get("evidence");
 require(signed_evidence!=nullptr,"minimized witness lacks evidence");
 Json evidence(json_object_get(signed_evidence));
 json_object* member_row=evidence.get("member");
 require(member_row!=nullptr,"minimized witness lacks member");
 const auto member=Json(json_object_get(member_row)).i("member_index",-1);
 const auto budget=evidence.i("shrink_budget",-1);
 require(member>=0&&budget>=0&&budget<=static_cast<std::int64_t>(kMinimizeBudgetCapV3),"invalid signed member/budget");
 json_object* evals=evidence.get("evaluations");
 require(evals&&json_object_is_type(evals,json_type_array),"signed evaluations missing");
 // Replay the deterministic driver over the signed raw observations; every
 // regenerated seed and case identity must match what was signed.
 auto recorded=[&](std::size_t block,std::size_t pair,const HoldoutProposalV3& p){
  require(block<json_object_array_length(evals),"driver requested an unsigned evaluation");
  json_object* pairs=json_object_object_get(json_object_array_get_idx(evals,block),"pairs");
  require(pairs&&json_object_is_type(pairs,json_type_array)&&pair<json_object_array_length(pairs),"signed evaluation lacks pair");
  Json row(json_object_get(json_object_array_get_idx(pairs,pair)));
  require(row.i("seed")==static_cast<std::int64_t>(p.seed)&&row.b("left_first")==p.left_first&&
          row.s("left_case_id")==p.a.id&&row.s("right_case_id")==p.b.id,"signed pair identity differs from deterministic replay");
  HoldoutMeasuredV3 m;
  m.l=observations_from_json_v3(json_object_object_get(row.p,"left"));
  m.r=observations_from_json_v3(json_object_object_get(row.p,"right"));
  return m;
 };
 const auto ctx=minimization_context_v3(plan,f,discovery,parent,static_cast<std::size_t>(member));
 const auto outcome=minimize_boundary_v3(ctx,static_cast<std::size_t>(budget),recorded);
 const auto rebuilt=Json::parse(minimization_evidence_v3(ctx,parent,static_cast<std::size_t>(budget),outcome).str());
 require(json_object_equal(signed_evidence,rebuilt.p)==1,"replayed minimization differs from signed witness");
 Json result;
 result.put("schema","phaseshift.v3.minimized_boundary_witness_verification.1");
 result.put("verified",true);result.put("state",outcome.state);
 result.put("witness_sha256",sha256(bytes));
 if(reproduce_text=="true"){
  // Independent re-measurement of the final witness on a verifier-only block.
  require(outcome.final_eval.has_value(),"no reproduced witness to re-measure");
  std::set<std::uint64_t> used;
  for(const auto& ev:outcome.evals)for(const auto& p:ev.pairs)used.insert(p.seed);
  const auto again=evaluate_boundary_v3(ctx,outcome.evals.at(*outcome.final_eval).base,"VERIFY_REPRODUCE",
                                        "verifier_reproduce",kVerifierReproduceBlockV3,live,used);
  Json rep;
  rep.put("block",static_cast<std::int64_t>(kVerifierReproduceBlockV3));
  rep.put("valid_pairs",static_cast<std::int64_t>(again.valid));
  rep.put("positive_pairs",static_cast<std::int64_t>(again.positive));
  rep.put("p",again.p);rep.put("holds",again.holds);
  result.put("independent_reproduction",std::move(rep));
 }
 std::cout<<result.str()<<"\n";
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
  command=="adaptive"||command=="conditional"||command=="conditional-rules"||(command=="conditional-tree"||(command=="acquisition-v3"||command=="acquisition-v3-coverage")||command=="accounted-v3")?
  std::set<std::string>{"--config","--out","--pairs","--pause-after"}:
  (command=="family-freeze"||command=="family-freeze-journaled")?std::set<std::string>{"--run","--manifest"}:
  command=="witness-export"?std::set<std::string>{"--run","--key","--out"}:
  command=="witness-verify"?std::set<std::string>{"--run","--witness","--public-key"}:
  command=="witness-minimize"?std::set<std::string>{"--run","--member","--key","--out","--budget"}:
  command=="witness-verify-minimized"?std::set<std::string>{"--run","--witness","--public-key","--reproduce"}:
  (command=="verify"||command=="taxonomy"||command=="family-report"||command=="accounting")?std::set<std::string>{"--run"}:
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
  // Orphaned worker descendants are reparented here and contained per measurement.
  require(::prctl(PR_SET_CHILD_SUBREAPER,1,0,0,0)==0,"cannot become child subreaper");
  require(argc>=2,"expected run, resume or verify");
  const std::string command=argv[1];
  require(command=="run"||command=="adaptive"||command=="conditional"||command=="conditional-rules"||(command=="conditional-tree"||(command=="acquisition-v3"||command=="acquisition-v3-coverage")||command=="accounted-v3")||command=="resume"||command=="verify"||command=="taxonomy"||command=="family-freeze"||command=="family-freeze-journaled"||command=="family-confirm"||command=="family-report"||command=="accounting"||command=="witness-export"||command=="witness-verify"||command=="witness-minimize"||command=="witness-verify-minimized","unknown pilot command");
  syntax(argc,argv,command);
  const auto pause=numeric_option(argc,argv,"--pause-after",0);
  if(command=="run"||command=="adaptive"||command=="conditional"||command=="conditional-rules"||(command=="conditional-tree"||(command=="acquisition-v3"||command=="acquisition-v3-coverage")||command=="accounted-v3")){
   const auto config=option(argc,argv,"--config"),out=option(argc,argv,"--out");
   require(!config.empty()&&!out.empty(),"run needs config and output path");
   Plan plan;
   if(command=="adaptive")
    plan=make_adaptive_plan(read_file(config),numeric_option(argc,argv,"--pairs",3));
   else if(command=="conditional"||command=="conditional-rules"||(command=="conditional-tree"||(command=="acquisition-v3"||command=="acquisition-v3-coverage")||command=="accounted-v3")){
    const auto frozen=read_file(config);
    const auto rules=(command=="conditional-tree"||(command=="acquisition-v3"||command=="acquisition-v3-coverage")||command=="accounted-v3")?
      !config_from_json(Json::parse(frozen)).conditional_rules.empty():
      command=="conditional-rules";
    plan=make_conditional_plan(frozen,numeric_option(argc,argv,"--pairs",3),
                               rules,(command=="conditional-tree"||(command=="acquisition-v3"||command=="acquisition-v3-coverage")||command=="accounted-v3"),(command=="acquisition-v3"||command=="acquisition-v3-coverage")||command=="accounted-v3",command=="acquisition-v3-coverage");
    plan.accounted=command=="accounted-v3";
   }
   else{
    const auto left=option(argc,argv,"--left"),right=option(argc,argv,"--right");
    require(!left.empty()&&!right.empty(),"run needs two frozen endpoint labels");
    plan=make_plan(read_file(config),left,right,numeric_option(argc,argv,"--pairs",3));
   }
   require(!fs::exists(out),"run directory exists; cannot overwrite immutable evidence");
   require(fs::create_directories(out),"run directory already reserved");
   Sqlite db(fs::path(out)/"run.sqlite");
   std::optional<AccountedRunLock> lock;
   if(plan.accounted){lock.emplace(fs::path(out)/"run.sqlite");db.exec("PRAGMA synchronous=FULL");}
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
      plan.config.seed,static_cast<std::size_t>(plan.pairs),plan.config.min_ratio,
      64,plan.region_tree,plan.acquisition,plan.coverage_first);
   }
   execute_pairs(db,plan,planner,adaptive?&*adaptive:nullptr,
                 conditional?&*conditional:nullptr,0,static_cast<std::size_t>(pause));
   return 0;
  }
  const auto run=option(argc,argv,"--run");
  require(!run.empty(),"resume/verify need --run");
  require(fs::is_regular_file(fs::path(run)/"run.sqlite"),"missing authoritative run.sqlite");
  const bool family=command=="family-freeze"||command=="family-freeze-journaled"||command=="family-confirm"||command=="family-report";
  const bool minimizing=command=="witness-minimize"||command=="witness-verify-minimized";
  const bool witness=command=="witness-export"||command=="witness-verify"||minimizing;
  const bool needs_taxonomy=command=="taxonomy"||family||witness;
  const bool readonly=needs_taxonomy||command=="accounting"||command=="verify";
  Sqlite db(fs::path(run)/"run.sqlite",readonly);
  if(readonly)db.exec("BEGIN"); // one read-only snapshot for replay and report
  const auto plan=read_plan(db);
  if(needs_taxonomy)require(plan.conditional,"taxonomy/family requires conditional evidence");
  if(command=="accounting")require(plan.accounted,"accounting requires schema 17");
  std::optional<AccountedRunLock> lock;
  if(plan.accounted&&!readonly){
   lock.emplace(fs::path(run)/"run.sqlite");db.exec("PRAGMA synchronous=FULL");check_accounting_origin(db,true);
  }
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
     plan.config.seed,static_cast<std::size_t>(plan.pairs),plan.config.min_ratio,
     64,plan.region_tree,plan.acquisition,plan.coverage_first);
  }
  std::vector<MatchedBoundaryObservationV3> taxonomy;
  const auto completed=replay(db,plan,planner,adaptive?&*adaptive:nullptr,
                              conditional?&*conditional:nullptr,needs_taxonomy?&taxonomy:nullptr);
  if(plan.accounted){
   (void)audit_accounting_v3(db,completed,static_cast<std::size_t>(plan.pairs));
   if(!readonly)db.exec("UPDATE v3_attempts SET state='ABANDONED' WHERE state='STARTED'");
  }
  if(command=="accounting"){
   report_accounting_v3(db,completed,static_cast<std::size_t>(plan.pairs));db.exec("COMMIT");return 0;
  }
  if(family){
   family_command_v3(command,fs::path(run),option(argc,argv,"--manifest"),plan,taxonomy,
                     source_digest_v3(db,plan),static_cast<std::size_t>(pause));
   db.exec("COMMIT");return 0;
  }
  if(witness){
   if(minimizing)minimize_command_v3(command,fs::path(run),argc,argv,plan,taxonomy,source_digest_v3(db,plan));
   else witness_command_v3(command,fs::path(run),argc,argv,plan,taxonomy,source_digest_v3(db,plan));
   db.exec("COMMIT");return 0;
  }
  if(command=="taxonomy"){
   taxonomy_json(plan,taxonomy,completed);db.exec("COMMIT");return 0;
  }
  if(command=="verify"){
   db.exec("COMMIT");std::cout<<"v3 replay verified "<<completed<<" committed matched pairs\n";return 0;
  }
  execute_pairs(db,plan,planner,adaptive?&*adaptive:nullptr,
                conditional?&*conditional:nullptr,completed,static_cast<std::size_t>(pause));
  return 0;
 }catch(const std::exception& e){std::cerr<<"PHASESHIFT v3 error: "<<e.what()<<"\n";return 2;}
}
