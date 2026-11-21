#pragma once
#include "phaseshift/core.hpp"
#include <filesystem>
#include <string>
namespace ps {
// Preserve every legacy/H1 artifact path; isolate named v2 alternatives.
// Strict identity validation prevents path traversal and unrecognized aliases.
inline std::filesystem::path witness_output_root(const std::filesystem::path& run,
 const std::string& hypothesis,const std::string& policy){
 if(policy!="v2"||hypothesis=="H1")return run;
 if(hypothesis.size()<2||hypothesis[0]!='H')throw Error("invalid v2 witness identity");
 std::size_t parsed=0;unsigned long rank=0;
 try{rank=std::stoul(hypothesis.substr(1),&parsed);}
 catch(const std::exception&){throw Error("invalid v2 witness rank");}
 if(parsed!=hypothesis.size()-1||rank<2||rank>16||
   hypothesis!="H"+std::to_string(rank))throw Error("unsupported v2 witness identity");
 return run/"witnesses"/hypothesis;
}
inline std::string witness_evidence_relative(const std::string& hypothesis,
 const std::string& policy){
 return policy=="v2"&&hypothesis!="H1"?
   "witnesses/"+hypothesis+"/minimization.sqlite":"minimization.sqlite";
}
} // namespace ps
