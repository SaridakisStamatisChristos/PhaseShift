#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <json-c/json.h>
namespace ps {
using Bytes = std::vector<std::uint8_t>;
struct Error : std::runtime_error { using std::runtime_error::runtime_error; };
struct Json {
 json_object* p=nullptr;
 Json():p(json_object_new_object()){} explicit Json(json_object* v):p(v){}
 Json(const Json& o):p(json_object_get(o.p)){} Json(Json&& o) noexcept:p(o.p){o.p=nullptr;}
 Json& operator=(Json o) noexcept {std::swap(p,o.p);return *this;}
 ~Json(){if(p)json_object_put(p);}
 std::string str() const;
 static Json parse(const std::string& s);
 static Json file(const std::string& path);
 json_object* get(const char* k) const;
 std::string s(const char* k,const std::string& def="") const;
 std::int64_t i(const char* k,std::int64_t def=0) const;
 double d(const char* k,double def=0.0) const;
 bool b(const char* k,bool def=false) const;
 void put(const char* k,Json&& v);
 void put(const char* k,const std::string& v);
 void put(const char* k,const char* v);
 void put(const char* k,std::int64_t v);
 void put(const char* k,double v);
 void put(const char* k,bool v);
};
std::string sha256(const Bytes& b);
std::string sha256(const std::string& s);
std::string sha256_file(const std::string& path);
Bytes hex_decode(const std::string& s);
std::string hex_encode(const Bytes& b);
std::uint64_t splitmix64(std::uint64_t& state);
std::uint64_t randbelow(std::uint64_t& state,std::uint64_t bound);
std::string now_utc();
void write_file(const std::string& path,const std::string& content);
std::string read_file(const std::string& path);
struct Case {
 std::string family,distribution="uniform",weight_class="general";
 std::uint64_t seed=0;
 std::int64_t generator_version=2;
 std::int64_t value_range=1000000,unique_count=0,run_length=17;
 std::int64_t duplicate_ppm=-1,disorder_ppm=10000,sign_ppm=500000;
 std::int64_t density_ppm=-1,degree_skew_ppm=0,community_count=1,max_weight=100;
 bool directed=true;
 std::string connectivity="none";
 std::int64_t x=0,n=32,k=8,vertices=16,edges=32,source=0,threshold=8;
 bool negative=false;
 Bytes bytes;
 std::string id;
};
std::string case_params(const Case& c);
void validate_case_schema(const Case& params,std::uint64_t max_bytes=64ULL*1024*1024);
Case make_case(const Case& params,std::uint64_t max_bytes=64ULL*1024*1024);
Case decode_case(const std::string& family,const std::string& params,std::uint64_t seed,const Bytes& bytes);
std::vector<std::int64_t> decode_values(const Bytes& b);
Bytes encode_values(const std::vector<std::int64_t>& v);
std::string environment();
}
