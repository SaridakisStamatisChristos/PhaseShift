#include "phaseshift/confirmation_seeds_v2.hpp"
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("confirmation seeds: " #x);}while(false)
int main(){
 using ps::validate_v2_confirmation_seed_space;
 for(std::uint64_t rank=1;rank<=16;++rank){
  validate_v2_confirmation_seed_space(0,rank,20);
  validate_v2_confirmation_seed_space(1257,rank,100);
 }
 std::set<std::uint64_t> all;
 for(std::uint64_t rank=1;rank<=16;++rank){
  const auto offset=(rank-1)*1000000000ULL;
  for(std::uint64_t i=0;i<100;++i){
   CHECK(all.insert(1257+offset+1000000ULL+i*97ULL).second);
   CHECK(all.insert(1257+offset+7000000ULL+i*101ULL).second);
  }
  CHECK(all.insert(1257+offset+30000000ULL).second);
 }
 CHECK(all.size()==16*201);
 const auto limit=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
 const auto last=30000000ULL+99*101ULL;
 validate_v2_confirmation_seed_space(limit-last,1,100);
 bool bad=false;
 try{validate_v2_confirmation_seed_space(limit-last+1,1,100);}
 catch(const ps::Error&){bad=true;}CHECK(bad);
 bad=false;
 try{validate_v2_confirmation_seed_space(limit,16,100);}
 catch(const ps::Error&){bad=true;}CHECK(bad);
 bad=false;
 try{validate_v2_confirmation_seed_space(0,17,20);}
 catch(const ps::Error&){bad=true;}CHECK(bad);
 return 0;
}
