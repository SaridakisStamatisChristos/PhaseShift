#include "phaseshift/core.hpp"
#include <openssl/evp.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <sys/utsname.h>
#include <thread>
#include <unordered_set>
namespace ps {
std::string Json::str() const { return p?json_object_to_json_string_ext(p,JSON_C_TO_STRING_PLAIN):"null"; }
Json Json::parse(const std::string& s){
 json_tokener* tok=json_tokener_new();if(!tok)throw Error("cannot allocate JSON parser");
 json_object* v=json_tokener_parse_ex(tok,s.data(),static_cast<int>(s.size()));
 auto err=json_tokener_get_error(tok);auto end=json_tokener_get_parse_end(tok);
 json_tokener_free(tok);
 if(err!=json_tokener_success || end!=s.size() || !v){if(v)json_object_put(v);throw Error("invalid JSON document");}
 return Json(v);
}
Json Json::file(const std::string& path){return parse(read_file(path));}
json_object* Json::get(const char* k) const {json_object* v=nullptr;return p&&json_object_object_get_ex(p,k,&v)?v:nullptr;}
std::string Json::s(const char* k,const std::string& def) const {auto* v=get(k);return v&&json_object_get_type(v)==json_type_string?json_object_get_string(v):def;}
std::int64_t Json::i(const char* k,std::int64_t def) const {auto* v=get(k);return v&&json_object_get_type(v)==json_type_int?json_object_get_int64(v):def;}
double Json::d(const char* k,double def) const {auto* v=get(k);return v&&(json_object_get_type(v)==json_type_double||json_object_get_type(v)==json_type_int)?json_object_get_double(v):def;}
bool Json::b(const char* k,bool def) const {auto* v=get(k);return v&&json_object_get_type(v)==json_type_boolean?json_object_get_boolean(v)!=0:def;}
void Json::put(const char* k,Json&& v){json_object_object_add(p,k,v.p);v.p=nullptr;}
void Json::put(const char* k,const std::string& v){json_object_object_add(p,k,json_object_new_string_len(v.data(),static_cast<int>(v.size())));}
void Json::put(const char* k,const char* v){put(k,std::string(v));}
void Json::put(const char* k,std::int64_t v){json_object_object_add(p,k,json_object_new_int64(v));}
void Json::put(const char* k,double v){if(!std::isfinite(v))throw Error("non-finite JSON number");json_object_object_add(p,k,json_object_new_double(v));}
void Json::put(const char* k,bool v){json_object_object_add(p,k,json_object_new_boolean(v));}
std::string read_file(const std::string& path){std::ifstream f(path,std::ios::binary);if(!f)throw Error("cannot open "+path);std::ostringstream o;o<<f.rdbuf();return o.str();}
void write_file(const std::string& path,const std::string& content){const std::filesystem::path p(path);if(!p.parent_path().empty())std::filesystem::create_directories(p.parent_path());std::ofstream f(path,std::ios::binary|std::ios::trunc);if(!f||!(f<<content))throw Error("cannot write "+path);}
std::string sha256(const Bytes& b){
 EVP_MD_CTX* ctx=EVP_MD_CTX_new();if(!ctx)throw Error("SHA-256 allocation failed");
 unsigned char digest[EVP_MAX_MD_SIZE]={};unsigned int n=0;
 const bool ok=EVP_DigestInit_ex(ctx,EVP_sha256(),nullptr)==1&&EVP_DigestUpdate(ctx,b.data(),b.size())==1&&EVP_DigestFinal_ex(ctx,digest,&n)==1;
 EVP_MD_CTX_free(ctx);if(!ok||n!=32)throw Error("SHA-256 failed");
 std::ostringstream out;out<<std::hex<<std::setfill('0');for(unsigned i=0;i<n;i++)out<<std::setw(2)<<static_cast<unsigned>(digest[i]);return out.str();
}
std::string sha256(const std::string& s){return sha256(Bytes(s.begin(),s.end()));}
std::string sha256_file(const std::string& path){
 std::ifstream input(path,std::ios::binary);if(!input)throw Error("SHA256 cannot open file: "+path);
 EVP_MD_CTX* ctx=EVP_MD_CTX_new();if(!ctx)throw Error("SHA-256 allocation failed");
 unsigned char digest[EVP_MAX_MD_SIZE]{};unsigned n=0;
 if(EVP_DigestInit_ex(ctx,EVP_sha256(),nullptr)!=1){EVP_MD_CTX_free(ctx);throw Error("SHA-256 init failed");}
 std::array<char,65536> buffer{};
 while(input){input.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));auto size=input.gcount();
  if(size>0&&EVP_DigestUpdate(ctx,buffer.data(),static_cast<std::size_t>(size))!=1){EVP_MD_CTX_free(ctx);throw Error("SHA256 update failed");}
 }
 if(!input.eof()){EVP_MD_CTX_free(ctx);throw Error("SHA256 file read failed: "+path);}
 if(EVP_DigestFinal_ex(ctx,digest,&n)!=1||n!=32){EVP_MD_CTX_free(ctx);throw Error("SHA256 final failed");}
 EVP_MD_CTX_free(ctx);std::ostringstream out;out<<std::hex<<std::setfill('0');for(unsigned i=0;i<n;i++)out<<std::setw(2)<<static_cast<unsigned>(digest[i]);return out.str();
}

std::string hex_encode(const Bytes& b){static constexpr char abc[]="0123456789abcdef";std::string s;s.reserve(b.size()*2);for(auto x:b){s.push_back(abc[x>>4]);s.push_back(abc[x&15]);}return s;}
Bytes hex_decode(const std::string& s){if(s.size()%2)throw Error("invalid hex byte count");Bytes b;b.reserve(s.size()/2);auto digit=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;};for(std::size_t i=0;i<s.size();i+=2){int a=digit(s[i]),d=digit(s[i+1]);if(a<0||d<0)throw Error("invalid hex digit");b.push_back(static_cast<std::uint8_t>(a*16+d));}return b;}
std::uint64_t splitmix64(std::uint64_t& s){std::uint64_t z=(s+=0x9e3779b97f4a7c15ULL);z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;z=(z^(z>>27))*0x94d049bb133111ebULL;return z^(z>>31);}
std::uint64_t randbelow(std::uint64_t& s,std::uint64_t bound){if(!bound)throw Error("random bound zero");const auto threshold=static_cast<std::uint64_t>(-bound)%bound;for(;;){auto r=splitmix64(s);if(r>=threshold)return r%bound;}}
Bytes encode_values(const std::vector<std::int64_t>& v){Bytes b;b.reserve(v.size()*8);for(auto x:v){auto n=static_cast<std::uint64_t>(x);for(int j=0;j<8;j++)b.push_back(static_cast<std::uint8_t>((n>>(8*j))&255));}return b;}
std::vector<std::int64_t> decode_values(const Bytes& b){if(b.size()%8)throw Error("invalid little-endian int64 payload");std::vector<std::int64_t> v;v.reserve(b.size()/8);for(std::size_t i=0;i<b.size();i+=8){std::uint64_t x=0;for(int j=0;j<8;j++)x|=static_cast<std::uint64_t>(b[i+static_cast<std::size_t>(j)])<<(8*j);v.push_back(static_cast<std::int64_t>(x));}return v;}
std::string case_params(const Case& c){
 Json j;j.put("distribution",c.distribution);j.put("weight_class",c.weight_class);j.put("x",c.x);j.put("n",c.n);j.put("k",c.k);j.put("vertices",c.vertices);j.put("edges",c.edges);j.put("source",c.source);j.put("threshold",c.threshold);j.put("negative",c.negative);
 if(c.generator_version>=2){
  j.put("generator_version",c.generator_version);
  j.put("value_range",c.value_range);j.put("unique_count",c.unique_count);j.put("run_length",c.run_length);
  j.put("duplicate_ppm",c.duplicate_ppm);j.put("disorder_ppm",c.disorder_ppm);j.put("sign_ppm",c.sign_ppm);
  j.put("density_ppm",c.density_ppm);j.put("degree_skew_ppm",c.degree_skew_ppm);
  j.put("community_count",c.community_count);j.put("max_weight",c.max_weight);
  j.put("directed",c.directed);j.put("connectivity",c.connectivity);
  if(c.generator_version==3)j.put("literal_hex",c.literal_hex);
 }
 return j.str();
}
void validate_case_schema(const Case& c,std::uint64_t max_bytes){
 if(c.generator_version!=1&&c.generator_version!=2&&c.generator_version!=3)throw Error("invalid generator version");
 /* An explicitly replayable, byte-literal v3 witness is permitted ONLY for
    schema-aware minimization. This is not a morphology PRNG generator. The
    64 KiB cap prevents unbounded hexadecimal witness metadata in SQLite. */
 if(c.generator_version==3){
  const auto limit=std::min<std::uint64_t>(max_bytes,64ULL*1024);
  if(c.literal_hex.size()>2*limit||c.literal_hex.size()%2)throw Error("literal v3 witness input exceeds bound or has odd hex length");
  const auto bytes=hex_decode(c.literal_hex);
  if(bytes.size()%8)throw Error("literal v3 input is not an int64 stream");
  if(c.family=="integer_array"||c.family=="topk"){
   if(c.distribution!="literal"||c.n<0||c.n>50000||bytes.size()!=static_cast<std::uint64_t>(c.n)*8||
      (c.family=="topk"&&(c.k<0||c.k>c.n)))throw Error("literal array shape or Top-K contract invalid");
   return;
  }
  if(c.family=="graph"){
   const auto data=decode_values(bytes);
   const auto arc_count=static_cast<std::uint64_t>(c.edges)*(c.directed?1ULL:2ULL);
   if(data.size()<3||c.vertices<1||c.vertices>4096||c.source<0||c.source>=c.vertices||
      c.edges<0||c.edges>200000||arc_count>400000||data[0]!=c.vertices||
      data[1]!=static_cast<std::int64_t>(arc_count)||data[2]!=c.source||
      data.size()!=3+static_cast<std::size_t>(arc_count)*3)throw Error("literal graph header/shape invalid");
   if(c.weight_class!="general"&&c.weight_class!="zero_one")throw Error("literal graph weight class invalid");
   // For undirected v3 witnesses an edge must have exactly two reciprocal
   // arcs with the same weight; no parallel edges or self loops. This is the
   // same topology contract as the seeded v2 undirected generator.
   std::map<std::pair<std::int64_t,std::int64_t>,std::pair<std::int64_t,unsigned>> reciprocal;
   for(std::size_t i=3;i<data.size();i+=3){
    const auto from=data[i],to=data[i+1],weight=data[i+2];
    if(from<0||from>=c.vertices||to<0||to>=c.vertices||weight<0||weight>1000000||
       (c.weight_class=="zero_one"&&weight>1))throw Error("literal graph contains invalid arc/weight");
    if(!c.directed){
     if(from==to)throw Error("literal undirected graph cannot contain self loops");
     const auto key=std::minmax(from,to);
     auto& record=reciprocal[{key.first,key.second}];
     const auto flag=from<to?1U:2U;
     if((record.second&flag)!=0|| (record.second!=0&&record.first!=weight))
      throw Error("literal undirected arcs must be unique reciprocal equal-weight pairs");
     record={weight,record.second|flag};
    }
   }
   if(!c.directed){
    if(reciprocal.size()!=static_cast<std::size_t>(c.edges))throw Error("literal undirected edge count mismatch");
    for(const auto& [edge,record]:reciprocal){(void)edge;
     if(record.second!=3U)throw Error("literal undirected edge lacks reciprocal arc");
    }
   }
   return;
  }
  throw Error("literal v3 witness only supports arrays, Top-K and graphs");
 }
 if(c.family=="calibration"){
  if(c.x<0||c.x>1000000||c.threshold<1||c.threshold>1000000||max_bytes<24)throw Error("invalid calibration schema/budget");
  return;
 }
 if(c.family=="integer_array"||c.family=="topk"){
  if(c.n<0||c.n>50000||(c.family=="topk"&&(c.k<0||c.k>c.n))||static_cast<std::uint64_t>(c.n)>max_bytes/8)
   throw Error("invalid array dimensions, Top-K k or byte limit");
  static const std::vector<std::string> distributions={"uniform","ascending","descending","nearly_sorted","organ_pipe","sawtooth","alternating","all_equal","heavy_duplicate","few_unique","zipf_like"};
  if(std::find(distributions.begin(),distributions.end(),c.distribution)==distributions.end())throw Error("unknown array distribution");
  if(c.generator_version==2){
   if(c.value_range<1||c.value_range>1000000000||c.run_length<1||c.run_length>50000||
      c.duplicate_ppm< -1||c.duplicate_ppm>1000000||c.disorder_ppm<0||c.disorder_ppm>1000000||
      c.sign_ppm<0||c.sign_ppm>1000000||c.unique_count<0||c.unique_count>50000)throw Error("invalid v2 array parameters");
   if(c.unique_count!=0&&c.distribution!="few_unique")throw Error("unique_count requires few_unique distribution");
   if(c.distribution=="few_unique"&&c.unique_count>std::min<std::int64_t>(c.n,2*c.value_range+1))throw Error("unique_count exceeds array domain");
  }
  return;
 }
 if(c.family=="graph"){
  if(c.vertices<1||c.vertices>4096||c.edges<0||c.edges>200000||c.source<0||c.source>=c.vertices)
   throw Error("invalid graph dimensions/source");
  if(c.weight_class!="zero_one"&&c.weight_class!="general")throw Error("invalid weight class");
  const auto vertices=static_cast<std::uint64_t>(c.vertices);
  if(c.generator_version==1){
   if(static_cast<std::uint64_t>(c.edges)>vertices*(vertices-1)||(3ULL+static_cast<std::uint64_t>(c.edges)*3)*8>max_bytes)throw Error("legacy graph edges exceed budget");
  }else{
   if(c.community_count<1||c.community_count>c.vertices||c.degree_skew_ppm<0||c.degree_skew_ppm>1000000||
      c.density_ppm< -1||c.density_ppm>1000000||c.max_weight<0||c.max_weight>1000000)throw Error("invalid graph morphology parameters");
   if(c.connectivity!="none"&&c.connectivity!="weak"&&c.connectivity!="strong")throw Error("invalid connectivity class");
   if(!c.directed&&c.connectivity=="strong")throw Error("strong connectivity requires directed graph");
   const auto capacity=c.directed?vertices*(vertices-1):vertices*(vertices-1)/2;
   auto count=c.density_ppm<0?static_cast<std::uint64_t>(c.edges):(capacity*static_cast<std::uint64_t>(c.density_ppm)+500000)/1000000;
   const auto minimum=c.connectivity=="strong"?(vertices>1?vertices:0):(c.connectivity=="weak"?vertices-1:0);
   if(count>capacity||count>200000||count<minimum)throw Error("graph topology cannot satisfy edges/connectivity");
   const auto arcs=count*(c.directed?1ULL:2ULL);
   if(arcs>400000||(3ULL+arcs*3)*8>max_bytes)throw Error("graph arc bytes exceed memory budget");
  }
  return;
 }
 throw Error("unknown case family");
}
static void synthesize_v2(Case& c,std::vector<std::int64_t>& v,std::uint64_t& state,std::uint64_t max_bytes){
 if(c.family=="calibration"){
  if(c.x<0||c.x>1000000||c.threshold<1||c.threshold>1000000)throw Error("invalid calibration x/threshold");
  v={c.x,c.threshold,c.negative?1:0};return;
 }
 if(c.family=="integer_array"||c.family=="topk"){
  if(c.n<0||c.n>50000||(c.family=="topk"&&(c.k<0||c.k>c.n)))throw Error("invalid array length or Top-K k");
  if(static_cast<std::uint64_t>(c.n)>max_bytes/8)throw Error("array exceeds case-byte budget");
  if(c.value_range<1||c.value_range>1000000000||c.run_length<1||c.run_length>50000||
     c.duplicate_ppm< -1||c.duplicate_ppm>1000000||c.disorder_ppm<0||c.disorder_ppm>1000000||
     c.sign_ppm<0||c.sign_ppm>1000000||c.unique_count<0||c.unique_count>50000)
   throw Error("invalid typed array morphology controls");
  static const std::vector<std::string> valid={"uniform","ascending","descending","nearly_sorted","organ_pipe","sawtooth","alternating","all_equal","heavy_duplicate","few_unique","zipf_like"};
  if(std::find(valid.begin(),valid.end(),c.distribution)==valid.end())throw Error("unknown distribution: "+c.distribution);
  if(c.unique_count!=0&&c.distribution!="few_unique")throw Error("unique_count only applies to few_unique");
  if(c.distribution=="few_unique"&&c.unique_count>c.n)throw Error("unique_count must not exceed n");
  if(c.distribution=="few_unique"&&c.unique_count>2*c.value_range+1)throw Error("unique_count exceeds value_range domain");
  v.resize(static_cast<std::size_t>(c.n));
  for(std::int64_t i=0;i<c.n;i++){
   auto raw=static_cast<std::int64_t>(randbelow(state,static_cast<std::uint64_t>(c.value_range)+1));
   const auto neg=randbelow(state,1000000)<static_cast<std::uint64_t>(c.sign_ppm);
   auto value=neg?-raw:raw;
   const auto sorted_value=std::clamp(i-c.n/2,-c.value_range,c.value_range);
   if(c.distribution=="ascending"||c.distribution=="descending"||c.distribution=="nearly_sorted")value=sorted_value;
   else if(c.distribution=="organ_pipe")value=std::min<std::int64_t>(i<=c.n/2?i:c.n-i,c.value_range);
   else if(c.distribution=="sawtooth")value=(i%c.run_length)% (c.value_range+1);
   else if(c.distribution=="alternating")value=std::min<std::int64_t>(i%2?c.n-i:i,c.value_range);
   else if(c.distribution=="all_equal")value=std::min<std::int64_t>(42,c.value_range);
   else if(c.distribution=="heavy_duplicate")value=static_cast<std::int64_t>(randbelow(state,static_cast<std::uint64_t>(std::min<std::int64_t>(3,2*c.value_range+1))));
   else if(c.distribution=="few_unique"){
    const auto count=c.unique_count==0?std::min<std::int64_t>({16,c.n,2*c.value_range+1}):c.unique_count;
    value=count==0?0:static_cast<std::int64_t>(randbelow(state,static_cast<std::uint64_t>(count)))-count/2;
   }else if(c.distribution=="zipf_like"){
    auto u=randbelow(state,10000);value=std::min<std::int64_t>(c.value_range,u<6000?0:u<8000?1:u<9000?2:static_cast<std::int64_t>(randbelow(state,64)));
   }
   v[static_cast<std::size_t>(i)]=value;
  }
  if(c.duplicate_ppm>=0&&c.n>1){
   for(std::size_t i=1;i<v.size();i++)if(randbelow(state,1000000)<static_cast<std::uint64_t>(c.duplicate_ppm))
    v[i]=v[static_cast<std::size_t>(randbelow(state,i))];
  }
  if(c.distribution=="ascending"||c.distribution=="descending"||c.distribution=="nearly_sorted"){
   std::sort(v.begin(),v.end());
   if(c.distribution=="descending")std::reverse(v.begin(),v.end());
   if(c.distribution=="nearly_sorted"&&v.size()>1){
    const auto swaps=static_cast<std::int64_t>((c.n*c.disorder_ppm+500000)/1000000);
    for(std::int64_t i=0;i<swaps;i++){
     const auto a=static_cast<std::size_t>(randbelow(state,v.size()));
     const auto b=static_cast<std::size_t>(randbelow(state,v.size()));
     std::swap(v[a],v[b]);
    }
   }
  }
  return;
 }
 if(c.family!="graph")throw Error("unknown input family: "+c.family);
 if(c.vertices<1||c.vertices>4096||c.edges<0||c.edges>200000||c.source<0||c.source>=c.vertices||
    c.community_count<1||c.community_count>c.vertices||c.degree_skew_ppm<0||c.degree_skew_ppm>1000000||
    c.density_ppm< -1||c.density_ppm>1000000||c.max_weight<0||c.max_weight>1000000)
  throw Error("invalid graph morphology controls");
 if(c.weight_class!="general"&&c.weight_class!="zero_one")throw Error("unknown graph weight class");
 if(c.connectivity!="none"&&c.connectivity!="weak"&&c.connectivity!="strong")throw Error("unknown graph connectivity class");
 if(!c.directed&&c.connectivity=="strong")throw Error("strong directed connectivity not meaningful for undirected graph");
 const auto n=static_cast<std::uint64_t>(c.vertices);
 const auto capacity=c.directed?n*(n-1):n*(n-1)/2;
 if(c.density_ppm>=0)c.edges=static_cast<std::int64_t>((capacity*static_cast<std::uint64_t>(c.density_ppm)+500000)/1000000);
 const auto needed=static_cast<std::uint64_t>(c.edges);
 if(needed>capacity||needed>200000)throw Error("graph edge count exceeds topology or generation budget");
 const auto minimum=c.connectivity=="strong"?(n>1?n:0):(c.connectivity=="weak"?n-1:0);
 if(needed<minimum)throw Error("insufficient edges for requested connectivity");
 const auto arcs=needed*(c.directed?1ULL:2ULL);
 if(arcs>400000||(3ULL+arcs*3ULL)>max_bytes/8)throw Error("graph arcs exceed input-byte budget");
 v={c.vertices,static_cast<std::int64_t>(arcs),c.source};
 std::unordered_set<std::uint64_t> selected;
 selected.reserve(static_cast<std::size_t>(needed)*2+1);
 std::vector<std::uint64_t> positions;positions.reserve(static_cast<std::size_t>(needed));
 const auto insert=[&](std::uint64_t position){if(selected.insert(position).second)positions.push_back(position);};
 const auto edge_index=[&](std::uint64_t a,std::uint64_t b){
  if(a==b||a>=n||b>=n)throw Error("invalid requested graph connectivity edge");
  if(c.directed)return a*(n-1)+(b<a?b:b-1);
  if(a>b)std::swap(a,b);
  return a*(2*n-a-1)/2+(b-a-1);
 };
 const auto edge_at=[&](std::uint64_t position){
  if(c.directed){auto a=position/(n-1),offset=position%(n-1);return std::pair<std::uint64_t,std::uint64_t>{a,offset>=a?offset+1:offset};}
  std::uint64_t lo=0,hi=n;
  while(lo+1<hi){const auto mid=(lo+hi)/2;const auto prefix=mid*(2*n-mid-1)/2;if(prefix<=position)lo=mid;else hi=mid;}
  auto prefix=lo*(2*n-lo-1)/2;return std::pair<std::uint64_t,std::uint64_t>{lo,lo+1+position-prefix};
 };
 if(c.connectivity!="none"){
  for(std::uint64_t i=1;i<n;i++)insert(edge_index(i-1,i));
  if(c.connectivity=="strong"&&n>1)insert(edge_index(n-1,0));
 }
 /* Floyd uniform baseline is rejection-free when the search space is dense.
    Biased generators use bounded rejection followed by a finite fallback. */
 if(c.connectivity=="none"&&c.degree_skew_ppm==0&&c.community_count==1){
  for(std::uint64_t j=capacity-needed;j<capacity;j++){
   auto t=randbelow(state,j+1);auto pos=selected.count(t)?j:t;insert(pos);
  }
 }else{
  std::uint64_t attempts=0,limit=std::max<std::uint64_t>(1024,needed*8);
  while(positions.size()<needed&&attempts++<limit){
   std::uint64_t a=randbelow(state,n),b=randbelow(state,n);
   if(c.degree_skew_ppm>0&&randbelow(state,1000000)<static_cast<std::uint64_t>(c.degree_skew_ppm))
    a=randbelow(state,std::max<std::uint64_t>(1,n/8));
   if(c.community_count>1&&randbelow(state,100)<75){
    const auto first=a%static_cast<std::uint64_t>(c.community_count);
    const auto count=(n-1-first)/static_cast<std::uint64_t>(c.community_count)+1;
    b=first+randbelow(state,count)*static_cast<std::uint64_t>(c.community_count);
   }
   if(a!=b)insert(edge_index(a,b));
  }
  if(positions.size()<needed){
   const auto start=randbelow(state,capacity);
   for(std::uint64_t i=0;i<capacity&&positions.size()<needed;i++)insert((start+i)%capacity);
  }
  if(positions.size()!=needed)throw Error("graph generator failed finite edge fill");
 }
 for(const auto position:positions){
  const auto [a,b]=edge_at(position);
  const auto w=static_cast<std::int64_t>(randbelow(state,c.weight_class=="zero_one"?2ULL:static_cast<std::uint64_t>(c.max_weight)+1));
  v.push_back(static_cast<std::int64_t>(a));v.push_back(static_cast<std::int64_t>(b));v.push_back(w);
  if(!c.directed){v.push_back(static_cast<std::int64_t>(b));v.push_back(static_cast<std::int64_t>(a));v.push_back(w);}
 }
}

Case make_case(const Case& params,std::uint64_t max_bytes){
 Case c=params;validate_case_schema(c,max_bytes);std::vector<std::int64_t> v;std::uint64_t state=c.seed;
 if(c.generator_version==3){
  c.bytes=hex_decode(c.literal_hex);
  c.id=sha256(std::string("ps-case-v1|")+c.family+"|generator-v3|"+case_params(c)+"|"+std::to_string(c.seed)+"|"+hex_encode(c.bytes));
  return c;
 }
 if(c.generator_version!=1&&c.generator_version!=2)throw Error("unsupported generator version");
 if(c.generator_version==2)synthesize_v2(c,v,state,max_bytes);
 else if(c.family=="calibration"){
  if(c.x<0||c.x>1000000||c.threshold<1||c.threshold>1000000)throw Error("calibration x or threshold out of range");
  v={c.x,c.threshold,c.negative?1:0};
 }else if(c.family=="integer_array"||c.family=="topk"){
  if(c.n<0||c.n>50000|| (c.family=="topk"&&(c.k<0||c.k>c.n)))throw Error("invalid n/k (n <= 50000, 0 <= k <= n)");
  if(static_cast<std::uint64_t>(c.n)*8>max_bytes)throw Error("input exceeds byte budget");
  static const std::vector<std::string> valid={"uniform","ascending","descending","nearly_sorted","organ_pipe","sawtooth","alternating","all_equal","heavy_duplicate","few_unique","zipf_like"};
  if(std::find(valid.begin(),valid.end(),c.distribution)==valid.end())throw Error("unknown array distribution: "+c.distribution);
  v.resize(static_cast<std::size_t>(c.n));
  for(std::int64_t i=0;i<c.n;i++){
   std::int64_t x=static_cast<std::int64_t>(randbelow(state,2000001))-1000000;
   if(c.distribution=="ascending"||c.distribution=="descending"||c.distribution=="nearly_sorted")x=i-c.n/2;
   else if(c.distribution=="organ_pipe")x=(i<=c.n/2?i:c.n-i);
   else if(c.distribution=="sawtooth")x=i%17;
   else if(c.distribution=="alternating")x=(i%2?c.n-i:i);
   else if(c.distribution=="all_equal")x=42;
   else if(c.distribution=="heavy_duplicate")x=static_cast<std::int64_t>(randbelow(state,3));
   else if(c.distribution=="few_unique")x=static_cast<std::int64_t>(randbelow(state,16));
   else if(c.distribution=="zipf_like"){auto r=randbelow(state,10000);x=r<6000?0:r<8000?1:r<9000?2:static_cast<std::int64_t>(randbelow(state,64));}
   v[static_cast<std::size_t>(i)]=x;
  }
  if(c.distribution=="descending")std::reverse(v.begin(),v.end());
  if(c.distribution=="nearly_sorted"&&c.n>1){for(std::int64_t i=0;i<std::max<std::int64_t>(1,c.n/100);i++){auto a=randbelow(state,static_cast<std::uint64_t>(c.n));auto b=randbelow(state,static_cast<std::uint64_t>(c.n));std::swap(v[static_cast<std::size_t>(a)],v[static_cast<std::size_t>(b)]);}}
 }else if(c.family=="graph"){
  if(c.vertices<1||c.vertices>4096||c.edges<0||c.edges>200000||c.source<0||c.source>=c.vertices)throw Error("invalid graph dimensions/source");
  if(c.weight_class!="general"&&c.weight_class!="zero_one")throw Error("unknown graph weight class");
  const auto vertices=static_cast<std::uint64_t>(c.vertices),edges=static_cast<std::uint64_t>(c.edges);
  if(edges>vertices*(vertices-1))throw Error("too many distinct directed non-loop graph edges");
  if((3ULL+edges*3ULL)*8ULL>max_bytes)throw Error("graph exceeds byte budget");
  v={c.vertices,c.edges,c.source};
  std::vector<std::uint64_t> positions;positions.reserve(static_cast<std::size_t>(edges));
  std::unordered_set<std::uint64_t> selected;selected.reserve(static_cast<std::size_t>(edges)*2+1);
  /* Deterministic sampling without rejection for dense graphs. */
  for(std::uint64_t j=vertices*(vertices-1)-edges;j<vertices*(vertices-1);j++){
   auto t=randbelow(state,j+1);auto pos=selected.count(t)?j:t;selected.insert(pos);
   positions.push_back(pos);
  }
  for(auto pos:positions){const auto a=pos/(vertices-1);const auto b0=pos%(vertices-1);const auto b=b0>=a?b0+1:b0;
   v.push_back(static_cast<std::int64_t>(a));v.push_back(static_cast<std::int64_t>(b));v.push_back(static_cast<std::int64_t>(randbelow(state,c.weight_class=="zero_one"?2:101)));
  }
 }else throw Error("unknown family: "+c.family);
 c.bytes=encode_values(v);if(c.bytes.size()>max_bytes)throw Error("input exceeds byte budget");
 c.id=sha256(std::string("ps-case-v1|")+c.family+"|generator-v"+std::to_string(c.generator_version)+"|"+case_params(c)+"|"+std::to_string(c.seed)+"|"+hex_encode(c.bytes));return c;
}
Case decode_case(const std::string& family,const std::string& params,std::uint64_t seed,const Bytes& bytes){
 auto j=Json::parse(params);Case c;c.family=family;c.seed=seed;
 c.generator_version=j.i("generator_version",1);
 c.distribution=j.s("distribution","uniform");c.weight_class=j.s("weight_class","general");
 c.n=j.i("n",32);c.x=j.i("x",0);c.k=j.i("k",8);c.vertices=j.i("vertices",16);
 c.edges=j.i("edges",32);c.source=j.i("source",0);c.threshold=j.i("threshold",8);c.negative=j.b("negative",false);
 c.value_range=j.i("value_range",1000000);c.unique_count=j.i("unique_count",0);c.run_length=j.i("run_length",17);
 c.duplicate_ppm=j.i("duplicate_ppm",-1);c.disorder_ppm=j.i("disorder_ppm",10000);c.sign_ppm=j.i("sign_ppm",500000);
 c.density_ppm=j.i("density_ppm",-1);c.degree_skew_ppm=j.i("degree_skew_ppm",0);
 c.community_count=j.i("community_count",1);c.max_weight=j.i("max_weight",100);
 c.directed=j.b("directed",true);c.connectivity=j.s("connectivity","none");
 c.literal_hex=j.s("literal_hex","");
 c=make_case(c);if(c.bytes!=bytes)throw Error("case regeneration digest/bytes mismatch");return c;
}
std::string now_utc(){std::time_t t=std::time(nullptr);std::tm utc{};gmtime_r(&t,&utc);char b[32];std::strftime(b,sizeof(b),"%Y-%m-%dT%H:%M:%SZ",&utc);return b;}
std::string environment(){
 struct utsname u{};uname(&u);Json j;j.put("timestamp_utc",now_utc());j.put("kernel",std::string(u.release));j.put("architecture",std::string(u.machine));j.put("compiler",std::string(__VERSION__));j.put("clock","std::chrono::steady_clock");j.put("cores",static_cast<std::int64_t>(std::thread::hardware_concurrency()));
 try{auto cpu=read_file("/proc/cpuinfo");auto p=cpu.find("model name");if(p!=std::string::npos){auto s=cpu.find(':',p),e=cpu.find('\n',p);if(s!=std::string::npos)j.put("cpu_model",cpu.substr(s+2,e-s-2));}}catch(const Error&){}
 return j.str();
}
}
