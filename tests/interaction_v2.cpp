#include "phaseshift/interaction_v2.hpp"
#include "phaseshift/core.hpp"
#include <cassert>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>
int main(){
 const std::array<std::string,2> a={"low","high"},b={"cat-x","cat-y"};
 using ps::FactorialCellV2;
 std::vector<FactorialCellV2> rows;
 for(std::uint64_t seed=0;seed<5;seed++)
  for(int ai=0;ai<2;ai++)for(int bi=0;bi<2;bi++)
   rows.push_back({a[static_cast<std::size_t>(ai)],b[static_cast<std::size_t>(bi)],seed,
    static_cast<double>(ai!=bi), "ok"});
 const auto xor_result=ps::screen_factorial_v2(a,b,rows,3,1.0);
 assert(xor_result.kind=="CONDITIONAL_PERFORMANCE_CANDIDATE");
 assert(xor_result.complete_cohorts==5&&xor_result.median_contrast==-2.0);
 auto additive=rows;
 for(auto& cell:additive)
  cell.log_metric=static_cast<double>((cell.first=="high")+(cell.second=="cat-y"));
 assert(ps::screen_factorial_v2(a,b,additive,3,0.1).kind=="NO_INTERACTION_CANDIDATE");
 auto incomplete=rows;incomplete.pop_back();
 assert(ps::screen_factorial_v2(a,b,incomplete,3,1.).kind=="INSUFFICIENT_EVIDENCE");
 auto failures=rows;
 for(auto& cell:failures)if(cell.first=="high"&&cell.second=="cat-y"){
  cell.log_metric.reset();cell.status="timeout";
 }
 const auto f=ps::screen_factorial_v2(a,b,failures,3,1.);
 assert(f.kind=="CONDITIONAL_STATUS_CANDIDATE"&&f.failure_status=="timeout");
 bool rejected=false;
 auto duplicates=rows;duplicates.push_back(rows[0]);
 try{(void)ps::screen_factorial_v2(a,b,duplicates,3,1.);}catch(const ps::Error&){rejected=true;}
 assert(rejected);
 rejected=false;
 rows[0].log_metric=std::numeric_limits<double>::quiet_NaN();
 try{(void)ps::screen_factorial_v2(a,b,rows,3,1.);}catch(const ps::Error&){rejected=true;}
 assert(rejected);
 return 0;
}
