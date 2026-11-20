#include "phaseshift/reproducer.hpp"
#include "phaseshift/algorithms.hpp"
#include "phaseshift/store.hpp"
#include <algorithm>
#include <filesystem>
#include <limits>
#include <sstream>
namespace ps {
namespace {
std::string integer_literal(std::int64_t x){
 if(x==std::numeric_limits<std::int64_t>::min())return "(std::numeric_limits<std::int64_t>::min)()";
 if(x==std::numeric_limits<std::int64_t>::max())return "(std::numeric_limits<std::int64_t>::max)()";
 return std::to_string(x)+"LL";
}
std::string vector_literal(const std::vector<std::int64_t>& v){
 std::ostringstream out;out<<"{";
 for(std::size_t i=0;i<v.size();i++){
  if(i)out<<',';
  if(i%12==0)out<<"\n  ";
  out<<integer_literal(v[i]);
 }
 out<<"\n }";return out.str();
}
}
std::string standalone_reproducer(const Case& input,const std::string& algorithm){
 const auto builtins=algorithms_for(input.family);
 if(std::find(builtins.begin(),builtins.end(),algorithm)==builtins.end())
  throw Error("standalone reproducers require a built-in algorithm of the exact case family");
 const auto c=make_case(input);
 if(c.bytes.size()>2ULL*1024*1024)throw Error("standalone embedded C++ source capped at 2 MiB of input; export a verified evidence bundle instead");
 if(c.bytes!=input.bytes||c.id!=input.id)throw Error("reproducer requires canonical, unchanged case bytes and SHA identity");
 const auto expected=oracle(c).values;
 const auto actual=execute(c,algorithm).values;
 if(actual!=expected)throw Error("cannot generate expected-correctness reproducer for failing built-in algorithm");
 std::ostringstream source;
 source<<R"PSRC(// PHASESHIFT standalone, verified-input witness. C++20 standard library only.
// The values, expected answer and algorithm ID were frozen at export time.
// One observed timing here is diagnostic only; it cannot confirm a boundary.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
using Int=std::int64_t;
using Values=std::vector<Int>;
constexpr Int INF=(std::numeric_limits<Int>::max)()/4;
Int add_dist(Int d,Int w){return d>=INF||w>=INF-d?INF:d+w;}
Values solve(const Values& bytes,const std::string& name,std::size_t k,bool zero_one){
 if(name=="calibration"){
  if(bytes.size()!=3)throw std::runtime_error("calibration payload");
  return bytes;
 }
 if(name=="std_sort"||name=="quicksort_first_pivot"||name=="mergesort"||name=="insertion_sort"){
  Values a=bytes;
  if(name=="std_sort")std::sort(a.begin(),a.end());
  else if(name=="insertion_sort"){
   if(a.size()>4096)throw std::runtime_error("insertion cap");
   for(std::size_t i=1;i<a.size();i++){auto v=a[i];auto j=i;while(j>0&&a[j-1]>v){a[j]=a[j-1];j--;}a[j]=v;}
  }else if(name=="mergesort"){
   Values tmp(a.size());
   for(std::size_t width=1;width<a.size();){
    for(std::size_t start=0;start<a.size();start+=2*width){
     auto mid=std::min(a.size(),start+width),end=std::min(a.size(),start+2*width);
     std::size_t i=start,j=mid,t=start;
     while(i<mid&&j<end)tmp[t++]=a[i]<=a[j]?a[i++]:a[j++];
     while(i<mid)tmp[t++]=a[i++];
     while(j<end)tmp[t++]=a[j++];
    }
    a.swap(tmp);if(width>a.size()/2)break;width*=2;
   }
  }else{
   if(a.size()>20000)throw std::runtime_error("quicksort cap");
   std::vector<std::pair<std::size_t,std::size_t>> stack;
   if(a.size()>1)stack.emplace_back(0,a.size()-1);
   while(!stack.empty()){
    auto [lo,hi]=stack.back();stack.pop_back();if(lo>=hi)continue;
    auto pivot=a[lo];std::size_t i=lo+1;
    for(std::size_t j=lo+1;j<=hi;j++)if(a[j]<pivot){std::swap(a[j],a[i]);i++;}
    auto mid=i-1;std::swap(a[lo],a[mid]);
    if(mid>lo+1)stack.emplace_back(lo,mid-1);
    if(mid+1<hi)stack.emplace_back(mid+1,hi);
   }
  }
  return a;
 }
 if(name=="topk_full_sort"||name=="topk_heap"||name=="topk_nth"){
  if(k>bytes.size())throw std::runtime_error("invalid k");
  if(k==0)return {};
  Values a=bytes;
  if(name=="topk_full_sort"){
   std::sort(a.begin(),a.end(),std::greater<Int>{});a.resize(k);return a;
  }
  if(name=="topk_heap"){
   std::priority_queue<Int,Values,std::greater<Int>> heap;
   for(auto v:a){if(heap.size()<k)heap.push(v);else if(v>heap.top()){heap.pop();heap.push(v);}}
   Values out;while(!heap.empty()){out.push_back(heap.top());heap.pop();}
   std::reverse(out.begin(),out.end());return out;
  }
  if(k<a.size())std::nth_element(a.begin(),a.begin()+static_cast<std::ptrdiff_t>(k),a.end(),std::greater<Int>{});
  a.resize(k);std::sort(a.begin(),a.end(),std::greater<Int>{});return a;
 }
 if(name=="bellman_ford"||name=="dijkstra_heap"||name=="dijkstra_dense"||name=="zero_one_bfs"){
  if(bytes.size()<3||bytes[0]<1||bytes[1]<0||bytes[2]<0||bytes[2]>=bytes[0]||
     bytes.size()!=3+static_cast<std::size_t>(bytes[1])*3)throw std::runtime_error("graph header");
  const auto n=static_cast<std::size_t>(bytes[0]),source=static_cast<std::size_t>(bytes[2]);
  std::vector<std::vector<std::pair<std::size_t,Int>>> adj(n);
  std::vector<std::tuple<std::size_t,std::size_t,Int>> edges;
  for(std::size_t i=3;i<bytes.size();i+=3){
   if(bytes[i]<0||bytes[i]>=bytes[0]||bytes[i+1]<0||bytes[i+1]>=bytes[0]||bytes[i+2]<0)throw std::runtime_error("invalid edge");
   const auto a=static_cast<std::size_t>(bytes[i]),b=static_cast<std::size_t>(bytes[i+1]);
   const auto w=bytes[i+2];
   edges.emplace_back(a,b,w);adj[a].emplace_back(b,w);
   if(name=="zero_one_bfs"&&(!zero_one||w>1))throw std::runtime_error("zero-one domain");
  }
  Values dist(n,INF);dist[source]=0;
  if(name=="bellman_ford"){
   for(std::size_t i=1;i<n;i++){
    bool changed=false;for(const auto& [a,b,w]:edges){auto d=add_dist(dist[a],w);if(d<dist[b]){dist[b]=d;changed=true;}}
    if(!changed)break;
   }
  }else if(name=="dijkstra_dense"){
   std::vector<bool> used(n,false);
   for(std::size_t i=0;i<n;i++){
    std::size_t u=n;for(std::size_t v=0;v<n;v++)if(!used[v]&&(u==n||dist[v]<dist[u]))u=v;
    if(u==n||dist[u]==INF)break;
    used[u]=true;
    for(const auto& [v,w]:adj[u])dist[v]=std::min(dist[v],add_dist(dist[u],w));
   }
  }else if(name=="dijkstra_heap"){
   using Entry=std::pair<Int,std::size_t>;
   std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> q;q.emplace(0,source);
   while(!q.empty()){
    auto [d,u]=q.top();q.pop();if(d!=dist[u])continue;
    for(const auto& [v,w]:adj[u]){auto next=add_dist(d,w);if(next<dist[v]){dist[v]=next;q.emplace(next,v);}}
   }
  }else{
   std::deque<std::pair<std::size_t,Int>> q;q.emplace_back(source,0);
   while(!q.empty()){
    auto [u,d]=q.front();q.pop_front();if(d!=dist[u])continue;
    for(const auto& [v,w]:adj[u]){auto next=add_dist(d,w);if(next<dist[v]){
     dist[v]=next;if(w==0)q.emplace_front(v,next);else q.emplace_back(v,next);
    }}
   }
  }
  return dist;
 }
 throw std::runtime_error("unknown algorithm");
}
int main(){
 try{
)PSRC";
 source<<"  const Values input="<<vector_literal(decode_values(c.bytes))<<";\n";
 source<<"  const Values expected="<<vector_literal(expected)<<";\n";
 source<<"  const std::string algorithm=\""<<algorithm<<"\";\n";
 source<<"  constexpr std::size_t k="<<static_cast<std::uint64_t>(c.k)<<"ULL;\n";
 source<<"  constexpr std::uint64_t expected_work="<<static_cast<std::uint64_t>(execute(c,algorithm).work)<<"ULL;\n";
 source<<"  constexpr bool zero_one="<<(c.weight_class=="zero_one"?"true":"false")<<";\n";
 source<<"  // Input SHA-256: "<<sha256(c.bytes)<<"\n  // Case ID: "<<c.id<<"\n";
 source<<R"PSRC(  const auto start=std::chrono::steady_clock::now();
  const Values observed=solve(input,algorithm,k,zero_one);
  const auto elapsed_ns=std::chrono::duration_cast<std::chrono::nanoseconds>(
   std::chrono::steady_clock::now()-start).count();
  if(observed!=expected){std::cerr<<"FAIL: output differs from frozen oracle\n";return 1;}
  if(algorithm=="calibration"){
   const auto x=input[0],threshold=input[1];
   const auto work=input[2]==1?1000ULL:static_cast<std::uint64_t>(
      x<threshold?x+10:x+10000);
   if(work!=expected_work){std::cerr<<"FAIL: calibration work count\n";return 1;}
  }
  std::cout<<"PASS: standalone case/oracle agreement, "<<observed.size()
           <<" output values; illustrative single run "<<elapsed_ns<<" ns (not a benchmark)\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 2;}
}
)PSRC";
 return source.str();
}
void reproduce_case(const std::string& run,const std::string& case_id,
                    const std::string& algorithm,const std::string& out){
 if(out.empty())throw Error("reproduce requires --out source.cpp");
 const auto target=std::filesystem::path(out);
 if(target.extension()!=".cpp")throw Error("standalone reproducer output must end in .cpp");
 if(std::filesystem::exists(target))throw Error("refusing to overwrite existing reproducer file");
 Store store(run+"/run.sqlite",true);
 std::optional<Case> selected;
 for(const auto& c:store.cases())if(c.id==case_id||c.id.rfind(case_id,0)==0){
  if(selected)throw Error("ambiguous reproducer case-id prefix");
  selected=c;
 }
 if(!selected)throw Error("case-id not found in evidence database");
 const auto src=standalone_reproducer(*selected,algorithm);
 if(!target.parent_path().empty())std::filesystem::create_directories(target.parent_path());
 write_file(out,src);
}
}
