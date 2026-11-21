#include "phaseshift/campaign_v2.hpp"
#include "phaseshift/statistics.hpp"
#include <sqlite3.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>
namespace ps {
namespace {
struct Sql {
 sqlite3* db;sqlite3_stmt* stmt=nullptr;
 Sql(sqlite3* d,const char* query):db(d){
  if(sqlite3_prepare_v2(db,query,-1,&stmt,nullptr)!=SQLITE_OK)
   throw Error(std::string("campaign SQL prepare: ")+sqlite3_errmsg(db));
 }
 ~Sql(){sqlite3_finalize(stmt);}
 void bind(int index,const std::string& x){
  if(sqlite3_bind_text(stmt,index,x.c_str(),-1,SQLITE_TRANSIENT)!=SQLITE_OK)
   throw Error("campaign bind text failed");
 }
 void bind(int index,std::int64_t x){
  if(sqlite3_bind_int64(stmt,index,x)!=SQLITE_OK)throw Error("campaign bind integer failed");
 }
 void bind(int index,double x){
  if(sqlite3_bind_double(stmt,index,x)!=SQLITE_OK)throw Error("campaign bind real failed");
 }
 int next(){
  const auto rc=sqlite3_step(stmt);
  if(rc!=SQLITE_ROW&&rc!=SQLITE_DONE)
   throw Error(std::string("campaign SQL step: ")+sqlite3_errmsg(db));
  return rc;
 }
 std::string str(int index) const{
  const auto* p=sqlite3_column_text(stmt,index);
  return p?reinterpret_cast<const char*>(p):"";
 }
 std::int64_t integer(int index) const{return sqlite3_column_int64(stmt,index);}
 double real(int index) const{return sqlite3_column_double(stmt,index);}
};
std::int64_t count(sqlite3* db,const char* query){
 Sql s(db,query);if(s.next()!=SQLITE_ROW)throw Error("campaign SQL count unavailable");
 return s.integer(0);
}
void require_v2(Store& s){
 const auto config=Json::parse(s.config());
 if(config.s("policy_version")!="v2")throw Error("v1 cannot use v2 research campaigns");
}
void upgrade_campaign_v2(Store& s){
 require_v2(s);
 s.upgrade_v2_schema();
 const auto current=count(s.raw(),"PRAGMA user_version");
 if(current==9)return;
 if(current!=8)throw Error("v2 campaign requires SQLite schema v8");
 // Migration is atomic; no old row is rewritten or reinterpreted.
 s.exec(R"SQL(
 BEGIN IMMEDIATE;
 CREATE TABLE campaigns_v2(
  campaign_id TEXT PRIMARY KEY,manifest TEXT NOT NULL,
  manifest_sha256 TEXT NOT NULL,alpha REAL NOT NULL CHECK(alpha>0 AND alpha<=0.25),
  required_pairs INTEGER NOT NULL CHECK(required_pairs>=20 AND required_pairs<=100),
  state TEXT NOT NULL CHECK(state IN ('FROZEN','FINALIZED')),
  frozen_at TEXT NOT NULL,finalized_at TEXT);
 CREATE TABLE campaign_members_v2(
  campaign_id TEXT NOT NULL REFERENCES campaigns_v2(campaign_id),
  member_index INTEGER NOT NULL CHECK(member_index>=0),
  hypothesis_id TEXT NOT NULL UNIQUE REFERENCES hypotheses(id),
  PRIMARY KEY(campaign_id,member_index),
  UNIQUE(campaign_id,hypothesis_id));
 CREATE TABLE campaign_pvalues_v2(
  hypothesis_id TEXT PRIMARY KEY REFERENCES campaign_members_v2(hypothesis_id),
  p_value REAL NOT NULL CHECK(p_value>0 AND p_value<=1),
  independent_pairs INTEGER NOT NULL CHECK(independent_pairs>=20),
  method TEXT NOT NULL CHECK(method='paired_sign_flip_v1'),
  recorded_at TEXT NOT NULL);
 CREATE TABLE campaign_outcomes_v2(
  hypothesis_id TEXT PRIMARY KEY REFERENCES campaign_members_v2(hypothesis_id),
  campaign_id TEXT NOT NULL REFERENCES campaigns_v2(campaign_id),
  raw_status TEXT NOT NULL,raw_p REAL NOT NULL CHECK(raw_p>=0 AND raw_p<=1),
  holm_adjusted_p REAL NOT NULL CHECK(holm_adjusted_p>=0 AND holm_adjusted_p<=1),
  familywise_reject INTEGER NOT NULL CHECK(familywise_reject IN (0,1)),
  finalized_at TEXT NOT NULL);
 PRAGMA user_version=9;
 COMMIT;
 )SQL");
}
struct Family {
 std::string id,manifest,digest,state;
 double alpha=0;
 int required_pairs=0;
};
Family family(Store& s,const std::string& id){
 Sql q(s.raw(),"SELECT manifest,manifest_sha256,alpha,required_pairs,state FROM campaigns_v2 WHERE campaign_id=?");
 q.bind(1,id);
 if(q.next()!=SQLITE_ROW)throw Error("unknown campaign: "+id);
 Family f;f.id=id;f.manifest=q.str(0);f.digest=q.str(1);f.alpha=q.real(2);
 f.required_pairs=static_cast<int>(q.integer(3));f.state=q.str(4);
 if(sha256(f.manifest)!=f.digest)throw Error("v2 frozen campaign manifest hash mismatch");
 return f;
}
bool allowed_performance_kind(const std::string& k){
 return k=="PERFORMANCE_CLIFF"||k=="PERFORMANCE_CROSSOVER"||
   k=="CATEGORICAL_PERFORMANCE_CONTRAST"||
   k=="CATEGORICAL_PERFORMANCE_CROSSOVER"||k=="MULTI_CLIFF";
}
struct Member {
 std::string id,status,kind;
 double p=1,adjusted=1;
 bool rejected=false;
};
std::vector<Member> members(Store& s,const Family& f){
 Sql q(s.raw(),"SELECT h.id,h.status,h.kind,p.p_value,p.independent_pairs,"
  "s.seed_pairs,s.case_count,(SELECT COUNT(*) FROM confirmation_evidence e WHERE e.hypothesis_id=h.id)"
  " FROM campaign_members_v2 m JOIN hypotheses h ON h.id=m.hypothesis_id"
  " LEFT JOIN campaign_pvalues_v2 p ON p.hypothesis_id=h.id"
  " LEFT JOIN confirmation_seals s ON s.hypothesis_id=h.id"
  " WHERE m.campaign_id=? ORDER BY m.member_index");
 q.bind(1,f.id);
 std::vector<Member> out;
 while(q.next()==SQLITE_ROW){
  Member m;m.id=q.str(0);m.status=q.str(1);m.kind=q.str(2);
  if(!allowed_performance_kind(m.kind))throw Error("campaign contains nonperformance effect");
  if(m.status!="CONFIRMED"&&m.status!="NOT_CONFIRMED")
   throw Error("campaign member lacks finalized holdout verdict: "+m.id);
  if(m.status=="CONFIRMED"){
   if(sqlite3_column_type(q.stmt,3)==SQLITE_NULL||
      sqlite3_column_type(q.stmt,5)==SQLITE_NULL)
    throw Error("positive campaign member has no verified p/holdout seal: "+m.id);
   const auto p_value=q.real(3);
   const auto pairs=q.integer(4),sealed=q.integer(5),case_count=q.integer(6),linked=q.integer(7);
   if(!std::isfinite(p_value)||p_value<=0||p_value>1||
      pairs<f.required_pairs||sealed<f.required_pairs||case_count!=linked||
      case_count<static_cast<std::int64_t>(2)*f.required_pairs)
    throw Error("positive campaign member has inadequate independent evidence: "+m.id);
   m.p=p_value;
  }else m.p=1.0; // Conservative: negative or incomplete test never creates a rejection.
  out.push_back(m);
 }
 return out;
}
}
void freeze_campaign_v2(Store& s,const Json& manifest){
 require_v2(s);
 if(!manifest.p||json_object_get_type(manifest.p)!=json_type_object)
  throw Error("campaign manifest must be an object");
 const std::set<std::string> allowed={"schema_version","campaign_id","members","alpha","confirm_seed_pairs","test"};
 json_object_object_foreach(manifest.p,key,value){
  (void)value;if(!allowed.count(key))throw Error(std::string("unknown campaign manifest key: ")+key);
 }
 if(json_object_object_length(manifest.p)!=static_cast<int>(allowed.size())||
    !manifest.get("schema_version")||json_object_get_type(manifest.get("schema_version"))!=json_type_int||
    manifest.i("schema_version")!=1)
  throw Error("campaign v2 manifest must contain all six fields and schema version 1");
 const auto* id_raw=manifest.get("campaign_id");
 const auto* alpha_raw=manifest.get("alpha");
 const auto* pairs_raw=manifest.get("confirm_seed_pairs");
 const auto* test_raw=manifest.get("test");
 auto* selected=manifest.get("members");
 if(!id_raw||json_object_get_type(id_raw)!=json_type_string||
    !alpha_raw||(json_object_get_type(alpha_raw)!=json_type_double&&json_object_get_type(alpha_raw)!=json_type_int)||
    !pairs_raw||json_object_get_type(pairs_raw)!=json_type_int||
    !test_raw||json_object_get_type(test_raw)!=json_type_string||
    !selected||json_object_get_type(selected)!=json_type_array)
  throw Error("campaign manifest has incorrect field types");
 const auto id=manifest.s("campaign_id");
 if(id.empty()||id.size()>64||!std::all_of(id.begin(),id.end(),[](unsigned char c){
  return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_';
 }))throw Error("invalid v2 campaign ID");
 if(manifest.s("test")!="paired_sign_flip_v1")throw Error("unsupported v2 confirmatory campaign test");
 const double alpha=manifest.d("alpha");
 if(!std::isfinite(alpha)||alpha<0.001||alpha>0.25)throw Error("invalid v2 family alpha");
 const auto pairs=manifest.i("confirm_seed_pairs");
 const auto configured=Json::parse(s.config()).i("confirm_blocks",20);
 if(pairs<20||pairs>100||pairs!=configured)
  throw Error("campaign seed cohort count must match frozen experiment confirm_blocks (20..100)");
 const auto n=json_object_array_length(selected);
 if(n<1||n>16)throw Error("v2 campaign family must have 1..16 predeclared members");
 if(count(s.raw(),"SELECT COUNT(*) FROM samples WHERE holdout=1")!=0)
  throw Error("campaign must be frozen before first holdout sample");
 std::set<std::string> seen;
 std::vector<std::string> ids;
 for(std::size_t i=0;i<n;++i){
  auto* e=json_object_array_get_idx(selected,i);
  if(!e||json_object_get_type(e)!=json_type_string)
   throw Error("campaign member ID must be a string");
  const auto member=std::string(json_object_get_string(e));
  if(member.empty()||member.size()>64||!seen.insert(member).second)
   throw Error("duplicate/empty campaign member");
  Sql h(s.raw(),"SELECT status,kind FROM hypotheses WHERE id=?");h.bind(1,member);
  if(h.next()!=SQLITE_ROW||h.str(0)!="HYPOTHESIZED"||
     !allowed_performance_kind(h.str(1)))
   throw Error("campaign member must be an eligible frozen performance hypothesis: "+member);
  ids.push_back(member);
 }
 upgrade_campaign_v2(s);
 if(count(s.raw(),"SELECT COUNT(*) FROM campaigns_v2")!=0)
  throw Error("one frozen family per experiment; campaign already registered");
 const auto serialized=manifest.str(),hash=sha256(serialized);
 s.exec("BEGIN IMMEDIATE");
 try{
  Sql insert(s.raw(),"INSERT INTO campaigns_v2(campaign_id,manifest,manifest_sha256,alpha,"
    "required_pairs,state,frozen_at) VALUES(?,?,?,?,?,'FROZEN',?)");
  insert.bind(1,id);insert.bind(2,serialized);insert.bind(3,hash);insert.bind(4,alpha);
  insert.bind(5,pairs);insert.bind(6,now_utc());if(insert.next()!=SQLITE_DONE)throw Error("campaign insert failed");
  for(std::size_t i=0;i<ids.size();++i){
   Sql link(s.raw(),"INSERT INTO campaign_members_v2(campaign_id,member_index,hypothesis_id) VALUES(?,?,?)");
   link.bind(1,id);link.bind(2,static_cast<std::int64_t>(i));link.bind(3,ids[i]);
   if(link.next()!=SQLITE_DONE)throw Error("campaign member freeze failed");
  }
  s.exec("COMMIT");
 }catch(...){s.exec("ROLLBACK");throw;}
}
void record_campaign_signflip_v2(Store& s,const std::string& id,double p_value,int independent_pairs){
 if(!std::isfinite(p_value)||p_value<=0||p_value>1||independent_pairs<20)
  throw Error("invalid campaign fixed-sample p-value");
 const auto cfg=Json::parse(s.config());
 if(cfg.s("policy_version")!="v2")return;
 if(count(s.raw(),"PRAGMA user_version")!=9)return; // v2 without a campaign unchanged
 Sql query(s.raw(),"SELECT c.state,c.required_pairs,h.status,"
   "s.seed_pairs,s.case_count,(SELECT COUNT(*) FROM confirmation_evidence e WHERE e.hypothesis_id=h.id)"
   " FROM campaign_members_v2 m JOIN campaigns_v2 c ON c.campaign_id=m.campaign_id"
   " JOIN hypotheses h ON h.id=m.hypothesis_id"
   " LEFT JOIN confirmation_seals s ON s.hypothesis_id=h.id"
   " WHERE m.hypothesis_id=?");
 query.bind(1,id);
 if(query.next()!=SQLITE_ROW)return; // unrelated single-hypothesis holdout
 if(query.str(0)!="FROZEN"||query.str(2)!="CONFIRMED"||
   sqlite3_column_type(query.stmt,3)==SQLITE_NULL||
   query.integer(3)<query.integer(1)||query.integer(1)!=independent_pairs||
   query.integer(4)!=query.integer(5))
  throw Error("cannot attest campaign p before final confirmed holdout seal");
 Sql record(s.raw(),"INSERT INTO campaign_pvalues_v2(hypothesis_id,p_value,independent_pairs,method,recorded_at)"
  " VALUES(?,?,?,'paired_sign_flip_v1',?)");
 record.bind(1,id);record.bind(2,p_value);record.bind(3,static_cast<std::int64_t>(independent_pairs));
 record.bind(4,now_utc());
 if(record.next()!=SQLITE_DONE)throw Error("cannot record campaign p-value twice");
}
Json finalize_campaign_v2(Store& s,const std::string& campaign_id){
 require_v2(s);
 if(count(s.raw(),"PRAGMA user_version")!=9)throw Error("no v2 campaign schema/frozen family");
 const auto frozen=family(s,campaign_id);
 if(frozen.state!="FROZEN")throw Error("campaign already finalized");
 auto trials=members(s,frozen);
 const auto expected=Json::parse(frozen.manifest);
 auto* list=expected.get("members");
 if(!list||trials.size()!=json_object_array_length(list)||trials.empty())
  throw Error("frozen campaign membership changed");
 std::vector<double> probabilities;probabilities.reserve(trials.size());
 for(const auto& m:trials)probabilities.push_back(m.p);
 const auto verdicts=holm_fwer(probabilities,frozen.alpha);
 std::vector<std::size_t> sorted(trials.size());
 for(std::size_t i=0;i<sorted.size();++i)sorted[i]=i;
 std::stable_sort(sorted.begin(),sorted.end(),[&](auto a,auto b){return trials[a].p<trials[b].p;});
 double running=0;
 for(std::size_t rank=0;rank<sorted.size();++rank){
  const auto idx=sorted[rank];
  running=std::max(running,probabilities[idx]*static_cast<double>(trials.size()-rank));
  trials[idx].adjusted=std::min(1.0,running);
  trials[idx].rejected=verdicts[idx]&&trials[idx].status=="CONFIRMED";
 }
 s.exec("BEGIN IMMEDIATE");
 try{
  for(const auto& m:trials){
   Sql insert(s.raw(),"INSERT INTO campaign_outcomes_v2(hypothesis_id,campaign_id,"
     "raw_status,raw_p,holm_adjusted_p,familywise_reject,finalized_at) VALUES(?,?,?,?,?,?,?)");
   insert.bind(1,m.id);insert.bind(2,campaign_id);insert.bind(3,m.status);
   insert.bind(4,m.p);insert.bind(5,m.adjusted);insert.bind(6,static_cast<std::int64_t>(m.rejected?1:0));
   insert.bind(7,now_utc());
   if(insert.next()!=SQLITE_DONE)throw Error("failed to store campaign outcome");
  }
  Sql finish(s.raw(),"UPDATE campaigns_v2 SET state='FINALIZED',finalized_at=?"
     " WHERE campaign_id=? AND state='FROZEN'");
  finish.bind(1,now_utc());finish.bind(2,campaign_id);
  if(finish.next()!=SQLITE_DONE||sqlite3_changes(s.raw())!=1)
   throw Error("campaign finalization race");
  s.exec("COMMIT");
 }catch(...){s.exec("ROLLBACK");throw;}
 Json output;output.put("campaign_id",campaign_id);output.put("manifest_sha256",frozen.digest);
 output.put("alpha",frozen.alpha);output.put("family_size",static_cast<std::int64_t>(trials.size()));
 output.put("status","FINALIZED");
 auto* rows=json_object_new_array();
 for(const auto& m:trials){
  Json row;row.put("hypothesis_id",m.id);row.put("raw_status",m.status);
  row.put("p_value",m.p);row.put("holm_adjusted_p",m.adjusted);
  row.put("familywise_reject",m.rejected);
  json_object_array_add(rows,row.p);row.p=nullptr;
 }
 json_object_object_add(output.p,"members",rows);
 return output;
}
} // namespace ps
