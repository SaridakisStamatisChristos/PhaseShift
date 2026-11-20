#pragma once
#include "phaseshift/core.hpp"
#include <string>
namespace ps {
// Produce a self-contained C++20 program for a validated stored built-in case.
// Every emitted program embeds exact input and independent oracle output.
// Plugin binaries are intentionally excluded: their source is not in the host.
std::string standalone_reproducer(const Case& input,const std::string& algorithm);
void reproduce_case(const std::string& run,const std::string& case_id,
                    const std::string& algorithm,const std::string& out);
}
