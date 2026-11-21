#include "phaseshift/cpu_usage_v2.hpp"
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("cpu_usage_v2: " #x);}while(false)
int main(){
 using namespace ps;
 const CpuSnapshotV2 a{100,200},b{400,900};
 const auto delta=cpu_delta_v2(a,b);
 CHECK(delta&&delta->self_ns==300&&delta->waited_child_ns==700);
 CHECK(!cpu_delta_v2(b,a));
 CHECK(!cpu_delta_v2(std::nullopt,b));
 CHECK(!cpu_delta_v2(a,std::nullopt));
 const auto before=cpu_snapshot_v2(),after=cpu_snapshot_v2();
#ifdef __linux__
 CHECK(before.has_value()&&after.has_value());
 CHECK(cpu_delta_v2(before,after).has_value());
#else
 CHECK(!before.has_value()&&!after.has_value());
#endif
 return 0;
}
