#include "phaseshift/experiment.hpp"
#include <iostream>
#include <algorithm>
#include <filesystem>
namespace ps {int worker_entry();}
static void require(bool yes,const std::string& message){if(!yes)throw ps::Error("TEST FAILURE: "+message);}
int main(int argc,char** argv){if(argc>1&&std::string(argv[1])=="--worker")return ps::worker_entry();
 try{
  std::uint64_t seed=0;require(ps::splitmix64(seed)==0xe220a8397b1dcdafULL,"SplitMix64 reference output");
  for(const auto& distribution:{"uniform","ascending","descending","nearly_sorted","organ_pipe","sawtooth","alternating","all_equal","heavy_duplicate","few_unique","zipf_like"}){
   for(int n:{0,1,2,3,5,32,101,256}){
    ps::Case c;c.family="integer_array";c.distribution=distribution;c.n=n;c.seed=123456;c=ps::make_case(c);
    require(c.id==ps::make_case(c).id,"deterministic case identity");
    const auto expected=ps::oracle(c).values;
    for(const auto& algorithm:{"std_sort","mergesort","quicksort_first_pivot","insertion_sort"}){
     require(ps::execute(c,algorithm).values==expected,std::string("sort mismatch: ")+algorithm+" "+distribution);
    }
    c.family="topk";for(int k:{0,1,n/2,n}){if(k>n)continue;c.k=k;c=ps::make_case(c);
      for(const auto& algorithm:{"topk_full_sort","topk_heap","topk_nth"})require(ps::execute(c,algorithm).values==ps::oracle(c).values,std::string("topk mismatch: ")+algorithm);
    }
   }
  }
  for(int n:{1,2,5,20})for(int count:{0,1,4,20,120}){
   if(count>n*(n-1))continue;for(const auto& cls:{"general","zero_one"}){
    ps::Case c;c.family="graph";c.vertices=n;c.edges=count;c.source=n-1;c.seed=41;c.weight_class=cls;c=ps::make_case(c);
    const auto expected=ps::oracle(c).values;
    for(const auto& alg:ps::algorithms_for("graph"))if(std::string(alg)!="zero_one_bfs"||c.weight_class=="zero_one")require(ps::execute(c,alg).values==expected,"shortest path mismatch");
   }
  }
  {
   ps::Case c;c.family="calibration";c.threshold=8;c.x=7;c=ps::make_case(c);require(ps::execute(c,"calibration").work==17,"calibration left");c.x=8;c=ps::make_case(c);require(ps::execute(c,"calibration").work==10008,"calibration right");c.negative=true;c=ps::make_case(c);require(ps::execute(c,"calibration").work==1000,"negative control");
  }
  {
   bool threw=false;try{ps::Case c;c.family="topk";c.n=3;c.k=4;ps::make_case(c);}catch(const ps::Error&){threw=true;}require(threw,"invalid topk k>n must throw");
   threw=false;try{ps::Case c;c.family="graph";c.vertices=2;c.edges=3;ps::make_case(c);}catch(const ps::Error&){threw=true;}require(threw,"invalid graph cardinality must throw");
   threw=false;try{ps::hex_decode("xy");}catch(const ps::Error&){threw=true;}require(threw,"invalid hex must throw");
  }
  {
   ps::Case c;c.family="integer_array";c.distribution="ascending";c.n=24;c=ps::make_case(c);
   auto observations=ps::measure(c,{"std_sort","quicksort_first_pivot"},2,1,10000,1024);
   require(observations.size()==8,"ABBA paired blocks");for(const auto& o:observations)require(o.status=="ok"&&o.ns>0&&o.batch>0,"worker correctness/timing");
   auto bad=ps::measure(c,{"plugin:"+(std::filesystem::path(argv[0]).parent_path()/"reverse_sort.so").string()},1,1,10000,1024);
   /* Plugin is deliberately wrong for descending input; must be excluded from timing. */
   require(!bad.empty()&&bad[0].status=="wrong_answer","plugin correctness gate");
  }
  {
   ps::Case c;c.family="integer_array";c.n=12;c=ps::make_case(c);
   const auto dir=std::filesystem::path(argv[0]).parent_path();
   auto bad=[&](int mode){auto lib=(dir/("fault_"+std::to_string(mode)+".so")).string();return ps::measure(c,{"plugin:"+lib},1,0,300,256);};
   auto crashed=bad(1);require(crashed.size()==1&&crashed[0].status=="signaled","segfault must be isolated and marked signaled");
   auto hung=bad(2);require(hung.size()==1&&hung[0].status=="timeout","hung worker must be terminated and timed out");
   auto oversize=bad(3);require(oversize.size()==1&&oversize[0].status=="infrastructure_error","oversized plugin output must fail");
   auto exception=bad(4);require(exception.size()==1&&exception[0].status=="infrastructure_error","plugin exception must not escape worker");
   auto rejected=bad(5);require(rejected.size()==1&&rejected[0].status=="invalid_input","plugin input rejection must not be timed");
   auto oom=bad(6);require(oom.size()==1&&oom[0].status=="out_of_memory","out-of-memory must not be timed");
   auto mismatch=bad(7);require(mismatch.size()==1&&mismatch[0].status=="infrastructure_error","ABI mismatch must fail");
  }
  std::cout<<"PASS: deterministic generator / sorting / topk / graphs / calibration / invalid input / isolated worker / plugin oracle\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
