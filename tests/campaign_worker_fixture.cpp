#include "phaseshift/store.hpp"
#include <filesystem>
#include <iostream>
#include <string>
int main(int argc,char** argv){try{
 if(argc!=2)throw ps::Error("campaign worker fixture requires run directory");
 const std::filesystem::path root(argv[1]);
 std::filesystem::remove_all(root);
 std::filesystem::create_directories(root);
 ps::Store store((root/"run.sqlite").string());
 store.upgrade_v2_schema();
 // Frozen v2 design: real sorting algorithm, worker timing, single algorithm,
 // four legal sizes and 20 independent paired confirmation seeds.
 store.experiment(R"({"schema_version":1,"experiment_id":"campaign-real-worker",
 "policy_version":"v2","problem_family":"integer_array","algorithm_a":"std_sort",
 "search_method":"uniform_v2","primary_axis":"n","seed":141,
 "max_cases":4,"discovery_blocks":1,"confirm_blocks":20,"warmup":0,
 "sample_ms":2000,"minimum_batch_ms":1,"rss_mb":1024,
 "total_wall_seconds":120,"fdr_q":0.05,"confirmation_confidence":0.99,
 "base_params":{"n":64,"distribution":"uniform"},
 "axes":{"n":["64","128","256","512"]}})",ps::environment());
 for(int rank=1;rank<=2;++rank){
  const auto id="H"+std::to_string(rank);
  const auto left=rank==1?64:128;
  const auto right=rank==1?128:256;
  store.hypothesis(id,"OBSERVED","n",left,right,1.0,
   "synthetic predeclared test geometry only; not a discovery result",
   "PERFORMANCE_CLIFF");
  store.hypothesis(id,"HYPOTHESIZED","n",left,right,1.0,
   "fixed before holdout, independent real-worker confirmation");
 }
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
