#include "phaseshift/core.hpp"
#include <openssl/evp.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
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
std::string hex_encode(const Bytes& b){static constexpr char abc[]="0123456789abcdef";std::string s;s.reserve(b.size()*2);for(auto x:b){s.push_back(abc[x>>4]);s.push_back(abc[x&15]);}return s;}
Bytes hex_decode(const std::string& s){if(s.size()%2)throw Error("invalid hex byte count");Bytes b;b.reserve(s.size()/2);auto digit=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;};for(std::size_t i=0;i<s.size();i+=2){int a=digit(s[i]),d=digit(s[i+1]);if(a<0||d<0)throw Error("invalid hex digit");b.push_back(static_cast<std::uint8_t>(a*16+d));}return b;}
std::uint64_t splitmix64(std::uint64_t& s){std::uint64_t z=(s+=0x9e3779b97f4a7c15ULL);z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;z=(z^(z>>27))*0x94d049bb133111ebULL;return z^(z>>31);}
std::uint64_t randbelow(std::uint64_t& s,std::uint64_t bound){if(!bound)throw Error("random bound zero");const auto threshold=static_cast<std::uint64_t>(-bound)%bound;for(;;){auto r=splitmix64(s);if(r>=threshold)return r%bound;}}
Bytes encode_values(const std::vector<std::int64_t>& v){Bytes b;b.reserve(v.size()*8);for(auto x:v){auto n=static_cast<std::uint64_t>(x);for(int j=0;j<8;j++)b.push_back(static_cast<std::uint8_t>((n>>(8*j))&255));}return b;}
std::vector<std::int64_t> decode_values(const Bytes& b){if(b.size()%8)throw Error("invalid little-endian int64 payload");std::vector<std::int64_t> v;v.reserve(b.size()/8);for(std::size_t i=0;i<b.size();i+=8){std::uint64_t x=0;for(int j=0;j<8;j++)x|=static_cast<std::uint64_t>(b[i+static_cast<std::size_t>(j)])<<(8*j);v.push_back(static_cast<std::int64_t>(x));}return v;}
std::string case_params(const Case& c){
 Json j;j.put("distribution",c.distribution);j.put("weight_class",c.weight_class);j.put("x",c.x);j.put("n",c.n);j.put("k",c.k);j.put("vertices",c.vertices);j.put("edges",c.edges);j.put("source",c.source);j.put("threshold",c.threshold);j.put("negative",c.negative);return j.str();
}
Case make_case(const Case& params,std::uint64_t max_bytes){
 Case c=params;std::vector<std::int64_t> v;std::uint64_t state=c.seed;
 if(c.family=="calibration"){
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
 c.id=sha256(std::string("ps-case-v1|")+c.family+"|generator-v1|"+case_params(c)+"|"+std::to_string(c.seed)+"|"+hex_encode(c.bytes));return c;
}
Case decode_case(const std::string& family,const std::string& params,std::uint64_t seed,const Bytes& bytes){
 auto j=Json::parse(params);Case c;c.family=family;c.seed=seed;c.distribution=j.s("distribution","uniform");c.weight_class=j.s("weight_class","general");c.n=j.i("n",32);c.x=j.i("x",0);c.k=j.i("k",8);c.vertices=j.i("vertices",16);c.edges=j.i("edges",32);c.source=j.i("source",0);c.threshold=j.i("threshold",8);c.negative=j.b("negative",false);c=make_case(c);if(c.bytes!=bytes)throw Error("case regeneration digest/bytes mismatch");return c;
}
std::string now_utc(){std::time_t t=std::time(nullptr);std::tm utc{};gmtime_r(&t,&utc);char b[32];std::strftime(b,sizeof(b),"%Y-%m-%dT%H:%M:%SZ",&utc);return b;}
std::string environment(){
 struct utsname u{};uname(&u);Json j;j.put("timestamp_utc",now_utc());j.put("kernel",std::string(u.release));j.put("architecture",std::string(u.machine));j.put("compiler",std::string(__VERSION__));j.put("clock","std::chrono::steady_clock");j.put("cores",static_cast<std::int64_t>(std::thread::hardware_concurrency()));
 try{auto cpu=read_file("/proc/cpuinfo");auto p=cpu.find("model name");if(p!=std::string::npos){auto s=cpu.find(':',p),e=cpu.find('\n',p);if(s!=std::string::npos)j.put("cpu_model",cpu.substr(s+2,e-s-2));}}catch(const Error&){}
 return j.str();
}
}
