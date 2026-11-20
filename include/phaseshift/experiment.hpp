#pragma once
#include "phaseshift/store.hpp"
namespace ps {
struct Config {
 std::string experiment_id="experiment",family="calibration",a="calibration",b="",method="grid",axis="x",output="";
 Case base;
 std::map<std::string,std::vector<std::string>> axes;
 std::uint64_t seed=137,max_cases=128,max_bytes=64ULL*1024*1024,max_queue=10000,max_report_bytes=10000000;
 int blocks=9,confirm_blocks=20,warmup=2,sample_ms=3000,rss_mb=1024,total_wall_s=600;
 double min_ratio=1.20;
};
Config parse_config(const std::string& path);
Config config_from_json(const Json& json);
void explore(const Config& cfg,const std::string& out,std::size_t pause_after=0);
void resume(const std::string& run,std::size_t pause_after=0);
void ablate(const Config& cfg,const std::string& out);
void confirm(const std::string& run,const std::string& id="H1");
void minimize(const std::string& run,const std::string& id="H1");
void replay(const std::string& run,const std::string& case_id);
void report(const std::string& run);
void export_run(const std::string& run,const std::string& out);
void verify_bundle(const std::string& bundle);
void import_bundle(const std::string& bundle,const std::string& out);
void compare(const std::string& a,const std::string& b);
}
