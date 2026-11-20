#include "phaseshift/reproducer.hpp"
#include "phaseshift/core.hpp"
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
int main(int argc,char** argv){try{
 if(argc!=2)throw ps::Error("expected .cpp output file");
 ps::Case input;input.family="integer_array";input.generator_version=3;
 input.distribution="literal";input.n=3;
 input.literal_hex=ps::hex_encode(ps::encode_values({
  (std::numeric_limits<std::int64_t>::min)(),0,
  (std::numeric_limits<std::int64_t>::max)()
 }));
 const auto c=ps::make_case(input);
 const auto source=ps::standalone_reproducer(c,"std_sort");
 if(source.find("(std::numeric_limits<std::int64_t>::min)()") == std::string::npos||
    source.find("(std::numeric_limits<std::int64_t>::max)()") == std::string::npos||
    source.find(c.id)==std::string::npos)throw ps::Error("extreme numeric input or identity not preserved");
 std::ofstream out(argv[1]);if(!out)throw ps::Error("cannot create standalone source");
 out<<source;out.close();if(!out)throw ps::Error("failed to write standalone source");
 bool rejected=false;
 try{(void)ps::standalone_reproducer(c,"plugin:/untrusted.so");}catch(const ps::Error&){rejected=true;}
 if(!rejected)throw ps::Error("plugin must be refused standalone source");
 ps::Case empty;empty.family="topk";empty.generator_version=3;
 empty.distribution="literal";empty.n=0;empty.k=0;
 empty.literal_hex="";
 const auto e=ps::make_case(empty);
 const auto empty_src=ps::standalone_reproducer(e,"topk_nth");
 const std::string empty_path=std::string(argv[1])+".empty.cpp";
 std::ofstream empty_out(empty_path);if(!empty_out)throw ps::Error("cannot create empty TopK witness");
 empty_out<<empty_src;empty_out.close();if(!empty_out)throw ps::Error("cannot write empty TopK witness");
 std::cout<<"PASS standalone numerical extreme export\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
