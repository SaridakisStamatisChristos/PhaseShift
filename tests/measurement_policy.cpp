#include "phaseshift/experiment.hpp"
#include <iostream>
#include <map>
#include <stdexcept>

static void require(bool condition,const char* what){if(!condition)throw std::runtime_error(what);}
int main(){
 try{
  const auto cfg=ps::config_from_json(ps::Json::parse(R"({"schema_version":1,"problem_family":"integer_array","algorithms":["std_sort","mergesort"],"axes":{"n":{"values":[16,32]}},"search":{"method":"adaptive_partition","exploration_fraction":0.5,"max_cases":2},"measurement":{"minimum_batch_ms":3,"isolation_mode":"per_sample","paired_discovery_blocks":2}})"));
  require(cfg.exploration_fraction==0.5,"exploration fraction was not parsed");
  require(cfg.minimum_batch_ms==3,"calibration floor was not parsed");
  require(cfg.isolation_mode=="per_sample","isolation mode was not parsed");
  auto invalid=[](const char* json){try{(void)ps::config_from_json(ps::Json::parse(json));return false;}catch(const ps::Error&){return true;}};
  require(invalid(R"({"problem_family":"calibration","search":{"exploration_fraction":-0.01}})"),"negative exploration fraction accepted");
  require(invalid(R"({"problem_family":"calibration","search":{"exploration_fraction":1.01}})"),"fraction >1 accepted");
  require(invalid(R"({"problem_family":"calibration","measurement":{"minimum_batch_ms":0}})"),"zero calibration floor accepted");
  require(invalid(R"({"problem_family":"calibration","measurement":{"isolation_mode":"none"}})"),"invalid isolation accepted");
  require(invalid(R"({"problem_family":"calibration","measurement":{"isolation_mode":33}})"),"wrongly typed isolation accepted");
  require(invalid(R"({"problem_family":"calibration","measurement":{"minimum_batch_ms":2.5}})"),"fractional batch milliseconds accepted");
  require(invalid(R"({"problem_family":"calibration","search":{"exploration_fraction":"half"}})"),"string exploration fraction accepted");
  ps::Case input;input.family="integer_array";input.n=12;input.seed=8871;input=ps::make_case(input);
  const auto separate=ps::measure(input,{"std_sort","mergesort"},2,0,3000,256,1,true);
  require(separate.size()==8,"per-sample A/B block count unexpected");
  std::map<int,std::map<std::string,int>> blocks;
  for(const auto& o:separate){require(o.status=="ok"&&o.batch>0,"per-sample measurement invalid");require(o.order_position>=0&&o.order_position<4,"invalid order position");blocks[static_cast<int>(o.block)][o.algorithm]++;}
  require(blocks.size()==2,"pairing block IDs were not preserved");
  for(const auto& [block,counts]:blocks){(void)block;require(counts.at("std_sort")==2&&counts.at("mergesort")==2,"ABBA pairing violated");}
  std::cout<<"Measurement policy and isolated per-sample correctness PASS\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<"Measurement policy FAIL: "<<e.what()<<"\n";return 1;}
}
