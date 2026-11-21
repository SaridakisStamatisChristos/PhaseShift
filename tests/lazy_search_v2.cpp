#include "phaseshift/lazy_search.hpp"
#include <stdexcept>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
// Unlike REQUIRE(), REQUIRE always evaluates its expression in Release builds.
// Side-effecting test operations (such as next()) must never vanish under NDEBUG.
#define REQUIRE(expr) do { if (!(expr)) throw std::runtime_error("lazy_search_v2: " #expr); } while (false)
int main(){
 using ps::LazySearchSpace;using ps::SparsePermutation;
 const LazySearchSpace tiny({{"x",{"a","b"}},{"y",{"1","2","3"}}});
 REQUIRE(tiny.cardinality()==6);
 REQUIRE(tiny.at(0)[0].second=="a"&&tiny.at(5)[1].second=="3");
 std::set<std::string> coords;
 for(std::uint64_t i=0;i<6;i++){const auto c=tiny.at(i);REQUIRE(coords.insert(c[0].second+c[1].second).second);}
 std::vector<std::string> labels;
 for(int i=0;i<1000;i++)labels.push_back(std::to_string(i));
 const LazySearchSpace huge({{"a",labels},{"b",labels},{"c",labels}});
 REQUIRE(huge.cardinality()==1000000000ULL);
 SparsePermutation p(huge.cardinality(),42),q(huge.cardinality(),42);
 std::set<std::uint64_t> selected;
 for(int i=0;i<512;i++){const auto x=p.next();REQUIRE(x==q.next());REQUIRE(selected.insert(x).second);}
 REQUIRE(p.resident_entries()<=512);
 SparsePermutation small(6,9);
 std::set<std::uint64_t> seen;
 while(small.has_next())REQUIRE(seen.insert(small.next()).second);
 REQUIRE(seen.size()==6);
 bool bad=false;
 try{(void)small.next();}catch(const ps::Error&){bad=true;}REQUIRE(bad);
 bad=false;try{(void)tiny.at(6);}catch(const ps::Error&){bad=true;}REQUIRE(bad);
 bad=false;try{(void)LazySearchSpace({{"a",{}}});}catch(const ps::Error&){bad=true;}REQUIRE(bad);
 bad=false;try{(void)LazySearchSpace({{"a",{"x","x"}}});}catch(const ps::Error&){bad=true;}REQUIRE(bad);
 bad=false;
 try{(void)LazySearchSpace({{"a",labels},{"b",labels},{"c",labels},{"d",labels},{"e",labels},{"f",labels},{"g",labels}});}
 catch(const ps::Error& e){bad=std::string(e.what()).find("CARDINALITY_OVERFLOW")!=std::string::npos;}
 REQUIRE(bad);
 return 0;
}
