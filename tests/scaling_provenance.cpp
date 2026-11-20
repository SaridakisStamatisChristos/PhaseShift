#include "phaseshift/store.hpp"
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <stdexcept>
static void verify(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static std::int64_t count(sqlite3* db,const char* sql){sqlite3_stmt* s=nullptr;verify(sqlite3_prepare_v2(db,sql,-1,&s,nullptr)==SQLITE_OK,"SQL prepare failed");verify(sqlite3_step(s)==SQLITE_ROW,"SQL count failed");const auto n=sqlite3_column_int64(s,0);sqlite3_finalize(s);return n;}
int main(int argc,char** argv){try{
 verify(argc==3,"usage: test <scaling db> <scaling.json>");
 ps::Store db(argv[1],true);const auto json=ps::Json::file(argv[2]);
 auto* section=json.get("scaling_regime_evidence");verify(section!=nullptr,"scaling evidence missing");
 ps::Json evidence(json_object_get(section));
 verify(count(db.raw(),"SELECT COUNT(DISTINCT case_id) FROM samples WHERE holdout=0")>=18,"discovery cases incorrectly marked holdout");
 verify(count(db.raw(),"SELECT COUNT(*) FROM samples WHERE holdout=0")>=54,"discovery samples missing");
 std::set<int> discovery,holdout;
 for(const auto& c:db.cases()){
  const bool is_holdout=!db.observations(c.id,true).empty();
  const bool is_discovery=!db.observations(c.id,false).empty();
  verify(is_holdout!=is_discovery,"scaling case labeled both discovery and holdout or neither");
  (is_holdout?holdout:discovery).insert(static_cast<int>(c.n));
 }
 if(evidence.b("candidate")){
  auto* raw=evidence.get("holdout_sizes");
  // The candidate may abstain before TESTED when distinct new sizes are not
  // available; in this case the report must not invent a holdout population.
  if(!holdout.empty()){
   verify(raw&&json_object_get_type(raw)==json_type_array,"confirmed candidate has no actual holdout-size list");
   std::set<int> reported;for(std::size_t i=0;i<json_object_array_length(raw);i++)reported.insert(json_object_get_int(json_object_array_get_idx(raw,i)));
   verify(reported==holdout,"reported holdout sizes do not match persisted cases");
   for(int n:reported)verify(discovery.count(n)==0,"holdout size leaked into discovery");
  }
 }else{
  verify(holdout.empty(),"null scaling candidate collected unrequested confirmation rows");
 }
 std::cout<<"PASS scaling discovery/holdout provenance discovery_n="<<discovery.size()<<" holdout_n="<<holdout.size()<<'\n';
 return 0;
}catch(const std::exception& e){std::cerr<<"SCALING PROVENANCE: "<<e.what()<<'\n';return 1;}}
