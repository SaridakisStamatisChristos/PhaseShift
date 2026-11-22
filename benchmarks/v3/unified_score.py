#!/usr/bin/env python3
"""V3-01b unified *audit and comparison*, never a fabricated common SOTA rank.

Each source is independently reverified from raw SQLite/command evidence.
V1/V2 holdout confirmations and V3 unconfirmed timing nominations are distinct
endpoints. Cross-engine superiority is ineligible until a shared confirmation
protocol and exactly matched workloads exist.
"""
import argparse
import json
from pathlib import Path

from protocol import sha256_file, seal
import run_scientific
import run_multidim

def need(ok,message):
    if not ok:
        raise ValueError(message)

def read(path):
    return json.loads(path.read_text())

def comparison_gate(left,right):
    need(left.get("engine") and right.get("engine") and\n         left.get("endpoint") and right.get("endpoint"),"invalid inputs")
    reasons=[]
    if left["endpoint"]!=right["endpoint"]:
        reasons.append("Different endpoints: independent confirmed work-unit boundary versus unconfirmed timing nomination")
    if left["fixture"]!=right["fixture"]:
        reasons.append("Different frozen task/case geometry")
    if left["suite"]!=right["suite"]:
        reasons.append("Development/evaluation mix is forbidden")
    if left["budget_protocol"]!=right["budget_protocol"]:
        reasons.append("Case-cost accounting differs from fixed matched-pair accounting")
    if left["source_commit"]!=right["source_commit"]:
        reasons.append("Different source revisions")
    if left["engine"]!=right["engine"]:
        reasons.append("Different candidate-generation and confirmation workflows")
    return {"comparable":not reasons,"reasons":reasons,"winner":None,
            "superiority_claim":False}

def normalized_incumbent(verified):
    plan=verified["source_commit"]
    out=[]
    for row in verified["summary"]:
        out.append({"engine":"incumbent-v1-v2","suite":verified["suite"] if "suite" in verified else None,
                    "source_commit":plan,"fixture":row["fixture"],
                    "policy":row["policy"],"endpoint":"independent_work_unit_confirmation"
                     if row["curves"] is not None else "unlabelled_worker",
                    "budget_protocol":"case_boundary_cpu_wall",
                    "trials":row["trials"],"failed":row["failed"],
                    "over_ceiling":row["over_ceiling"],
                    "truth_matches":row["known_truth_matches"],
                    "false_promotions":row["false_promotions"],
                    "curves":row["curves"]})
    return out

def normalized_v3(verified):
    out=[]
    paired=[]
    for row in verified["summary"]:
        if "comparison" in row:
            paired.append(row)
            continue
        out.append({"engine":"v3-matched-pairs","suite":verified["suite"],
                    "source_commit":verified["source_commit"],"fixture":row["fixture"],
                    "policy":row["policy"],
                    "endpoint":"unconfirmed_timing_nomination",
                    "budget_protocol":"fixed_pair_count_cpu_wall",
                    "trials":row["trials"],"failed":row["failed"],
                    "over_ceiling":row["over_ceiling"],
                    "truth_matches":row["total_distinct_true_hits"],
                    "false_promotions":row["false_nomination_observations"],
                    "curves":row["curves"]})
    return out,paired

def build(incumbent_root,multidim_root):
    incumbent=run_scientific.verify(incumbent_root)
    v3=run_multidim.verify(multidim_root)
    old_plan=read(incumbent_root/"frozen_plan.json")
    new_plan=read(multidim_root/"frozen_plan.json")
    need(read(incumbent_root/"scorecard.json")==incumbent and
         read(multidim_root/"scorecard.json")==v3,
         "scorecard differs from independent raw data")
    need(old_plan["suite"]==new_plan["suite"],"mixed development/evaluation cohorts")
    left=normalized_incumbent(incumbent)
    for row in left:
        row["suite"]=old_plan["suite"]
    right,paired=normalized_v3(v3)
    need(all(x["suite"]==old_plan["suite"] for x in left+right),"suite conflict")
    gates=[{"left":l["fixture"]+"/"+l["policy"],
            "right":r["fixture"]+"/"+r["policy"],
            "decision":comparison_gate(l,r)}
           for l in left for r in right]
    need(all(not g["decision"]["comparable"] for g in gates),
         "incomparable protocol accidentally admitted")
    return {"schema":1,"protocol":"phaseshift.v3b.unified-evidence.1",
            "suite":old_plan["suite"],"eligible_for_sota":False,
            "sota_claim":False,"winner":None,
            "source_provenance":[
                {"engine":"incumbent-v1-v2","root_plan_sha256":old_plan["sha256"],
                 "source_commit":incumbent["source_commit"],
                 "scorecard_sha256":sha256_file(incumbent_root/"scorecard.json")},
                {"engine":"v3-matched-pairs","root_plan_sha256":new_plan["sha256"],
                 "source_commit":v3["source_commit"],
                 "scorecard_sha256":sha256_file(multidim_root/"scorecard.json")}],
            "cohorts":left+right,
            "within_v3_paired_seed_comparisons":paired,
            "cross_engine_comparisons":gates,
            "limitations":["No direct winner: different confirmation endpoints and resource semantics",
                           "Only synthetic analytic work-unit jumps carry labelled truth",
                           "Observed CLI-subtree CPU is not complete process-tree accounting",
                           "V3 paired discovery nominations are not fixed-family confirmations",
                           "No multiple-testing-adjusted population SOTA claims",
                           "All costs and failed campaigns retained within their source runs"]}

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--incumbent-root",required=True,type=Path)
    p.add_argument("--multidim-root",required=True,type=Path)
    p.add_argument("--out",required=True,type=Path)
    p.add_argument("--verify-only",action="store_true")
    a=p.parse_args()
    result=build(a.incumbent_root.resolve(),a.multidim_root.resolve())
    if a.verify_only:
        need(read(a.out)==result,"unified scorecard drift")
    else:
        need(not a.out.exists(),"refusing to overwrite unified evidence")
        a.out.write_text(json.dumps(result,sort_keys=True,indent=2,allow_nan=False)+"\n")
    print("PASS independent cross-engine evidence audit; superiority gate CLOSED")

if __name__=="__main__":
    main()
