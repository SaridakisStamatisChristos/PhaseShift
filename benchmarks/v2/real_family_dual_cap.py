#!/usr/bin/env python3
"""Frozen real-family (sorting/Top-K/graph) dual-ceiling diagnostic.

No planted boundary assumed; correctness is delegated to PhaseShift's actual
workers and independent oracles. These data are *not* SOTA evidence, and no
matched external competitor is represented. OS resource costs retain startup.
"""
import argparse
import csv
import hashlib
import json
import pathlib

from dual_cap_challenge import run_one

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCENARIOS = {
    "sorting": "configs/sorting_quicksort.json",
    "topk": "configs/topk_fraction.json",
    "graph": "configs/graph_density.json",
}
POLICIES = ("adaptive_partition", "uniform_v2", "stratified_v2",
            "hybrid_mixed_v2")


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--binary",type=pathlib.Path,required=True)
    ap.add_argument("--out",type=pathlib.Path,required=True)
    ap.add_argument("--seeds",default="31,47,83")
    ap.add_argument("--cases",type=int,default=6)
    ap.add_argument("--cpu-budget-ns",type=int,default=5_000_000_000)
    ap.add_argument("--wall-budget-ns",type=int,default=20_000_000_000)
    a=ap.parse_args()
    if not a.binary.is_file():ap.error("binary must exist")
    if not 4<=a.cases<=32:ap.error("cases must be 4..32")
    if not 1_000_000<=a.cpu_budget_ns<=2**63-1:ap.error("invalid CPU budget")
    if not 1_000_000<=a.wall_budget_ns<=86400*10**9:ap.error("invalid wall budget")
    try:seeds=[int(x) for x in a.seeds.split(",")]
    except ValueError:ap.error("invalid seeds")
    if not seeds or len(seeds)!=len(set(seeds)) or any(s<0 or s>2**31-1 for s in seeds):
        ap.error("seeds must be distinct nonnegative 31-bit integers")
    if a.out.exists():ap.error("refusing to overwrite immutable real-family evidence")
    a.out.mkdir(parents=True)
    configs={}
    hashes={}
    for name,path in SCENARIOS.items():
        payload=(ROOT/path).read_bytes()
        configs[name]=json.loads(payload)
        hashes[name]=hashlib.sha256(payload).hexdigest()
    plan={
        "schema":1,"name":"real-family-cli-dual-cap-v2",
        "fixtures":list(SCENARIOS),"config_source_sha256":hashes,
        "policies":list(POLICIES),"seeds":seeds,"cases":a.cases,
        "cpu_ceiling_ns":a.cpu_budget_ns,"wall_ceiling_ns":a.wall_budget_ns,
        "accounting":"Linux wait4/getrusage self and RUSAGE_CHILDREN sampled after each CLI case",
        "stop":"both ceilings at case boundary; report full overrun",
        "ground_truth_boundaries":None,"external_competitors_included":False,
        "equal_consumed_resources":False,"sota_claim":False,"no_exclusions":True
    }
    plan["sha256"]=hashlib.sha256(json.dumps(plan,sort_keys=True).encode()).hexdigest()
    (a.out/"frozen_plan.json").write_text(json.dumps(plan,indent=2,sort_keys=True)+"\n")
    rows=[]
    for fixture,cfg in configs.items():
        for seed in seeds:
            for policy in POLICIES:
                row=run_one(a.binary.resolve(),a.out,policy,fixture,seed,
                            a.cases,a.cpu_budget_ns,a.wall_budget_ns,cfg)
                rows.append(row)
                print(json.dumps(row,sort_keys=True),flush=True)
    with (a.out/"results.csv").open("w",newline="") as handle:
        writer=csv.DictWriter(handle,fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (a.out/"results.json").write_text(json.dumps({
        "plan_sha256":plan["sha256"],"rows":rows,
        "complete":not any(r["had_failure"] for r in rows),
        "sota_claim":False,
        "limitations":"No exact ground-truth performance boundary in real families; "
         "OS-accounted CPU/wall ceilings, not equality of consumed compute. "
         "v1/v2 case seed policies differ. No external baseline."
    },indent=2,sort_keys=True)+"\n")
    if any(r["had_failure"] for r in rows):
        raise SystemExit("real-family challenge has failed commands; evidence retained")


if __name__=="__main__":
    main()
