#include "phaseshift/campaign_v2.hpp"
#include <sqlite3.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#define REQUIRE(x) do{if(!(x))throw std::runtime_error("campaign_v2: " #x);}while(false)
static bool fails(const auto& run){try{run();return false;}catch(const ps::Error&){return true;}}
static int scalar(sqlite3* db,const char* sql){
 sqlite3_stmt* p=nullptr;REQUIRE(sqlite3_prepare_v2(db,sql,-1,&p,nullptr)==SQLITE_OK);
 REQUIRE(sqlite3_step(p)==SQLITE_ROW);
 const auto result=sqlite3_column_int(p,0);sqlite3_finalize(p);return result;
}
int main(int argc,char** argv){try{
 REQUIRE(argc==2);
 namespace fs=std::filesystem;fs::remove_all(argv[1]);fs::create_directories(argv[1]);
 ps::Store store(std::string(argv[1])+"/campaign.sqlite");
 store.upgrade_v2_schema();
 store.experiment(R"({"policy_version":"v2","confirm_blocks":20})",ps::environment());
 for(int index=1;index<=3;++index){
  auto id="H"+std::to_string(index);
  const auto left=static_cast<std::int64_t>(index*5);
  store.hypothesis(id,"OBSERVED","x",left,left+1,2.0,"discovery","PERFORMANCE_CLIFF");
  store.hypothesis(id,"HYPOTHESIZED","x",left,left+1,2.0,"frozen");
 }
 const auto valid=ps::Json::parse(R"({"schema_version":1,"campaign_id":"C1","members":["H1","H2","H3"],"alpha":0.05,"confirm_seed_pairs":20,"test":"paired_sign_flip_v1"})");
 REQUIRE(fails([&]{ps::freeze_campaign_v2(store,ps::Json::parse(R"({"schema_version":1,"campaign_id":"bad","members":["H1","H1"],"alpha":0.05,"confirm_seed_pairs":20,"test":"paired_sign_flip_v1"})"));}));
 REQUIRE(fails([&]{ps::freeze_campaign_v2(store,ps::Json::parse(R"({"schema_version":1,"campaign_id":"bad","members":["H1"],"alpha":1.0,"confirm_seed_pairs":20,"test":"paired_sign_flip_v1"})"));}));
 REQUIRE(fails([&]{ps::freeze_campaign_v2(store,ps::Json::parse(R"({"schema_version":1,"campaign_id":"bad","members":["H1"],"alpha":0.05,"confirm_seed_pairs":19,"test":"paired_sign_flip_v1"})"));}));
 REQUIRE(scalar(store.raw(),"PRAGMA user_version")==8); // no invalid-manifest migration
 ps::freeze_campaign_v2(store,valid);
 REQUIRE(scalar(store.raw(),"PRAGMA user_version")==9);
 REQUIRE(scalar(store.raw(),"SELECT COUNT(*) FROM campaign_members_v2")==3);
 {auto initial=store.summary();auto* list=initial.get("hypotheses");
  REQUIRE(list&&json_object_array_length(list)==3);
  for(std::size_t i=0;i<json_object_array_length(list);++i){
   ps::Json h(json_object_get(json_object_array_get_idx(list,i)));
   REQUIRE(!h.b("familywise_reject"));
   REQUIRE(h.s("familywise_status")=="PENDING_FAMILY_FINALIZATION");
  }
 }
 REQUIRE(fails([&]{ps::freeze_campaign_v2(store,valid);}));
 REQUIRE(fails([&]{(void)ps::finalize_campaign_v2(store,"C1");}));
 ps::Observation v;v.algorithm="calibration";v.status="ok";v.ns=1200;v.batch=1;v.work=44;
 for(int index=1;index<=2;++index){
  const auto id="H"+std::to_string(index);
  const auto left=index*5;
  store.hypothesis(id,"TESTED","x",left,left+1,2.0,"holdout");
  for(int pair=0;pair<20;++pair)for(int side=0;side<2;++side){
   ps::Case c;c.family="calibration";c.x=left+side;c.seed=static_cast<std::uint64_t>(100000*index+pair);
   c=ps::make_case(c);store.add_observation(c,v,true);
  }
  store.seal_confirmation(id,"calibration",20);
  store.hypothesis(id,"CONFIRMED","x",left,left+1,2.0,"attested");
  REQUIRE(fails([&]{ps::record_campaign_signflip_v2(store,id,0.0,20);}));
  REQUIRE(fails([&]{ps::record_campaign_signflip_v2(store,id,0.5,19);}));
  ps::record_campaign_signflip_v2(store,id,index==1?0.0005:0.03,20);
  REQUIRE(fails([&]{ps::record_campaign_signflip_v2(store,id,0.001,20);}));
 }
 store.hypothesis("H3","NOT_CONFIRMED","x",15,16,0,"no complete/valid holdout");
 const auto answer=ps::finalize_campaign_v2(store,"C1");
 REQUIRE(answer.s("status")=="FINALIZED");
 REQUIRE(scalar(store.raw(),"SELECT COUNT(*) FROM campaign_outcomes_v2")==3);
 REQUIRE(scalar(store.raw(),"SELECT COUNT(*) FROM campaign_outcomes_v2 WHERE familywise_reject=1")==1);
 REQUIRE(scalar(store.raw(),"SELECT COUNT(*) FROM campaign_outcomes_v2 WHERE raw_p=1")==1);
 REQUIRE(scalar(store.raw(),"SELECT COUNT(*) FROM campaigns_v2 WHERE state='FINALIZED'")==1);
 {auto outcome=store.summary();REQUIRE(outcome.i("registered_campaigns")==1);
  REQUIRE(outcome.i("familywise_positive_members")==1);
  auto* list=outcome.get("hypotheses");REQUIRE(list&&json_object_array_length(list)==3);
  for(std::size_t i=0;i<json_object_array_length(list);++i){
   ps::Json h(json_object_get(json_object_array_get_idx(list,i)));
   REQUIRE(h.b("familywise_reject")== (h.s("id")=="H1"));
   REQUIRE(h.s("campaign_state")=="FINALIZED");
  }
 }
 REQUIRE(fails([&]{(void)ps::finalize_campaign_v2(store,"C1");}));
 // Independent SQL failure injection: the confirmation path must not publish
 // an unsupported raw positive if the p-value attestation aborts.
 ps::Store atomic(std::string(argv[1])+"/rollback.sqlite");
 atomic.upgrade_v2_schema();
 atomic.experiment(R"({"policy_version":"v2","confirm_blocks":20})",ps::environment());
 atomic.hypothesis("H1","OBSERVED","x",1,2,2.0,"discovery","PERFORMANCE_CLIFF");
 atomic.hypothesis("H1","HYPOTHESIZED","x",1,2,2.0,"frozen");
 const auto only=ps::Json::parse(R"({"schema_version":1,"campaign_id":"ATOMIC","members":["H1"],"alpha":0.05,"confirm_seed_pairs":20,"test":"paired_sign_flip_v1"})");
 ps::freeze_campaign_v2(atomic,only);
 atomic.hypothesis("H1","TESTED","x",1,2,2.0,"independent holdout");
 for(int pair=0;pair<20;++pair)for(int side=0;side<2;++side){
  ps::Case c;c.family="calibration";c.x=1+side;
  c.seed=static_cast<std::uint64_t>(800000+pair);c=ps::make_case(c);
  atomic.add_observation(c,v,true);
 }
 bool injected=false;
 atomic.exec("BEGIN IMMEDIATE");
 try{
  atomic.seal_confirmation("H1","calibration",20);
  atomic.hypothesis("H1","CONFIRMED","x",1,2,2.0,"should not survive failed p");
  ps::record_campaign_signflip_v2(atomic,"H1",0.0,20);
  atomic.exec("COMMIT");
 }catch(const ps::Error&){injected=true;atomic.exec("ROLLBACK");}
 REQUIRE(injected);
 REQUIRE(scalar(atomic.raw(),"SELECT COUNT(*) FROM confirmation_seals")==0);
 REQUIRE(scalar(atomic.raw(),"SELECT COUNT(*) FROM campaign_pvalues_v2")==0);
 REQUIRE(scalar(atomic.raw(),"SELECT COUNT(*) FROM hypotheses WHERE status='TESTED'")==1);
 REQUIRE(scalar(atomic.raw(),"SELECT COUNT(*) FROM samples WHERE holdout=1")==40);
 atomic.exec("BEGIN IMMEDIATE");
 try{
  atomic.seal_confirmation("H1","calibration",20);
  atomic.hypothesis("H1","CONFIRMED","x",1,2,2.0,"attested");
  ps::record_campaign_signflip_v2(atomic,"H1",0.001,20);
  atomic.exec("COMMIT");
 }catch(...){atomic.exec("ROLLBACK");throw;}
 REQUIRE(scalar(atomic.raw(),"SELECT COUNT(*) FROM confirmation_seals")==1);
 REQUIRE(scalar(atomic.raw(),"SELECT COUNT(*) FROM campaign_pvalues_v2")==1);
 REQUIRE(scalar(atomic.raw(),"SELECT COUNT(*) FROM hypotheses WHERE status='CONFIRMED'")==1);
 // Immutable read-only connections ignore an active WAL unless it has been
 // checkpointed; flush the completed campaign transaction before verifying.
 REQUIRE(sqlite3_wal_checkpoint_v2(store.raw(),nullptr,SQLITE_CHECKPOINT_TRUNCATE,
                                   nullptr,nullptr)==SQLITE_OK);
 ps::Store readonly(std::string(argv[1])+"/campaign.sqlite",true);
 REQUIRE(scalar(readonly.raw(),"PRAGMA user_version")==9);
 REQUIRE(scalar(readonly.raw(),"SELECT COUNT(*) FROM campaign_outcomes_v2")==3);
 // Fail-closed: do not claim family positivity if a nominally confirmed
 // hypothesis lacks a valid fixed-sample p-value even with an old raw seal.
 ps::Store missing(std::string(argv[1])+"/missing.sqlite");
 missing.upgrade_v2_schema();missing.experiment(R"({"policy_version":"v2","confirm_blocks":20})",ps::environment());
 missing.hypothesis("H1","OBSERVED","x",1,2,1,"discovered","PERFORMANCE_CLIFF");
 missing.hypothesis("H1","HYPOTHESIZED","x",1,2,1,"frozen");
 const auto single=ps::Json::parse(R"({"schema_version":1,"campaign_id":"C2","members":["H1"],"alpha":0.05,"confirm_seed_pairs":20,"test":"paired_sign_flip_v1"})");
 ps::freeze_campaign_v2(missing,single);
 missing.hypothesis("H1","NOT_CONFIRMED","x",1,2,0,"negative result");
 REQUIRE(scalar(missing.raw(),"PRAGMA user_version")==9);
 auto negative=ps::finalize_campaign_v2(missing,"C2");
 REQUIRE(negative.s("status")=="FINALIZED");
 REQUIRE(scalar(missing.raw(),"SELECT COUNT(*) FROM campaign_outcomes_v2 WHERE familywise_reject=1")==0);
 std::cout<<"PASS frozen v2 campaign, additive v9, raw sealed holdouts, Holm dependence-safe stepdown, negative p=1, replay/no-retest\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
