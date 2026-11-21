#!/usr/bin/env python3
"""Detector-only external PELT comparator against a frozen C++ sequence set.

Both algorithms use ordered scalar observations, squared-error segment cost,
min segment length 3 and per-change penalty 2.5 with full coordinate search.
PHASESHIFT's hard max_changes=3 differs from ruptures' unconstrained PELT;
only results with <=3 external changes share the feasible objective.
Search/acquisition, discovery CPU and full engine superiority are NOT tested.
"""
import argparse
import csv
import hashlib
import json
import math
import pathlib
import time

import numpy as np
import ruptures as rpt

PENALTY=2.5
MIN_SEGMENT=3
MAX_CPP_CHANGES=3

def sse(values,breaks):
    start=0
    residual=0.
    for stop in breaks:
        part=values[start:stop]
        if len(part)<MIN_SEGMENT:
            raise ValueError("external segmentation has segment below minimum")
        center=math.fsum(part)/len(part)
        residual+=math.fsum((x-center)**2 for x in part)
        start=stop
    if start!=len(values):
        raise ValueError("external segmentation fails to consume all points")
    return residual

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--dataset-root",required=True,type=pathlib.Path)
    parser.add_argument("--out",required=True,type=pathlib.Path)
    args=parser.parse_args()
    if args.out.exists():parser.error("refusing to replace immutable comparator output")
    if not (args.dataset_root/"datasets.csv").is_file():
        parser.error("no real C++ generated frozen dataset")
    groups={}
    with (args.dataset_root/"datasets.csv").open(newline="") as source:
        for item in csv.DictReader(source):
            key=(item["scenario"],int(item["seed"]))
            groups.setdefault(key,[]).append((int(item["index"]),float(item["value"])))
    cpp={}
    with (args.dataset_root/"observations.csv").open(newline="") as source:
        for row in csv.DictReader(source):
            if row["model_version"]=="piecewise_constant_v2":
                cpp[(row["scenario"],int(row["seed"]))]=row
    if len(groups)!=100 or set(groups)!=set(cpp):
        raise ValueError("external benchmark requires all 5 scenarios x 20 independent seeds")
    rows=[]
    for key in sorted(groups):
        entries=sorted(groups[key])
        if [i for i,_ in entries]!=list(range(48)):
            raise ValueError(f"missing, duplicate or unordered data index {key}")
        vals=[v for _,v in entries]
        if not all(math.isfinite(v) for v in vals):
            raise ValueError(f"nonfinite frozen observation {key}")
        started=time.perf_counter_ns()
        model=rpt.Pelt(model="l2",min_size=MIN_SEGMENT,jump=1).fit(
            np.asarray(vals,dtype=np.float64))
        bkps=model.predict(pen=PENALTY)
        elapsed=time.perf_counter_ns()-started
        if not bkps or bkps[-1]!=len(vals) or any(
             b<=0 or b>len(vals) for b in bkps):
            raise ValueError("invalid external segmentation")
        external_changes=bkps[:-1]
        comparable=len(external_changes)<=MAX_CPP_CHANGES
        computed=sse(vals,bkps)+PENALTY*len(external_changes)
        reference=cpp[key]
        cpp_penalized=float(reference["dp_sse"])
        rows.append({
            "scenario":key[0],"seed":key[1],
            "external_changes":external_changes,
            "external_change_count":len(external_changes),
            "external_penalized_sse":computed,
            "phaseshift_penalized_sse":cpp_penalized,
            "objective_feasible_set_matched":comparable,
            "absolute_objective_difference_if_matched":
                abs(computed-cpp_penalized) if comparable else None,
            "ruptures_call_wall_ns":elapsed,
            "not_comparable_to_full_engine_latency":True,
        })
    summary={
        "schema_version":1,
        "comparator":"ruptures.Pelt l2, min_size=3, jump=1, penalty=2.5",
        "library_version":rpt.__version__,
        "numpy_version":np.__version__,
        "phaseshift_cpp_max_changes":MAX_CPP_CHANGES,
        "dataset_sha256":hashlib.sha256(
            (args.dataset_root/"datasets.csv").read_bytes()).hexdigest(),
        "total_scenario_seed_rows":len(rows),
        "matched_feasible_set_rows":sum(x["objective_feasible_set_matched"] for x in rows),
        "ineligible_slope_rows":"20 slope-specific sequences use a different model/objective",
        "sota_claim":False,
        "search_policy_comparison":False,
        "limitations":[
          "Only fixed offline change-point segmentation, not acquisition, confirmation or system CPU",
          "Python interpreter and C++ benchmark timing are not comparable",
          "External PELT is unconstrained in K; rows with >3 cuts have unmatched feasible sets",
          "Numerical score inputs were serialized to CSV with finite decimal precision",
          "Results must be retained including negative/mismatching outputs"
        ],
        "rows":rows,
    }
    args.out.write_text(json.dumps(summary,indent=2,sort_keys=True)+"\n")
    print(json.dumps({k:v for k,v in summary.items() if k!="rows"},sort_keys=True))

if __name__=="__main__":
    main()
