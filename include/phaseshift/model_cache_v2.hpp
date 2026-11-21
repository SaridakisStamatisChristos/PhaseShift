#pragma once
#include "phaseshift/core.hpp"
#include "phaseshift/runner.hpp"
#include <sqlite3.h>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace ps {
// Independently reconstructable, derived cache: run.sqlite remains the ONLY
// authoritative sample/decision ledger. A crash between authoritative COMMIT
// and checkpoint writing leaves a missing row which replay can rebuild.
// A mismatched/extra row, changed configuration or invalid SQL fails closed.
class ModelCheckpointCacheV2 {
 sqlite3* db_=nullptr;
 void exec(const char* sql){
  char* error=nullptr;
  if(sqlite3_exec(db_,sql,nullptr,nullptr,&error)!=SQLITE_OK){
   std::string why=error?error:"unknown SQLite error";
   sqlite3_free(error);
   throw Error("v2 model cache SQL: "+why);
  }
 }
 static std::string encoded(const std::string& text){
  return std::to_string(text.size())+":"+text;
 }
 static std::string fingerprint(const std::vector<Observation>& observations){
  std::string raw;
  for(const auto& o:observations){
   // Ignore detail adornments and failed-case unset timing columns:
   // Store canonicalizes those when inserting into source SQLite.
   const bool valid=o.status=="ok";
   raw+=encoded(o.algorithm)+encoded(o.status);
   raw+=encoded(std::to_string(valid?o.ns:0));
   raw+=encoded(std::to_string(valid?o.batch:0));
   raw+=encoded(std::to_string(valid?o.work:0));
   raw+=encoded(std::to_string(o.block));
   raw+=encoded(std::to_string(valid?o.cpu_ns:-1));
   raw+=encoded(std::to_string(o.peak_rss_bytes));
   raw+=encoded(std::to_string(o.order_position));
   raw+=encoded(o.order_seed);
   raw+=encoded(o.digest);
  }
  return sha256(raw);
 }
 using Statement=std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)>;
 Statement prepare(const char* sql){
  sqlite3_stmt* p=nullptr;
  if(sqlite3_prepare_v2(db_,sql,-1,&p,nullptr)!=SQLITE_OK)
   throw Error(std::string("v2 cache SQL prepare: ")+sqlite3_errmsg(db_));
  return Statement(p,&sqlite3_finalize);
 }
 void check(int code,const char* where){
  if(code!=SQLITE_OK)throw Error(std::string("v2 cache bind failure at ")+where);
 }
public:
 ModelCheckpointCacheV2(const std::string& path,const std::string& config_digest){
  namespace fs=std::filesystem;
  if(config_digest.empty()||fs::is_symlink(fs::symlink_status(path))||
    (fs::exists(path)&&!fs::is_regular_file(path)))
   throw Error("v2 model cache path/config invalid");
  const int status=sqlite3_open_v2(path.c_str(),&db_,
    SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,nullptr);
  if(status!=SQLITE_OK){
   const auto msg=std::string(db_?sqlite3_errmsg(db_):"open failed");
   if(db_)sqlite3_close(db_);db_=nullptr;
   throw Error("v2 model cache open: "+msg);
  }
  try{
   exec("PRAGMA journal_mode=WAL;PRAGMA busy_timeout=5000;");
   int n=0;
   {
    auto version=prepare("PRAGMA user_version");
    if(sqlite3_step(version.get())!=SQLITE_ROW)
     throw Error("v2 model cache user_version unavailable");
    n=sqlite3_column_int(version.get(),0);
   }
   if(n!=0&&n!=1)throw Error("unsupported v2 model cache version");
   if(n==0){
    {
     auto tables=prepare("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'");
     if(sqlite3_step(tables.get())!=SQLITE_ROW||sqlite3_column_int64(tables.get(),0)!=0)
      throw Error("v2 model cache version-zero file is not empty");
    }
    exec("BEGIN IMMEDIATE;"
     "CREATE TABLE cache_identity("
      "id INTEGER PRIMARY KEY CHECK(id=1),config_sha256 TEXT NOT NULL);"
     "CREATE TABLE checkpoints("
      "ordinal INTEGER PRIMARY KEY CHECK(ordinal>0),"
      "pool_index INTEGER NOT NULL UNIQUE,"
      "case_id TEXT NOT NULL UNIQUE,"
      "rng_hex TEXT NOT NULL,"
      "metric REAL,"
      "cpu_ms REAL NOT NULL CHECK(cpu_ms>=1),"
      "sample_sha256 TEXT NOT NULL);"
     "PRAGMA user_version=1;"
     "COMMIT;");
   }
   auto q=prepare("SELECT config_sha256 FROM cache_identity WHERE id=1");
   const int rc=sqlite3_step(q.get());
   if(rc==SQLITE_ROW){
    if(config_digest!=std::string(reinterpret_cast<const char*>(sqlite3_column_text(q.get(),0))))
     throw Error("v2 model cache configuration digest mismatch");
   }else if(rc==SQLITE_DONE){
    auto ins=prepare("INSERT INTO cache_identity(id,config_sha256) VALUES(1,?)");
    check(sqlite3_bind_text(ins.get(),1,config_digest.c_str(),-1,SQLITE_TRANSIENT),"identity");
    if(sqlite3_step(ins.get())!=SQLITE_DONE)
     throw Error("v2 model cache identity insert failed");
   }else throw Error("v2 model cache identity lookup failed");
  }catch(...){sqlite3_close(db_);db_=nullptr;throw;}
 }
 ModelCheckpointCacheV2(const ModelCheckpointCacheV2&)=delete;
 ModelCheckpointCacheV2& operator=(const ModelCheckpointCacheV2&)=delete;
 ~ModelCheckpointCacheV2(){if(db_)sqlite3_close(db_);}
 void remember(std::size_t ordinal,std::uint64_t pool_index,
   const std::string& case_id,std::uint64_t rng_state,
   const std::optional<double>& metric,double cpu_ms,
   const std::vector<Observation>& evidence){
  if(ordinal==0||ordinal>20000||case_id.empty()||evidence.empty()||
    pool_index>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())||
    (metric&&!std::isfinite(*metric))||!std::isfinite(cpu_ms)||cpu_ms<1)
   throw Error("invalid v2 model cache checkpoint");
  std::ostringstream state;state<<std::hex<<rng_state;
  const auto fingerprint_sha=fingerprint(evidence);
  auto existing=prepare("SELECT pool_index,case_id,rng_hex,metric,cpu_ms,sample_sha256"
                         " FROM checkpoints WHERE ordinal=?");
  check(sqlite3_bind_int64(existing.get(),1,static_cast<sqlite3_int64>(ordinal)),"ordinal");
  const auto found=sqlite3_step(existing.get());
  if(found==SQLITE_ROW){
   const auto str=[&](int col){auto* p=sqlite3_column_text(existing.get(),col);
    return std::string(p?reinterpret_cast<const char*>(p):"");};
   const bool recorded_metric=sqlite3_column_type(existing.get(),3)!=SQLITE_NULL;
   if(sqlite3_column_int64(existing.get(),0)!=static_cast<std::int64_t>(pool_index)||
      str(1)!=case_id||str(2)!=state.str()||
      recorded_metric!=metric.has_value()||
      (recorded_metric&&sqlite3_column_double(existing.get(),3)!=*metric)||
      sqlite3_column_double(existing.get(),4)!=cpu_ms||str(5)!=fingerprint_sha)
    throw Error("v2 model cache disagrees with authoritative replay evidence");
   if(sqlite3_step(existing.get())!=SQLITE_DONE)
    throw Error("v2 model cache duplicate ordinal");
   return;
  }
  if(found!=SQLITE_DONE)throw Error("v2 model cache lookup failed");
  existing.reset();
  auto insert=prepare("INSERT INTO checkpoints"
    "(ordinal,pool_index,case_id,rng_hex,metric,cpu_ms,sample_sha256)"
    " VALUES(?,?,?,?,?,?,?)");
  check(sqlite3_bind_int64(insert.get(),1,static_cast<sqlite3_int64>(ordinal)),"ordinal");
  check(sqlite3_bind_int64(insert.get(),2,static_cast<sqlite3_int64>(pool_index)),"pool_index");
  check(sqlite3_bind_text(insert.get(),3,case_id.c_str(),-1,SQLITE_TRANSIENT),"case_id");
  const auto encoded_state=state.str();
  check(sqlite3_bind_text(insert.get(),4,encoded_state.c_str(),-1,SQLITE_TRANSIENT),"rng");
  if(metric)check(sqlite3_bind_double(insert.get(),5,*metric),"metric");
  else check(sqlite3_bind_null(insert.get(),5),"metric_null");
  check(sqlite3_bind_double(insert.get(),6,cpu_ms),"cpu_ms");
  check(sqlite3_bind_text(insert.get(),7,fingerprint_sha.c_str(),-1,SQLITE_TRANSIENT),"fingerprint");
  if(sqlite3_step(insert.get())!=SQLITE_DONE)
   throw Error(std::string("v2 model cache append: ")+sqlite3_errmsg(db_));
 }
 void verify_no_ahead(std::size_t committed_count){
  auto q=prepare("SELECT COUNT(*) FROM checkpoints WHERE ordinal>?");
  check(sqlite3_bind_int64(q.get(),1,static_cast<sqlite3_int64>(committed_count)),"committed count");
  if(sqlite3_step(q.get())!=SQLITE_ROW||sqlite3_column_int64(q.get(),0)!=0)
   throw Error("v2 model cache has an uncommitted or foreign future checkpoint");
 }
};
} // namespace ps
