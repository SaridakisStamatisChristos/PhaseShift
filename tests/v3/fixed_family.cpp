#include "phaseshift/v3/fixed_family.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#define CHECK(x) do{if(!(x))throw std::runtime_error("fixed family falsifier: " #x);}while(false)
using namespace ps;
using namespace ps::v3;
template<class F>void rejects(F&& fn){
 bool rejected=false;try{fn();}catch(const Error&){rejected=true;}CHECK(rejected);
}
int main(){
 // Independent enumeration of the 2^20 binary sign outcomes.
 std::vector<std::uint64_t> count(21,0);
 for(std::uint32_t mask=0;mask<(1U<<20);++mask){
  auto value=mask;std::size_t n=0;
  while(value){n+=value&1U;value>>=1U;}
  ++count[n];
 }
 for(std::size_t k=0;k<=20;++k){
  std::uint64_t tail=0;for(std::size_t i=k;i<=20;++i)tail+=count[i];
  CHECK(std::abs(exact_upper_sign_tail_v3(20,k)-static_cast<double>(tail)/1048576.)<1e-14);
 }
 CHECK(exact_upper_sign_tail_v3(100,100)==std::ldexp(1.,-100));
 rejects([]{(void)exact_upper_sign_tail_v3(0,0);});
 rejects([]{(void)exact_upper_sign_tail_v3(20,21);});
 const std::vector<FrozenEdgeMemberV3> members{{1,1},{2,-1},{4,1}};
 using Row=std::vector<std::optional<double>>;
 std::vector<Row> effects{Row(20,1.),Row(20,-1.),Row(20,.0)};
 auto result=fixed_family_v3(members,effects,20,.2,.05,true);
 CHECK(result.complete&&result.members.size()==3);
 CHECK(result.members[0].state=="REJECTED_UNDER_DECLARED_MODEL");
 CHECK(result.members[1].state=="REJECTED_UNDER_DECLARED_MODEL");
 CHECK(result.members[2].state=="NOT_REJECTED");
 CHECK(result.members[0].holm_p==3./1048576.);
 CHECK(result.members[1].holm_p==3./1048576.);
 CHECK(result.members[2].raw_p==1.);
 effects[1][0]=std::nullopt;
 result=fixed_family_v3(members,effects,20,.2,.05,true);
 CHECK(result.members[1].state=="ABSTAIN_CENSORED"&&result.members[1].raw_p==1.);
 CHECK(result.members[0].holm_p==3./1048576.); // censored never shrinks denominator
 result=fixed_family_v3(members,effects,20,.2,.05,false);
 CHECK(result.members[0].state=="ABSTAIN_ASSUMPTIONS"&&result.members[0].raw_p==1.);
 effects[2].pop_back();
 result=fixed_family_v3(members,effects,20,.2,.05,true);
 CHECK(!result.complete);
 for(const auto& m:result.members)CHECK(m.state=="INCOMPLETE"&&m.raw_p==1.&&m.holm_p==1.);
 effects[2].push_back(.2);
 result=fixed_family_v3(members,effects,20,.2,.05,true);
 CHECK(result.members[2].positive==0); // exact threshold ties count against rejection
 auto duplicate=members;duplicate[2].discovery_pair=1;
 rejects([&]{(void)fixed_family_v3(duplicate,effects,20,.2,.05,true);});
 auto invalid=members;invalid[0].direction=0;
 rejects([&]{(void)fixed_family_v3(invalid,effects,20,.2,.05,true);});
 rejects([&]{(void)fixed_family_v3(members,effects,19,.2,.05,true);});
 rejects([&]{(void)fixed_family_v3(members,effects,20,.2,.5,true);});
 effects[0][0]=std::numeric_limits<double>::quiet_NaN();
 rejects([&]{(void)fixed_family_v3(members,effects,20,.2,.05,true);});
 std::cout<<"PASS exact sign-tail enumeration, fixed stopping, censored denominator and Holm\n";
}
