#!/usr/bin/env bash
set -euo pipefail

# Fresh compiler-instrumented production + CLI coverage. Never reuse stale raw
# profiles, and fail if new source files are absent from the coverage report.
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="${PHASESHIFT_COVERAGE_BUILD:-${root}/build-coverage}"
min_line="${PHASESHIFT_COVERAGE_MIN_LINE:-95}"
for tool in cmake clang clang++ llvm-profdata llvm-cov python3; do
  command -v "$tool" >/dev/null || { echo "Missing coverage tool: $tool" >&2; exit 2; }
done
if command -v ninja >/dev/null; then generator=(-G Ninja); else generator=(); fi
cmake -S "$root" -B "$build" "${generator[@]}" \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  '-DCMAKE_C_FLAGS=-fprofile-instr-generate -fcoverage-mapping' \
  '-DCMAKE_CXX_FLAGS=-fprofile-instr-generate -fcoverage-mapping' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fprofile-instr-generate' \
  '-DCMAKE_SHARED_LINKER_FLAGS=-fprofile-instr-generate'
cmake --build "$build" --parallel "${PHASESHIFT_BUILD_JOBS:-2}"
mkdir -p "$build/profiles"
rm -f "$build"/profiles/*.profraw "$build/merged.profdata" "$build/coverage.json"
export LLVM_PROFILE_FILE="$build/profiles/%p-%m.profraw"
ctest --test-dir "$build" --output-on-failure -j "${PHASESHIFT_TEST_JOBS:-2}"
shopt -s nullglob
profiles=("$build"/profiles/*.profraw)
if ((${#profiles[@]} == 0)); then echo 'No coverage profiles generated' >&2; exit 2; fi
llvm-profdata merge -sparse "${profiles[@]}" -o "$build/merged.profdata"
llvm-cov export "$build/phaseshift_tests" \
  -instr-profile="$build/merged.profdata" \
  -object="$build/phaseshift" -object="$build/phaseshift_worker" \
  -object="$build/phaseshift_v3_pairs" \
  -object="$build/phaseshift_boundary_v2_tests" -object="$build/phaseshift_interaction_v2_tests" \
  -object="$build/phaseshift_pelt_v2_tests" -object="$build/phaseshift_campaign_v2_tests" \
  -object="$build/phaseshift_hybrid_v2_tests" \
  -object="$build/phaseshift_multi_slope_v2_tests" \
  -object="$build/phaseshift_confirmation_seed_bounds_v2_tests" \
  -object="$build/phaseshift_v2_multi_witness_tests" \
  -object="$build/phaseshift_witness_paths_v2_tests" \
  -object="$build/phaseshift_block_drift_v2_tests" \
  -object="$build/phaseshift_weighted_v2_tests" \
  -object="$build/phaseshift_conditional_rule_v2_tests" \
  -object="$build/phaseshift_v2_evidence_view_tests" \
  -object="$build/phaseshift_cpu_usage_v2_tests" \
  -object="$build/phaseshift_scheduler_heap_v2_tests" \
  -object="$build/phaseshift_fixed_factorial_holdout_tests" \
  -object="$build/phaseshift_model_cache_v2_tests" \
  -object="$build/phaseshift_v2_noisy_detection_tests" \
  -object="$build/phaseshift_v2_large_space_rss_tests" \
  > "$build/coverage.json"
python3 "$root/scripts/coverage_gate.py" --root "$root" --min-line "$min_line" --input "$build/coverage.json"
