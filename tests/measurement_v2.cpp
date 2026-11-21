#include "phaseshift/runner.hpp"
#include <json-c/json.h>
#include <stdexcept>
#include <string>
#define CHECK(x) do{if(!(x))throw std::runtime_error("measurement_v2: " #x);}while(false)
int main(){
 ps::Case c;c.family="calibration";c.x=3;c.threshold=2;c.seed=23;c=ps::make_case(c);
 ps::Json req;
 req.put("protocol",static_cast<std::int64_t>(1));req.put("family",c.family);
 req.put("params",ps::case_params(c));req.put("seed",static_cast<std::int64_t>(c.seed));
 req.put("bytes_length",static_cast<std::int64_t>(c.bytes.size()));
 req.put("bytes_sha256",ps::sha256(c.bytes));
 req.put("blocks",static_cast<std::int64_t>(1));req.put("warmup",static_cast<std::int64_t>(0));
 req.put("sample_ms",static_cast<std::int64_t>(2000));
 req.put("minimum_batch_ms",static_cast<std::int64_t>(1));
 auto* names=json_object_new_array();
 json_object_array_add(names,json_object_new_string("calibration"));
 json_object_object_add(req.p,"algorithms",names);
 auto legacy=ps::worker_measure(req,c.bytes);
 auto* samples=legacy.get("observations");
 CHECK(samples&&json_object_array_length(samples)==1);
 ps::Json baseline(json_object_get(json_object_array_get_idx(samples,0)));
 CHECK(baseline.s("status")=="ok");
 CHECK(!baseline.get("pilot_ns")&&!baseline.get("measurement_protocol_version"));
 req.put("measurement_protocol_version","calibrated_batch_v2");
 const auto calibrated=ps::worker_measure(req,c.bytes);
 samples=calibrated.get("observations");
 CHECK(samples&&json_object_array_length(samples)==1);
 ps::Json v2(json_object_get(json_object_array_get_idx(samples,0)));
 CHECK(v2.s("status")=="ok");
 CHECK(v2.i("ns")>0&&v2.i("batch")>=1);
 CHECK(v2.i("pilot_ns")>0&&v2.i("pilot_iterations")>=1);
 CHECK(v2.i("batch")==v2.i("pilot_iterations"));
 CHECK(v2.s("measurement_protocol_version")=="calibrated_batch_v2");
 CHECK(v2.s("digest")==baseline.s("digest")&&v2.i("work")==baseline.i("work"));
 req.put("measurement_protocol_version","kernel_only_v2");
 bool rejected=false;
 try{(void)ps::worker_measure(req,c.bytes);}catch(const ps::Error&){rejected=true;}
 CHECK(rejected);
 return 0;
}
