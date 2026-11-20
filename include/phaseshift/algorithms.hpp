#pragma once
#include "phaseshift/core.hpp"
namespace ps {
struct Result { std::vector<std::int64_t> values; std::uint64_t work=0; };
Result execute(const Case& c,const std::string& algorithm);
Result oracle(const Case& c);
std::vector<std::string> algorithms_for(const std::string& family);
std::vector<std::string> families();
}
