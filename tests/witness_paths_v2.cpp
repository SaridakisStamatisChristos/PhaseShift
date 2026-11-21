#include "phaseshift/witness_paths_v2.hpp"
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("witness paths v2: " #x);}while(false)
int main(){
 const std::filesystem::path base="/safe/run";
 CHECK(ps::witness_output_root(base,"H1","v1")==base);
 CHECK(ps::witness_output_root(base,"H1","v2")==base);
 CHECK(ps::witness_output_root(base,"H2","v2")==base/"witnesses"/"H2");
 CHECK(ps::witness_output_root(base,"H16","v2")==base/"witnesses"/"H16");
 CHECK(ps::witness_output_root(base,"H2","v1")==base);
 CHECK(ps::witness_evidence_relative("H1","v2")=="minimization.sqlite");
 CHECK(ps::witness_evidence_relative("H2","v2")=="witnesses/H2/minimization.sqlite");
 for(const char* invalid:{"../x","H0","H17","H02","H2/x","H2/..","H99999999999999999999","H2\\tmp"}){
  bool refused=false;try{(void)ps::witness_output_root(base,invalid,"v2");}
  catch(const ps::Error&){refused=true;}
  CHECK(refused);
 }
 return 0;
}
