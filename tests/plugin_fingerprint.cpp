#include "phaseshift/store.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <sqlite3.h>
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
 check(argc==3,"need scratch directory and compiled native plugin");
 namespace fs=std::filesystem;
 const fs::path dir(argv[1]);fs::remove_all(dir);fs::create_directories(dir);
 const fs::path plugin=dir/"fingerprinted.so";fs::copy_file(argv[2],plugin);
 const auto digest=ps::sha256_file(plugin.string());
 ps::Store s((dir/"run.sqlite").string());s.experiment("{\"schema_version\":1}",ps::environment());
 ps::Case c;c.family="integer_array";c.n=16;c=ps::make_case(c);
 ps::Observation v;v.algorithm="plugin:"+plugin.string();v.status="wrong_answer";v.block=0;s.add_observation(c,v,false);
 sqlite3_stmt* q=nullptr;
 check(sqlite3_prepare_v2(s.raw(),"SELECT version FROM algorithms WHERE algorithm_id=?",-1,&q,nullptr)==SQLITE_OK,"SQL algorithm prepare");
 sqlite3_bind_text(q,1,v.algorithm.c_str(),-1,SQLITE_TRANSIENT);
 check(sqlite3_step(q)==SQLITE_ROW,"plugin registry row missing");
 const std::string actual=reinterpret_cast<const char*>(sqlite3_column_text(q,0));sqlite3_finalize(q);
 check(actual=="external-abi-v1;sha256="+digest,"plugin binary digest not retained");
 {std::ofstream f(plugin,std::ios::app|std::ios::binary);f.put('X');}
 bool rejected=false;try{s.add_observation(c,v,false);}catch(const ps::Error&){rejected=true;}
 check(rejected,"changed external plugin was mixed into earlier experimental evidence");
 std::cout<<"PASS plugin binary digest persisted and tampering rejected\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"plugin fingerprint: "<<e.what()<<'\n';return 1;}}
