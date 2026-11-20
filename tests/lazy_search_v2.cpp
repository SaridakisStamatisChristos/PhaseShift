#include "phaseshift/lazy_search.hpp"
#include <cassert>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
int main(){
 using ps::LazySearchSpace;using ps::SparsePermutation;
 const LazySearchSpace tiny({{"x",{"a","b"}},{"y",{"1","2","3"}}});
 assert(tiny.cardinality()==6);
 assert(tiny.at(0)[0].second=="a"&&tiny.at(5)[1].second=="3");
 std::set<std::string> coords;
 for(std::uint64_t i=0;i<6;i++){const auto c=tiny.at(i);assert(coords.insert(c[0].second+c[1].second).second);}
 std::vector<std::string> labels;
 for(int i=0;i<1000;i++)labels.push_back(std::to_string(i));
 const LazySearchSpace huge({{"a",labels},{"b",labels},{"c",labels}});
 assert(huge.cardinality()==1000000000ULL);
 SparsePermutation p(huge.cardinality(),42),q(huge.cardinality(),42);
 std::set<std::uint64_t> selected;
 for(int i=0;i<512;i++){const auto x=p.next();assert(x==q.next());assert(selected.insert(x).second);}
 assert(p.resident_entries()<=512);
 SparsePermutation small(6,9);
 std::set<std::uint64_t> seen;
 while(small.has_next())assert(seen.insert(small.next()).second);
 assert(seen.size()==6);
 bool bad=false;
 try{(void)small.next();}catch(const ps::Error&){bad=true;}assert(bad);
 bad=false;try{(void)tiny.at(6);}catch(const ps::Error&){bad=true;}assert(bad);
 bad=false;try{(void)LazySearchSpace({{"a",{}}});}catch(const ps::Error&){bad=true;}assert(bad);
 bad=false;try{(void)LazySearchSpace({{"a",{"x","x"}}});}catch(const ps::Error&){bad=true;}assert(bad);
 bad=false;
 try{(void)LazySearchSpace({{"a",labels},{"b",labels},{"c",labels},{"d",labels},{"e",labels},{"f",labels},{"g",labels}});}
 catch(const ps::Error& e){bad=std::string(e.what()).find("CARDINALITY_OVERFLOW")!=std::string::npos;}
 assert(bad);
 return 0;
}
