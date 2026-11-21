cmake_minimum_required(VERSION 3.24)
project(PHASESHIFT VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONC REQUIRED IMPORTED_TARGET json-c)
option(PHASESHIFT_SANITIZE "Build with AddressSanitizer and UBSan" OFF)
add_library(phaseshift_core STATIC src/core.cpp src/algorithms.cpp src/morphology.cpp src/statistics.cpp src/scaling.cpp src/minimize.cpp src/reproducer.cpp src/runner.cpp src/store.cpp src/experiment.cpp src/boundary_v2.cpp src/interaction_v2.cpp src/campaign_v2.cpp)
target_include_directories(phaseshift_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# Record actual compilation profile and checkout provenance for every run.
set(PHASESHIFT_EFFECTIVE_FLAGS "${CMAKE_CXX_FLAGS}")
string(TOUPPER "${CMAKE_BUILD_TYPE}" PHASESHIFT_UPPER_PROFILE)
if(DEFINED CMAKE_CXX_FLAGS_${PHASESHIFT_UPPER_PROFILE})
  string(APPEND PHASESHIFT_EFFECTIVE_FLAGS " ${CMAKE_CXX_FLAGS_${PHASESHIFT_UPPER_PROFILE}}")
endif()
execute_process(COMMAND git rev-parse --verify HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE PHASESHIFT_SOURCE_SHA OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git status --porcelain WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE PHASESHIFT_DIRTY OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(PHASESHIFT_SOURCE_SHA STREQUAL "")
  set(PHASESHIFT_SOURCE_SHA "unavailable")
endif()
if(PHASESHIFT_DIRTY STREQUAL "")
  set(PHASESHIFT_DIRTY_LABEL "false")
else()
  set(PHASESHIFT_DIRTY_LABEL "true")
endif()
target_compile_definitions(phaseshift_core PRIVATE
  PHASESHIFT_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
  PHASESHIFT_COMPILER_ID="${CMAKE_CXX_COMPILER_ID}"
  PHASESHIFT_COMPILER_VERSION="${CMAKE_CXX_COMPILER_VERSION}"
  PHASESHIFT_BUILD_FLAGS="${PHASESHIFT_EFFECTIVE_FLAGS}"
  PHASESHIFT_SOURCE_COMMIT="${PHASESHIFT_SOURCE_SHA}"
  PHASESHIFT_SOURCE_DIRTY="${PHASESHIFT_DIRTY_LABEL}")

target_link_libraries(phaseshift_core PUBLIC SQLite::SQLite3 OpenSSL::Crypto PkgConfig::JSONC ${CMAKE_DL_LIBS})
target_compile_options(phaseshift_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
if(PHASESHIFT_SANITIZE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  foreach(t phaseshift_core)
    target_compile_options(${t} PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${t} PUBLIC -fsanitize=address,undefined)
    target_compile_definitions(${t} PUBLIC PHASESHIFT_ASAN_BUILD=1)
  endforeach()
endif()
add_executable(phaseshift apps/phaseshift.cpp)
target_link_libraries(phaseshift PRIVATE phaseshift_core)
add_executable(phaseshift_worker apps/phaseshift_worker.cpp)
target_link_libraries(phaseshift_worker PRIVATE phaseshift_core)
add_library(phaseshift_example_plugin SHARED plugins/examples/reverse_sort.c)
target_include_directories(phaseshift_example_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(phaseshift_example_plugin PROPERTIES PREFIX "" OUTPUT_NAME "reverse_sort")
foreach(mode IN ITEMS 0 1)
  add_library(phaseshift_stateful_${mode} SHARED plugins/examples/stateful_plugin.c)
  target_include_directories(phaseshift_stateful_${mode} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_compile_definitions(phaseshift_stateful_${mode} PRIVATE STATEFUL_RESET=${mode})
  set_target_properties(phaseshift_stateful_${mode} PROPERTIES PREFIX "" OUTPUT_NAME "stateful_${mode}")
endforeach()
enable_testing()
add_executable(phaseshift_tests tests/tests.cpp)
target_link_libraries(phaseshift_tests PRIVATE phaseshift_core)
target_compile_definitions(phaseshift_tests PRIVATE PHASESHIFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME core COMMAND phaseshift_tests)
configure_file(tests/run_demo.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_demo.cmake @ONLY)
add_test(NAME demo COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_demo.cmake)
set_tests_properties(demo PROPERTIES TIMEOUT 60)
configure_file(tests/run_null.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_null.cmake @ONLY)
add_test(NAME negative_control COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_null.cmake)
set_tests_properties(negative_control PROPERTIES TIMEOUT 60)
configure_file(tests/run_ablation.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_ablation.cmake @ONLY)
add_test(NAME synthetic_ablation COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_ablation.cmake)
set_tests_properties(synthetic_ablation PROPERTIES TIMEOUT 60)
configure_file(tests/run_resume.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_resume.cmake @ONLY)
add_test(NAME restart_resume COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_resume.cmake)
set_tests_properties(restart_resume PROPERTIES TIMEOUT 60)

foreach(mode IN ITEMS 1 2 3 4 5 6 7 8 9)
  add_library(phaseshift_fault_${mode} SHARED plugins/examples/fault_plugin.cpp)
  target_include_directories(phaseshift_fault_${mode} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_compile_definitions(phaseshift_fault_${mode} PRIVATE FAULT_MODE=${mode})
  set_target_properties(phaseshift_fault_${mode} PROPERTIES PREFIX "" OUTPUT_NAME "fault_${mode}")
endforeach()
configure_file(tests/run_bundle.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_bundle.cmake @ONLY)
add_test(NAME evidence_bundle COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_bundle.cmake)
set_tests_properties(evidence_bundle PROPERTIES TIMEOUT 90)
configure_file(tests/run_scaling.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_scaling.cmake @ONLY)
add_test(NAME scaling_fingerprint COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_scaling.cmake)
set_tests_properties(scaling_fingerprint PROPERTIES TIMEOUT 90)

add_executable(phaseshift_morphology_tests tests/morphology_v2.cpp)
target_link_libraries(phaseshift_morphology_tests PRIVATE phaseshift_core)
add_test(NAME morphology_v2 COMMAND phaseshift_morphology_tests)
set_tests_properties(morphology_v2 PROPERTIES TIMEOUT 90)

configure_file(tests/run_witness.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_witness.cmake @ONLY)
add_test(NAME failure_witness COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_witness.cmake)
set_tests_properties(failure_witness PROPERTIES TIMEOUT 120)
add_executable(phaseshift_minimization_tests tests/minimization_unit.cpp)
target_link_libraries(phaseshift_minimization_tests PRIVATE phaseshift_core)
add_test(NAME minimization_unit COMMAND phaseshift_minimization_tests)
set_tests_properties(minimization_unit PROPERTIES TIMEOUT 45)
configure_file(tests/run_multidim.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_multidim.cmake @ONLY)
add_test(NAME frozen_multidimensional_boundary COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_multidim.cmake)
set_tests_properties(frozen_multidimensional_boundary PROPERTIES TIMEOUT 100)
add_executable(phaseshift_schema_tests tests/evidence_schema.cpp)
target_link_libraries(phaseshift_schema_tests PRIVATE phaseshift_core)
add_test(NAME evidence_schema COMMAND phaseshift_schema_tests ${CMAKE_CURRENT_BINARY_DIR}/ctest_schema)
set_tests_properties(evidence_schema PROPERTIES TIMEOUT 20)
configure_file(tests/run_real_ablation.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_real_ablation.cmake @ONLY)
add_test(NAME real_equal_budget_ablation COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_real_ablation.cmake)
set_tests_properties(real_equal_budget_ablation PROPERTIES TIMEOUT 120)
configure_file(tests/run_categorical.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_categorical.cmake @ONLY)
add_test(NAME categorical_axis_semantics COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_categorical.cmake)
set_tests_properties(categorical_axis_semantics PROPERTIES TIMEOUT 60)
add_executable(phaseshift_performance_witness_tests tests/performance_witness.cpp)
target_link_libraries(phaseshift_performance_witness_tests PRIVATE phaseshift_core)
add_test(NAME performance_witness_fail_closed COMMAND phaseshift_performance_witness_tests ${CMAKE_CURRENT_BINARY_DIR}/ctest_performance_witness)
set_tests_properties(performance_witness_fail_closed PROPERTIES TIMEOUT 120)

add_executable(phaseshift_ipc_negative_tests tests/ipc_negative.cpp)
add_test(NAME malformed_worker_ipc COMMAND phaseshift_ipc_negative_tests $<TARGET_FILE:phaseshift_worker>)
set_tests_properties(malformed_worker_ipc PROPERTIES TIMEOUT 25)
add_executable(phaseshift_adversarial_algorithms_tests tests/adversarial_algorithms.cpp)
target_link_libraries(phaseshift_adversarial_algorithms_tests PRIVATE phaseshift_core)
add_test(NAME adversarial_algorithm_campaign COMMAND phaseshift_adversarial_algorithms_tests)
set_tests_properties(adversarial_algorithm_campaign PROPERTIES TIMEOUT 120)
add_library(phaseshift_threshold_resource SHARED plugins/examples/threshold_fault.c)
target_include_directories(phaseshift_threshold_resource PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(phaseshift_threshold_resource PRIVATE THRESHOLD_MODE=1)
set_target_properties(phaseshift_threshold_resource PROPERTIES PREFIX "" OUTPUT_NAME "threshold_resource")
configure_file(tests/run_failure_boundaries.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_failure_boundaries.cmake @ONLY)
add_test(NAME failure_boundary_confirmation COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_failure_boundaries.cmake)
set_tests_properties(failure_boundary_confirmation PROPERTIES TIMEOUT 120)
add_executable(phaseshift_literal_witness_tests tests/literal_witness.cpp)
target_link_libraries(phaseshift_literal_witness_tests PRIVATE phaseshift_core)
add_test(NAME literal_structural_witness COMMAND phaseshift_literal_witness_tests)
set_tests_properties(literal_structural_witness PROPERTIES TIMEOUT 40)
add_executable(phaseshift_statistical_calibration_tests tests/statistical_calibration.cpp)
target_link_libraries(phaseshift_statistical_calibration_tests PRIVATE phaseshift_core)
add_test(NAME statistical_calibration COMMAND phaseshift_statistical_calibration_tests)
set_tests_properties(statistical_calibration PROPERTIES TIMEOUT 35)

# All tests which actually time worker algorithms must have one measurement
# lane per machine. Parallel CTest would otherwise introduce unrecorded
# competition and make noise/error-status tests spuriously flaky.
set_tests_properties(core demo negative_control synthetic_ablation
  restart_resume evidence_bundle scaling_fingerprint failure_witness
  frozen_multidimensional_boundary real_equal_budget_ablation
  categorical_axis_semantics performance_witness_fail_closed
  failure_boundary_confirmation PROPERTIES RUN_SERIAL TRUE)

configure_file(tests/run_reproducer.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_reproducer.cmake @ONLY)
add_test(NAME standalone_builtin_reproducers COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_reproducer.cmake)
set_tests_properties(standalone_builtin_reproducers PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
add_executable(phaseshift_reproducer_unit tests/reproducer_unit.cpp)
target_link_libraries(phaseshift_reproducer_unit PRIVATE phaseshift_core)

add_executable(phaseshift_measurement_policy_tests tests/measurement_policy.cpp)
target_link_libraries(phaseshift_measurement_policy_tests PRIVATE phaseshift_core)
add_test(NAME measurement_policy COMMAND phaseshift_measurement_policy_tests)
set_tests_properties(measurement_policy PROPERTIES TIMEOUT 45 RUN_SERIAL TRUE)
add_executable(phaseshift_scaling_boundary_tests tests/scaling_boundary.cpp)
target_link_libraries(phaseshift_scaling_boundary_tests PRIVATE phaseshift_core)
add_test(NAME scaling_boundary COMMAND phaseshift_scaling_boundary_tests)
set_tests_properties(scaling_boundary PROPERTIES TIMEOUT 15)
add_executable(phaseshift_environment_tests tests/environment_provenance.cpp)
target_link_libraries(phaseshift_environment_tests PRIVATE phaseshift_core)
add_test(NAME environment_provenance COMMAND phaseshift_environment_tests ${CMAKE_CURRENT_BINARY_DIR}/ctest_environment)
set_tests_properties(environment_provenance PROPERTIES TIMEOUT 30)
configure_file(tests/run_intervention.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_intervention.cmake @ONLY)
add_test(NAME matched_intervention COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_intervention.cmake)
set_tests_properties(matched_intervention PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)

# Independent adversarial witness for the durable confirmation evidence gate.
add_executable(phaseshift_confirmation_integrity_tests tests/confirmation_integrity.cpp)
target_link_libraries(phaseshift_confirmation_integrity_tests PRIVATE phaseshift_core)
add_test(NAME confirmation_integrity COMMAND phaseshift_confirmation_integrity_tests ${CMAKE_CURRENT_BINARY_DIR}/ctest_confirmation_integrity)
set_tests_properties(confirmation_integrity PROPERTIES TIMEOUT 30)
configure_file(tests/run_b07.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_b07.cmake @ONLY)
add_test(NAME b07_zero_one_timed COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_b07.cmake)
set_tests_properties(b07_zero_one_timed PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)
configure_file(tests/run_cli_validation.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_cli_validation.cmake @ONLY)
add_test(NAME cli_contract_falsifiers COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_cli_validation.cmake)
set_tests_properties(cli_contract_falsifiers PROPERTIES TIMEOUT 30)
add_executable(phaseshift_research_campaigns_tests tests/research_campaigns.cpp)
target_link_libraries(phaseshift_research_campaigns_tests PRIVATE phaseshift_core)
add_test(NAME b10_b11_research_campaigns COMMAND phaseshift_research_campaigns_tests ${CMAKE_CURRENT_BINARY_DIR}/ctest_b10_b11)
set_tests_properties(b10_b11_research_campaigns PROPERTIES TIMEOUT 40)
add_executable(phaseshift_scaling_provenance_tests tests/scaling_provenance.cpp)
target_link_libraries(phaseshift_scaling_provenance_tests PRIVATE phaseshift_core)
add_executable(phaseshift_plugin_fingerprint_tests tests/plugin_fingerprint.cpp)
target_link_libraries(phaseshift_plugin_fingerprint_tests PRIVATE phaseshift_core)
add_test(NAME native_plugin_fingerprint COMMAND phaseshift_plugin_fingerprint_tests ${CMAKE_CURRENT_BINARY_DIR}/ctest_plugin_fingerprint $<TARGET_FILE:phaseshift_example_plugin>)
set_tests_properties(native_plugin_fingerprint PROPERTIES TIMEOUT 30)
configure_file(tests/run_null_scorecard.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_null_scorecard.cmake @ONLY)
add_test(NAME b02_null_ablation_scorecard COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_null_scorecard.cmake)
set_tests_properties(b02_null_ablation_scorecard PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)

# SOTA independent audit F-NEW-01/02/03: typed JSON and absolute wall budget.
configure_file(tests/run_sota_audit_falsifiers.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_sota_audit_falsifiers.cmake @ONLY)
add_test(NAME sota_audit_falsifiers COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_sota_audit_falsifiers.cmake)
set_tests_properties(sota_audit_falsifiers PROPERTIES TIMEOUT 20 RUN_SERIAL TRUE)

# Independent v1.0 contract falsifiers: frozen categorical coordinates,
# reproducible seeded strata, failed holdout control, and absolute worker limits.
foreach(mode IN ITEMS 1 2 3)
 add_library(phaseshift_audit_boundary_${mode} SHARED tests/audit_boundary_plugin.c)
 target_include_directories(phaseshift_audit_boundary_${mode} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
 target_compile_definitions(phaseshift_audit_boundary_${mode} PRIVATE AUDIT_MODE=${mode})
 set_target_properties(phaseshift_audit_boundary_${mode} PROPERTIES PREFIX "" OUTPUT_NAME "audit_boundary_${mode}")
endforeach()
add_executable(phaseshift_full_v1_contract tests/full_v1_contract.cpp)
target_link_libraries(phaseshift_full_v1_contract PRIVATE phaseshift_core)
add_test(NAME full_v1_contract COMMAND phaseshift_full_v1_contract
 ${CMAKE_CURRENT_BINARY_DIR}/ctest_full_v1_contract
 $<TARGET_FILE:phaseshift_audit_boundary_1>
 $<TARGET_FILE:phaseshift_audit_boundary_2>
 $<TARGET_FILE:phaseshift_audit_boundary_3>)
set_tests_properties(full_v1_contract PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)

# Instrumentable synthetic knee. Runs the real worker pipeline with an
# oracle-correct plugin; a deterministic test signal, never real SOTA evidence.
add_library(phaseshift_scaling_knee SHARED tests/scaling_knee_plugin.c)
target_include_directories(phaseshift_scaling_knee PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(phaseshift_scaling_knee PROPERTIES PREFIX "" OUTPUT_NAME "scaling_knee")
configure_file(tests/run_scaling_knee.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_scaling_knee.cmake @ONLY)
add_test(NAME scaling_independent_holdout_knee COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_scaling_knee.cmake)
set_tests_properties(scaling_independent_holdout_knee PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)

# Natural CLI behaviors including error exits and help/version/list surface.
configure_file(tests/run_cli_surface.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_cli_surface.cmake @ONLY)
add_test(NAME cli_surface_contract COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_cli_surface.cmake)
set_tests_properties(cli_surface_contract PROPERTIES TIMEOUT 20)

# Fresh independent post-v1.0 audit: field-level invariants, worker-backed
# plugin provenance, wrong-answer scaling quarantine and categorical witness.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME fresh_audit_regressions
 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/fresh_audit_regressions.py
 --repo ${CMAKE_CURRENT_SOURCE_DIR} --binary $<TARGET_FILE:phaseshift>)
set_tests_properties(fresh_audit_regressions PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)

# Independent opt-in v2 lazy-space falsification suite.
add_executable(phaseshift_lazy_search_v2_tests tests/lazy_search_v2.cpp)
target_link_libraries(phaseshift_lazy_search_v2_tests PRIVATE phaseshift_core)
add_test(NAME lazy_search_v2 COMMAND phaseshift_lazy_search_v2_tests)

configure_file(tests/run_v2_lazy.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_v2_lazy.cmake @ONLY)
add_test(NAME lazy_search_v2_cli COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_v2_lazy.cmake)
set_tests_properties(lazy_search_v2_cli PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)

add_executable(phaseshift_boundary_v2_tests tests/boundary_v2.cpp)
target_link_libraries(phaseshift_boundary_v2_tests PRIVATE phaseshift_core)
add_test(NAME multi_change_v2_oracle COMMAND phaseshift_boundary_v2_tests)

add_executable(phaseshift_scheduler_v2_tests tests/scheduler_v2.cpp)
target_link_libraries(phaseshift_scheduler_v2_tests PRIVATE phaseshift_core)
add_test(NAME hierarchical_policy_v2 COMMAND phaseshift_scheduler_v2_tests)
configure_file(tests/run_v2_hierarchical.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_v2_hierarchical.cmake @ONLY)
add_test(NAME hierarchical_policy_v2_cli COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_v2_hierarchical.cmake)
set_tests_properties(hierarchical_policy_v2_cli PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)

# Small frozen calibration comparison through actual CLI/worker/SQLite. It never
# enforces a positive result; failed discoveries remain benchmark evidence.
configure_file(benchmarks/v2/run_equal_case_budget.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/v2_equal_budget.cmake @ONLY)
add_test(NAME v2_frozen_equal_case_budget_smoke COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/v2_equal_budget.cmake)
set_tests_properties(v2_frozen_equal_case_budget_smoke PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)

configure_file(tests/run_v2_replicates.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_v2_replicates.cmake @ONLY)
add_test(NAME v2_replicated_seeds_resume COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_v2_replicates.cmake)
set_tests_properties(v2_replicated_seeds_resume PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)

add_executable(phaseshift_interaction_v2_tests tests/interaction_v2.cpp)
target_link_libraries(phaseshift_interaction_v2_tests PRIVATE phaseshift_core)
add_test(NAME factorial_interaction_v2 COMMAND phaseshift_interaction_v2_tests)

configure_file(tests/run_v2_factorial.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_v2_factorial.cmake @ONLY)
add_test(NAME v2_conditional_factorial_discovery COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_v2_factorial.cmake)
set_tests_properties(v2_conditional_factorial_discovery PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)

configure_file(tests/run_v2_multi.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_v2_multi.cmake @ONLY)
add_test(NAME v2_multi_change_worker_smoke COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_v2_multi.cmake)
set_tests_properties(v2_multi_change_worker_smoke PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)

# Exact opt-in structural feasible-union oracle and resumed SQLite evidence.
add_executable(phaseshift_admissible_v2_tests tests/conditional_subspaces_v2.cpp)
target_link_libraries(phaseshift_admissible_v2_tests PRIVATE phaseshift_core)
add_test(NAME admissible_subspace_v2_oracle COMMAND phaseshift_admissible_v2_tests)
configure_file(tests/run_v2_admissible.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_v2_admissible.cmake @ONLY)
add_test(NAME v2_admissible_subspaces_resume COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_v2_admissible.cmake)
set_tests_properties(v2_admissible_subspaces_resume PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)

# Distinct opt-in v2 per-axis stratification with exactly-once sparse draws.
add_executable(phaseshift_stratified_v2_tests tests/stratified_v2.cpp)
target_link_libraries(phaseshift_stratified_v2_tests PRIVATE phaseshift_core)
add_test(NAME stratified_v2_oracle COMMAND phaseshift_stratified_v2_tests)
configure_file(tests/run_v2_stratified.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_v2_stratified.cmake @ONLY)
add_test(NAME v2_stratified_resume COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_v2_stratified.cmake)
set_tests_properties(v2_stratified_resume PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)

# Independent exhaustive score oracle for pruned changepoint candidate generation.
add_executable(phaseshift_pelt_v2_tests tests/pelt_v2.cpp)
target_link_libraries(phaseshift_pelt_v2_tests PRIVATE phaseshift_core)
add_test(NAME pelt_v2_differential COMMAND phaseshift_pelt_v2_tests)
set_tests_properties(pelt_v2_differential PROPERTIES TIMEOUT 90)

# PELT opt-in CLI/worker/SQLite smoke, under same oracle and H1 gates.
configure_file(tests/run_v2_pelt.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_v2_pelt.cmake @ONLY)
add_test(NAME v2_pelt_worker_smoke COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_v2_pelt.cmake)
set_tests_properties(v2_pelt_worker_smoke PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)

# Predeclared four-policy real-worker cliff and constant-null calibration.
configure_file(benchmarks/v2/run_policy_challenge.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/v2_policy_challenge.cmake @ONLY)
add_test(NAME v2_four_policy_cliff_null_challenge COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/v2_policy_challenge.cmake)
set_tests_properties(v2_four_policy_cliff_null_challenge PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)

# Timed-region CPU budget: per-case bound with replay-stable terminal state.
configure_file(tests/run_v2_cpu_budget.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_v2_cpu_budget.cmake @ONLY)
add_test(NAME v2_sampled_cpu_budget_resume COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_v2_cpu_budget.cmake)
set_tests_properties(v2_sampled_cpu_budget_resume PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)

# Explicit opt-in v7->v8 migration; legacy v1 v7 left bitwise untouched.
add_executable(phaseshift_v2_schema_migration_tests tests/v2_schema_migration.cpp)
target_link_libraries(phaseshift_v2_schema_migration_tests PRIVATE phaseshift_core)
add_test(NAME v2_schema8_non_destructive_migration COMMAND phaseshift_v2_schema_migration_tests ${CMAKE_CURRENT_BINARY_DIR}/ctest_v2_schema_migration)

# M7 fixed predeclared multi-hypothesis Holm family, stored in opt-in schema v9.
add_executable(phaseshift_campaign_v2_tests tests/campaign_v2.cpp)
target_link_libraries(phaseshift_campaign_v2_tests PRIVATE phaseshift_core)
add_test(NAME campaign_v2_frozen_holm COMMAND phaseshift_campaign_v2_tests ${CMAKE_CURRENT_BINARY_DIR}/ctest_campaign_v2)
set_tests_properties(campaign_v2_frozen_holm PROPERTIES TIMEOUT 25)

# Real CLI gateway through registry, with duplicate and premature-finalize rejection.
add_executable(phaseshift_campaign_cli_fixture tests/campaign_cli_fixture.cpp)
target_link_libraries(phaseshift_campaign_cli_fixture PRIVATE phaseshift_core)
configure_file(tests/run_v2_campaign_cli.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_v2_campaign_cli.cmake @ONLY)
add_test(NAME v2_campaign_cli_lifecycle COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_v2_campaign_cli.cmake)
set_tests_properties(v2_campaign_cli_lifecycle PROPERTIES TIMEOUT 35 RUN_SERIAL TRUE)

# Fixed-family two-hypothesis full CPU-worker/independent-holdout campaign.
add_executable(phaseshift_campaign_worker_fixture tests/campaign_worker_fixture.cpp)
target_link_libraries(phaseshift_campaign_worker_fixture PRIVATE phaseshift_core)
configure_file(tests/run_v2_campaign_worker.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_v2_campaign_worker.cmake @ONLY)
add_test(NAME v2_campaign_two_real_worker_hypotheses COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_v2_campaign_worker.cmake)
set_tests_properties(v2_campaign_two_real_worker_hypotheses PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)

# M6 independent worker-v2 pilot and original v1 oracle comparison.
add_executable(phaseshift_measurement_v2_tests tests/measurement_v2.cpp)
target_link_libraries(phaseshift_measurement_v2_tests PRIVATE phaseshift_core)
add_test(NAME calibrated_batch_v2_unit COMMAND phaseshift_measurement_v2_tests)

# V2 opt-in calibrated pilot through real worker, immutable config and SQLite.
configure_file(tests/run_v2_measurement.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_v2_measurement.cmake @ONLY)
add_test(NAME v2_calibrated_batch_worker_resume COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_v2_measurement.cmake)
set_tests_properties(v2_calibrated_batch_worker_resume PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)

# Adversarial family sample isolation: H1 rows cannot seal H2 with same boundary.
add_executable(phaseshift_confirmation_seed_v2_tests tests/confirmation_seed_v2.cpp)
target_link_libraries(phaseshift_confirmation_seed_v2_tests PRIVATE phaseshift_core)
add_test(NAME confirmation_prescribed_seed_isolation COMMAND phaseshift_confirmation_seed_v2_tests ${CMAKE_CURRENT_BINARY_DIR}/ctest_confirmation_seed_v2)

# M7 null stress campaign with correct discrete marginal p-values and dependence.
add_executable(phaseshift_fwer_null_tests tests/fwer_correlated_null.cpp)
target_link_libraries(phaseshift_fwer_null_tests PRIVATE phaseshift_core)
add_test(NAME v2_correlated_null_holm_smoke COMMAND phaseshift_fwer_null_tests)
set_tests_properties(v2_correlated_null_holm_smoke PROPERTIES TIMEOUT 25)

# M9 failpoint injection around sample, decision and budget commit boundary.
add_executable(phaseshift_v2_atomicity_tests tests/v2_atomicity.cpp)
target_link_libraries(phaseshift_v2_atomicity_tests PRIVATE phaseshift_core)
add_test(NAME v2_atomic_case_decision_budget_recovery COMMAND phaseshift_v2_atomicity_tests ${CMAKE_CURRENT_BINARY_DIR}/ctest_v2_atomic)
set_tests_properties(v2_atomic_case_decision_budget_recovery PROPERTIES TIMEOUT 30)

add_executable(phaseshift_hybrid_v2_tests tests/hybrid_v2.cpp)
target_link_libraries(phaseshift_hybrid_v2_tests PRIVATE phaseshift_core)
add_test(NAME hybrid_mixed_v2_unit COMMAND phaseshift_hybrid_v2_tests)
configure_file(tests/run_v2_hybrid.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_v2_hybrid.cmake @ONLY)
add_test(NAME hybrid_mixed_v2_replay COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_v2_hybrid.cmake)
set_tests_properties(hybrid_mixed_v2_replay PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)

add_executable(phaseshift_witness_paths_v2_tests tests/witness_paths_v2.cpp)
target_link_libraries(phaseshift_witness_paths_v2_tests PRIVATE phaseshift_core)
add_test(NAME v2_named_witness_path_isolation COMMAND phaseshift_witness_paths_v2_tests)

# Linux-only real OS kill (SIGKILL), WAL recovery, retry and replay budget checks.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
 add_executable(phaseshift_v2_os_crash_tests tests/v2_os_crash.cpp)
 target_link_libraries(phaseshift_v2_os_crash_tests PRIVATE phaseshift_core)
 add_test(NAME v2_sigkill_transaction_recovery
  COMMAND phaseshift_v2_os_crash_tests ${CMAKE_CURRENT_BINARY_DIR}/ctest_v2_os_kill)
 set_tests_properties(v2_sigkill_transaction_recovery PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
endif()

add_executable(phaseshift_confirmation_seed_bounds_v2_tests tests/confirmation_seed_bounds_v2.cpp)
target_link_libraries(phaseshift_confirmation_seed_bounds_v2_tests PRIVATE phaseshift_core)
add_test(NAME v2_confirmation_seed_window_overflow COMMAND phaseshift_confirmation_seed_bounds_v2_tests)

add_executable(phaseshift_multi_slope_v2_tests tests/slope_v2.cpp)
target_link_libraries(phaseshift_multi_slope_v2_tests PRIVATE phaseshift_core)
add_test(NAME v2_piecewise_linear_slope_oracle COMMAND phaseshift_multi_slope_v2_tests)
configure_file(tests/run_v2_slope.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_v2_slope.cmake @ONLY)
add_test(NAME v2_slope_discovery_only_worker COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_v2_slope.cmake)
set_tests_properties(v2_slope_discovery_only_worker PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)

configure_file(benchmarks/v2/run_sampled_cpu_challenge.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/v2_sampled_cpu_challenge.cmake @ONLY)
add_test(NAME v2_sampled_worker_cpu_four_policy_challenge
 COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/v2_sampled_cpu_challenge.cmake)
set_tests_properties(v2_sampled_worker_cpu_four_policy_challenge PROPERTIES TIMEOUT 150 RUN_SERIAL TRUE)

add_executable(phaseshift_v2_multi_witness_tests tests/v2_multi_witness.cpp)
target_link_libraries(phaseshift_v2_multi_witness_tests PRIVATE phaseshift_core)
add_test(NAME v2_real_minimize_multi_hypothesis_artifact_isolation
 COMMAND phaseshift_v2_multi_witness_tests ${CMAKE_CURRENT_BINARY_DIR}/ctest_v2_multi_witness)
set_tests_properties(v2_real_minimize_multi_hypothesis_artifact_isolation PROPERTIES TIMEOUT 70 RUN_SERIAL TRUE)

add_executable(phaseshift_block_drift_v2_tests tests/measurement_drift_v2.cpp)
target_link_libraries(phaseshift_block_drift_v2_tests PRIVATE phaseshift_core)
add_test(NAME v2_descriptive_block_drift_oracle COMMAND phaseshift_block_drift_v2_tests)

# Weighted v2 prior policy: lazy admissible subspaces, no-replacement
# property oracle, immutable selection and SQLite replay in a real worker run.
add_executable(phaseshift_weighted_v2_tests tests/weighted_v2.cpp)
target_link_libraries(phaseshift_weighted_v2_tests PRIVATE phaseshift_core)
add_test(NAME weighted_v2_no_replacement_oracle COMMAND phaseshift_weighted_v2_tests)
configure_file(tests/run_v2_weighted.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/run_v2_weighted.cmake @ONLY)
add_test(NAME weighted_v2_resume_admissible
 COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_v2_weighted.cmake)
set_tests_properties(weighted_v2_resume_admissible PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)

add_test(NAME weighted_v2_invalid_config_contract
 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_v2_weighted_invalid.py
 --binary ${CMAKE_CURRENT_BINARY_DIR}/phaseshift
 --config ${CMAKE_CURRENT_SOURCE_DIR}/configs/v2/weighted_admissible.json)
set_tests_properties(weighted_v2_invalid_config_contract PROPERTIES TIMEOUT 90)

# M2: bounded exact disjoint decomposition of conditional implication rules.
add_executable(phaseshift_conditional_rule_v2_tests tests/conditional_rules_v2.cpp)
target_link_libraries(phaseshift_conditional_rule_v2_tests PRIVATE phaseshift_core)
add_test(NAME conditional_rules_v2_exhaustive_oracle COMMAND phaseshift_conditional_rule_v2_tests)

configure_file(tests/run_v2_conditional_rules.cmake.in
 ${CMAKE_CURRENT_BINARY_DIR}/run_v2_conditional_rules.cmake @ONLY)
add_test(NAME conditional_rules_v2_cli_replay
 COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_v2_conditional_rules.cmake)
set_tests_properties(conditional_rules_v2_cli_replay PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)

add_executable(phaseshift_v2_evidence_view_tests tests/evidence_view_v2.cpp)
target_link_libraries(phaseshift_v2_evidence_view_tests PRIVATE phaseshift_core)
add_test(NAME v2_committed_discovery_view_oracle COMMAND phaseshift_v2_evidence_view_tests)

configure_file(benchmarks/v2/run_dual_cap_challenge.cmake.in
 ${CMAKE_CURRENT_BINARY_DIR}/run_dual_cap_challenge.cmake @ONLY)
add_test(NAME v2_dual_cap_full_cli_accounting
 COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_dual_cap_challenge.cmake)
set_tests_properties(v2_dual_cap_full_cli_accounting PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)

add_executable(phaseshift_cpu_usage_v2_tests tests/cpu_usage_v2.cpp)
target_link_libraries(phaseshift_cpu_usage_v2_tests PRIVATE phaseshift_core)
add_test(NAME v2_linux_parent_waited_worker_cpu_oracle COMMAND phaseshift_cpu_usage_v2_tests)

add_executable(phaseshift_scheduler_heap_v2_tests tests/scheduler_heap_v2.cpp)
target_link_libraries(phaseshift_scheduler_heap_v2_tests PRIVATE phaseshift_core)
add_test(NAME v2_scheduler_heap_matches_independent_full_scan COMMAND phaseshift_scheduler_heap_v2_tests)

add_executable(phaseshift_fixed_factorial_holdout_tests tests/factorial_holdout_v2.cpp)
target_link_libraries(phaseshift_fixed_factorial_holdout_tests PRIVATE phaseshift_core)
add_test(NAME v2_fixed_independent_factorial_holdout_math
 COMMAND phaseshift_fixed_factorial_holdout_tests)

configure_file(benchmarks/v2/run_real_family_dual_cap.cmake.in
 ${CMAKE_CURRENT_BINARY_DIR}/run_real_family_dual_cap.cmake @ONLY)
add_test(NAME v2_real_sorting_topk_graph_dual_cap
 COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_real_family_dual_cap.cmake)
set_tests_properties(v2_real_sorting_topk_graph_dual_cap PROPERTIES TIMEOUT 240 RUN_SERIAL TRUE)

add_test(NAME v2_dual_cap_scorecard_falsification
 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/summarize_dual_cap_v2.py)
set_tests_properties(v2_dual_cap_scorecard_falsification PROPERTIES
 ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/v2" TIMEOUT 30)

configure_file(tests/run_v2_factorial_holdout.cmake.in
 ${CMAKE_CURRENT_BINARY_DIR}/run_v2_factorial_holdout.cmake @ONLY)
add_test(NAME v2_factorial_independent_real_worker_holdout
 COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_v2_factorial_holdout.cmake)
set_tests_properties(v2_factorial_independent_real_worker_holdout PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
