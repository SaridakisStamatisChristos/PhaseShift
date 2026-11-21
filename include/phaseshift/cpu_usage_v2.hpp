#pragma once
#include <cstdint>
#include <limits>
#include <optional>
#ifdef __linux__
#include <sys/resource.h>
#endif

namespace ps {
// CPU used by this process plus waited child processes during a single v2
// proposal/measurement window. This does not capture startup, un-reaped
// descendants or later reporting, and is not a whole-campaign CPU bill.
struct CpuSnapshotV2 {std::int64_t self_ns=0,waited_child_ns=0;};
inline std::optional<CpuSnapshotV2> cpu_snapshot_v2() {
#ifdef __linux__
 rusage self{},children{};
 if(getrusage(RUSAGE_SELF,&self)!=0||getrusage(RUSAGE_CHILDREN,&children)!=0)
  return std::nullopt;
 const auto nanos=[](const timeval& user,const timeval& sys)
    ->std::optional<std::int64_t>{
  if(user.tv_sec<0||sys.tv_sec<0||user.tv_usec<0||sys.tv_usec<0||
     user.tv_usec>=1000000||sys.tv_usec>=1000000)return std::nullopt;
  const long double value=
   (static_cast<long double>(user.tv_sec)+static_cast<long double>(sys.tv_sec))*1000000000.L+
   (static_cast<long double>(user.tv_usec)+static_cast<long double>(sys.tv_usec))*1000.L;
  if(value<0.L||value>static_cast<long double>(std::numeric_limits<std::int64_t>::max()))
   return std::nullopt;
  return static_cast<std::int64_t>(value);
 };
 auto a=nanos(self.ru_utime,self.ru_stime);
 auto b=nanos(children.ru_utime,children.ru_stime);
 if(!a||!b)return std::nullopt;
 return CpuSnapshotV2{*a,*b};
#else
 return std::nullopt;
#endif
}
inline std::optional<CpuSnapshotV2> cpu_delta_v2(
 const std::optional<CpuSnapshotV2>& earlier,
 const std::optional<CpuSnapshotV2>& later){
 if(!earlier||!later||later->self_ns<earlier->self_ns||
    later->waited_child_ns<earlier->waited_child_ns)return std::nullopt;
 return CpuSnapshotV2{later->self_ns-earlier->self_ns,
                       later->waited_child_ns-earlier->waited_child_ns};
}
} // namespace ps
