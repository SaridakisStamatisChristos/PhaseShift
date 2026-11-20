#!/usr/bin/env bash
# Executed integration acceptance: no network, GPU or external AI service.
# Retains evidence in a fresh, unique runs/acceptance.* directory.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${PHASESHIFT_BIN:-$ROOT/build/phaseshift}"
[[ -x "$BIN" ]] || { echo "Missing executable: $BIN (build Release first)" >&2; exit 2; }
command -v jq >/dev/null || { echo 'Requires jq for report assertions' >&2; exit 2; }
mkdir -p "$ROOT/runs"
RUN="$(mktemp -d "$ROOT/runs/acceptance.XXXXXXXX")"
printf 'PHASESHIFT acceptance evidence: %s\n' "$RUN"
"$BIN" --version
"$BIN" doctor
"$BIN" list families >"$RUN/families.txt"
"$BIN" list algorithms >"$RUN/algorithms.txt"
for cfg in sorting_quicksort topk_fraction graph_density negative_control demo_transition; do
 "$BIN" validate --config "$ROOT/configs/$cfg.json"
done
"$BIN" demo --out "$RUN/demo"
jq -e '.summary.hypotheses[0].status == "MINIMIZED_CONFIRMED"' "$RUN/demo/report.json" >/dev/null
jq -e '.summary.hypotheses[0].kind == "SYNTHETIC_CALIBRATION"' "$RUN/demo/report.json" >/dev/null
"$BIN" explore --config "$ROOT/configs/sorting_quicksort.json" --out "$RUN/sorting"
"$BIN" explore --config "$ROOT/configs/topk_fraction.json" --out "$RUN/topk"
"$BIN" explore --config "$ROOT/configs/graph_density.json" --out "$RUN/graph"
"$BIN" explore --config "$ROOT/configs/negative_control.json" --out "$RUN/negative"
jq -e '.summary.hypotheses[0].status == "NO_CONFIRMED_BOUNDARY"' "$RUN/negative/report.json" >/dev/null
for family in sorting topk graph negative; do
 jq -e '.summary.invalid_samples == 0' "$RUN/$family/report.json" >/dev/null
 "$BIN" report --run "$RUN/$family" --format markdown,json,csv,html >/dev/null
 done
"$BIN" report --run "$RUN/demo" --format markdown,json,csv,html >/dev/null
case_id="$(jq -r '.raw_samples[0].case_id' "$RUN/demo/report.json")"
"$BIN" replay --run "$RUN/demo" --case "$case_id"
"$BIN" export --run "$RUN/demo" --out "$RUN/bundle"
"$BIN" verify-bundle --bundle "$RUN/bundle"
"$BIN" import --bundle "$RUN/bundle" --out "$RUN/imported"
"$BIN" replay --run "$RUN/imported" --case "$case_id"
"$BIN" compare --a "$RUN/graph" --b "$RUN/graph" >"$RUN/compare.txt"
"$BIN" scaling --run "$RUN/sorting" --out "$RUN/scaling" --seeds 3
jq -e '.scaling_regime_evidence.status | type == "string"' "$RUN/scaling/scaling.json" >/dev/null
"$BIN" intervene --run "$RUN/demo" --out "$RUN/intervention" --pairs 3
jq -e '.valid_seed_pairs == 3 and .causal_claim == false and .verdict == "MATCHED_MEASUREMENTS_COMPLETE"' "$RUN/intervention/interventions.json" >/dev/null
"$BIN" ablate --config "$ROOT/configs/adaptive_sparse.json" --out "$RUN/ablation"
jq -n --arg path "$RUN" '{acceptance_status:"PASS", run_directory:$path}' >"$RUN/acceptance.json"
echo "PASS: complete command-line acceptance and replay/export/import checks: $RUN"
