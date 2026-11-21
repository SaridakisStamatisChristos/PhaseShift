#!/usr/bin/env python3
"""Frozen four-policy synthetic cliff/null diagnostic: not a SOTA claim."""
import argparse
import csv
import hashlib
import json
import pathlib
from equal_budget import run

POLICIES=("adaptive_partition","uniform_v2","stratified_v2","hierarchical_boundary_v2")
FIXTURES=("single_cliff","constant_null")

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--binary",type=pathlib.Path,required=True)
    p.add_argument("--out",type=pathlib.Path,required=True)
    p.add_argument("--seeds",default="31,47,83")
    p.add_argument("--cases",type=int,default=20)
    args=p.parse_args()
    if not 4<=args.cases<=64:p.error("--cases must be 4..64")
    seeds=[int(x) for x in args.seeds.split(",")]
    if not seeds or len(set(seeds))!=len(seeds) or any(x<0 or x>2**31-1 for x in seeds):
        p.error("seeds must be distinct 0..2^31-1 integers")
    if args.out.exists():p.error("cannot overwrite benchmark evidence")
    args.out.mkdir(parents=True)
    plan={"schema":1,"name":"four-policy-cliff-null","fixtures":FIXTURES,
          "policies":POLICIES,"seeds":seeds,"cases":args.cases,
          "selection_budget":"equal case ceiling",
          "equal_cpu_budget":False,"equal_wall_budget":False,
          "ground_truth":{"single_cliff":24,"constant_null":None},
          "exclusions":[],"claim":"synthetic diagnostic only"}
    plan["sha256"]=hashlib.sha256(json.dumps(plan,sort_keys=True).encode()).hexdigest()
    (args.out/"frozen_plan.json").write_text(json.dumps(plan,indent=2)+"\n")
    rows=[]
    for fixture in FIXTURES:
        directory=args.out/fixture
        directory.mkdir()
        for seed in seeds:
            for policy in POLICIES:
                result=run(args.binary.resolve(),directory,policy,seed,args.cases,
                           negative=fixture=="constant_null")
                result["fixture"]=fixture
                result["false_positive"]=int(
                    fixture=="constant_null" and
                    result["final_status"]=="CONFIRMED" and result["confirmation_seals"]>0)
                if fixture=="constant_null":
                    result["confirmed_true_boundary"]=0
                    result["localization_error"]=None
                rows.append(result)
                print(json.dumps(result,sort_keys=True))
    assert len(rows)==len(FIXTURES)*len(POLICIES)*len(seeds)
    for row in rows:
        assert row["discovery_cases"]>=0 and row["discovery_wall_ns"]>0
        assert row["discovery_worker_cpu_ns"] is None or row["discovery_worker_cpu_ns"]>=0
        assert not(row["fixture"]=="constant_null" and row["confirmed_true_boundary"])
    with (args.out/"results.csv").open("w",newline="") as handle:
        writer=csv.DictWriter(handle,fieldnames=list(rows[0]))
        writer.writeheader();writer.writerows(rows)
    (args.out/"results.json").write_text(json.dumps({
        "plan_sha256":plan["sha256"],"rows":rows,"sota_claim":False,
        "budget_warning":"equal case ceilings do not equalize actual worker CPU or wall"
    },indent=2)+"\n")

if __name__=="__main__":
    main()
