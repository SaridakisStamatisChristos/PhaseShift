#pragma once
#include "phaseshift/core.hpp"
#include <functional>
#include <string>
#include <vector>
namespace ps {
struct ShrinkStep {
 std::string case_id;
 std::size_t input_bytes=0;
 std::string action;
 bool predicate_preserved=false;
};
struct ShrinkResult {
 Case original;
 Case smallest;
 std::vector<ShrinkStep> trace;
 std::size_t evaluations=0;
 bool reduced=false;
};
/* Deterministic, generator-aware minimization: every candidate is rebuilt from
   typed parameters and exact SHA-256 identity. Caller supplies the tested
   preservation predicate; candidates are never accepted without its success. */
ShrinkResult shrink_case(const Case& original,
                         const std::function<bool(const Case&)>& predicate,
                         std::size_t max_evaluations=64,
                         std::uint64_t max_case_bytes=64ULL*1024ULL*1024ULL);
}
