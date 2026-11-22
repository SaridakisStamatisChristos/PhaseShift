#include "phaseshift/algorithms.hpp"
#include <algorithm>
#include <limits>
#include <numeric>
#include <queue>
namespace ps {
std::vector<std::string> families(){return {"calibration","integer_array","topk","graph"};}
std::vector<std::string> algorithms_for(const std::string& family){
 if(family=="calibration")return {"calibration","calibration_null_v3","calibration_null_fault_v3"};
 if(family=="integer_array")return {"std_sort","quicksort_first_pivot","mergesort","insertion_sort"};
 if(family=="topk")return {"topk_full_sort","topk_heap","topk_nth"};
 if(family=="graph")return {"bellman_ford","dijkstra_heap","dijkstra_dense","zero_one_bfs"};
 return {};
}
struct Graph {std::size_t n;std::size_t src;std::vector<std::vector<std::pair<std::size_t,std::int64_t>>> adj;std::vector<std::tuple<std::size_t,std::size_t,std::int64_t>> edges;};
static Graph graph_from(const Case& c){
 auto v=decode_values(c.bytes);
 if(v.size()<3||v[0]<1||v[0]>4096||v[1]<0||v[1]>400000||v[2]<0||v[2]>=v[0]||v.size()!=3+static_cast<std::size_t>(v[1])*3)throw Error("bad graph encoding");
 Graph g;g.n=static_cast<std::size_t>(v[0]);g.src=static_cast<std::size_t>(v[2]);g.adj.resize(g.n);
 for(std::size_t i=3;i<v.size();i+=3){if(v[i]<0||v[i]>=v[0]||v[i+1]<0||v[i+1]>=v[0]||v[i+2]<0||v[i+2]>1000000)throw Error("invalid graph edge");auto a=static_cast<std::size_t>(v[i]),b=static_cast<std::size_t>(v[i+1]);auto w=v[i+2];g.edges.emplace_back(a,b,w);g.adj[a].emplace_back(b,w);}
 return g;
}
static constexpr std::int64_t INF=std::numeric_limits<std::int64_t>::max()/4;
static std::int64_t add_dist(std::int64_t d,std::int64_t w){return d>=INF||w>=INF-d?INF:d+w;}
static Result run_graph(const Case& c,const std::string& algorithm){
 auto g=graph_from(c);Result r;r.values.assign(g.n,INF);r.values[g.src]=0;
 if(algorithm=="bellman_ford"){
  for(std::size_t i=1;i<g.n;i++){bool changed=false;for(const auto& [a,b,w]:g.edges){r.work++;auto d=add_dist(r.values[a],w);if(d<r.values[b]){r.values[b]=d;changed=true;}}if(!changed)break;}
 }else if(algorithm=="dijkstra_dense"){
  std::vector<bool> used(g.n,false);
  for(std::size_t i=0;i<g.n;i++){std::size_t u=g.n;for(std::size_t v=0;v<g.n;v++){r.work++;if(!used[v]&&(u==g.n||r.values[v]<r.values[u]))u=v;}if(u==g.n||r.values[u]==INF)break;used[u]=true;for(const auto& [v,w]:g.adj[u]){r.work++;r.values[v]=std::min(r.values[v],add_dist(r.values[u],w));}}
 }else if(algorithm=="dijkstra_heap"){
  using Entry=std::pair<std::int64_t,std::size_t>;std::priority_queue<Entry,std::vector<Entry>,std::greater<>> q;q.emplace(0,g.src);
  while(!q.empty()){auto [dist,u]=q.top();q.pop();r.work++;if(dist!=r.values[u])continue;for(const auto& [v,w]:g.adj[u]){r.work++;auto d=add_dist(dist,w);if(d<r.values[v]){r.values[v]=d;q.emplace(d,v);}}}
 }else if(algorithm=="zero_one_bfs"){
  if(c.weight_class!="zero_one")throw Error("zero_one_bfs requires zero_one graph class");
  for(const auto& [a,b,w]:g.edges){(void)a;(void)b;if(w!=0&&w!=1)throw Error("edge weight outside {0,1}");}
  std::deque<std::pair<std::size_t,std::int64_t>> q;q.emplace_back(g.src,0);
  while(!q.empty()){auto [u,dist]=q.front();q.pop_front();r.work++;if(dist!=r.values[u])continue;
   for(const auto& [v,w]:g.adj[u]){r.work++;auto d=add_dist(dist,w);if(d<r.values[v]){r.values[v]=d;if(w==0)q.emplace_front(v,d);else q.emplace_back(v,d);}}}
 }else throw Error("unknown graph algorithm: "+algorithm);
 return r;
}
static Result run_sort(const Case& c,const std::string& algorithm){
 Result r;r.values=decode_values(c.bytes);auto& a=r.values;
 if(algorithm=="std_sort"){
  std::sort(a.begin(),a.end(),[&](std::int64_t x,std::int64_t y){r.work++;return x<y;});
 }else if(algorithm=="insertion_sort"){
  if(a.size()>4096)throw Error("insertion sort restricted to n<=4096");
  for(std::size_t i=1;i<a.size();i++){auto v=a[i];std::size_t j=i;while(j>0){r.work++;if(a[j-1]<=v)break;a[j]=a[j-1];j--;}a[j]=v;}
 }else if(algorithm=="mergesort"){
  std::vector<std::int64_t> tmp(a.size());for(std::size_t width=1;width<a.size();){for(std::size_t start=0;start<a.size();start+=2*width){const auto mid=std::min(a.size(),start+width),end=std::min(a.size(),start+2*width);std::size_t i=start,j=mid,k=start;while(i<mid&&j<end){r.work++;tmp[k++]=a[i]<=a[j]?a[i++]:a[j++];}while(i<mid)tmp[k++]=a[i++];while(j<end)tmp[k++]=a[j++];}a.swap(tmp);if(width>a.size()/2)break;width*=2;}
 }else if(algorithm=="quicksort_first_pivot"){
  if(a.size()>20000)throw Error("vulnerable baseline capped at n<=20000");
  std::vector<std::pair<std::size_t,std::size_t>> stack;if(a.size()>1)stack.emplace_back(0,a.size()-1);
  while(!stack.empty()){
   auto [lo,hi]=stack.back();stack.pop_back();if(lo>=hi)continue;
   auto pivot=a[lo];std::size_t i=lo+1;for(std::size_t j=lo+1;j<=hi;j++){r.work++;if(a[j]<pivot){std::swap(a[j],a[i]);i++;}}
   const auto mid=i-1;std::swap(a[lo],a[mid]);
   if(mid>lo+1)stack.emplace_back(lo,mid-1);
   if(mid+1<hi)stack.emplace_back(mid+1,hi);
  }
 }else throw Error("unknown sort algorithm: "+algorithm);
 return r;
}
static Result run_topk(const Case& c,const std::string& algorithm){
 Result r;auto v=decode_values(c.bytes);const auto k=static_cast<std::size_t>(c.k);if(k>v.size())throw Error("topk k>n");if(k==0)return r;
 if(algorithm=="topk_full_sort"){
  std::sort(v.begin(),v.end(),[&](auto a,auto b){r.work++;return a>b;});r.values.assign(v.begin(),v.begin()+static_cast<std::ptrdiff_t>(k));
 }else if(algorithm=="topk_heap"){
  std::priority_queue<std::int64_t,std::vector<std::int64_t>,std::greater<>> q;
  for(auto x:v){r.work++;if(q.size()<k)q.push(x);else if(x>q.top()){q.pop();q.push(x);}}
  while(!q.empty()){r.values.push_back(q.top());q.pop();}std::reverse(r.values.begin(),r.values.end());
 }else if(algorithm=="topk_nth"){
  if(k<v.size())std::nth_element(v.begin(),v.begin()+static_cast<std::ptrdiff_t>(k),v.end(),[&](auto a,auto b){r.work++;return a>b;});
  v.resize(k);std::sort(v.begin(),v.end(),[&](auto a,auto b){r.work++;return a>b;});r.values=std::move(v);
 }else throw Error("unknown topk algorithm: "+algorithm);
 return r;
}
Result execute(const Case& c,const std::string& algorithm){
 if(c.family=="calibration"){
  if(algorithm!="calibration"&&algorithm!="calibration_null_v3"&&algorithm!="calibration_null_fault_v3")
   throw Error("invalid calibration algorithm");
  auto a=decode_values(c.bytes);if(a.size()!=3||a[0]<0||a[1]<1||a[2]<0||a[2]>1)throw Error("invalid calibration payload");
  auto work=a[2]==1?1000ULL:(a[0]<a[1]?static_cast<std::uint64_t>(a[0]+10):static_cast<std::uint64_t>(a[0]+10000));
  if(algorithm!="calibration"){
   // Explicit synthetic null: common seed-level log-work shock plus a
   // symmetric coordinate-local shock of unequal amplitude. Log-work
   // center is constant; this does NOT assert a null for elapsed time.
   std::uint64_t common=c.seed^std::uint64_t{0xf20c938bda5716a1ULL};
   std::uint64_t local=c.seed^(static_cast<std::uint64_t>(a[0])*0x9e3779b97f4a7c15ULL)^
      (static_cast<std::uint64_t>(a[1])*0x94d049bb133111ebULL)^0xb58154fca3ad590dULL;
   const auto cs=splitmix64(common);
   const auto ls=splitmix64(local);
   const auto magnitude=(ls&15ULL)==0?3:static_cast<int>(a[0]%3);
   const int exponent=6+((cs&1ULL)?1:-1)+((ls&16ULL)?magnitude:-magnitude);
   work=64ULL<<static_cast<unsigned>(exponent); // bounded 256..65536
   if(algorithm=="calibration_null_fault_v3"&&a[0]%4==3&&(splitmix64(local)&7ULL)==0)
    a[2]=2; // genuine independent worker oracle disagreement, not fake status
  }
  volatile std::uint64_t sink=0;for(std::uint64_t i=0;i<work;i++)sink=sink^i;
  (void)sink;return {{a[0],a[1],a[2]},work};
 }
 if(c.family=="integer_array")return run_sort(c,algorithm);
 if(c.family=="topk")return run_topk(c,algorithm);
 if(c.family=="graph")return run_graph(c,algorithm);
 throw Error("unknown family");
}
Result oracle(const Case& c){
 if(c.family=="calibration")return execute(c,"calibration");
 if(c.family=="integer_array"){Result r;r.values=decode_values(c.bytes);std::stable_sort(r.values.begin(),r.values.end());return r;}
 if(c.family=="topk")return run_topk(c,"topk_full_sort");
 if(c.family=="graph")return run_graph(c,"bellman_ford");
 throw Error("unknown family");
}
}
