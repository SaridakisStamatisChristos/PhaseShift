#!/usr/bin/env python3
"""Independent V3-01f two-member null familywise-error development report."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from confirmation_protocol import require,read
from protocol import sha256_file
from run_multimember_null import manifest,reconstruct
from run_forced_null_calibration import upper_bound_if_independent


def assemble(roots,temporal):
    m,mh=manifest()
    expected={s["id"] for s in m["strata"]}
    require(set(roots)==expected,"missing, duplicated or extra multiedge evidence")
    require(temporal["protocol"]=="phaseshift.v3-01f.temporal-replay.1" and
            temporal["source_run_id"]==m["read_only_predecessor_run"] and
            temporal["family_count"]==256 and temporal["paired_measurements"]==5120 and
            temporal["sign_independence_certified"] is False and
            temporal["fwer_certified"] is False,
            "archived temporal audit not from complete v3-01e evidence")
    revisions=set()
    scenarios=[]
    provenance={}
    for s in m["strata"]:
        name=s["id"]
        root=roots[name]
        report=reconstruct(root)
        require(report==read(root/"scorecard.json") and
                report["stratum"]==name and
                report["diagnostic"] is False and
                report["complete_families"]==64 and
                report["observed_holdout_pairs"]==2560 and
                report["manifest_sha256"]==mh and
                report["population_fwer_certified"] is False and
                report["independence_empirically_validated"] is False and
                report["sota_claim"] is False,
                "tampered or incomplete two-member trial inventory")
        source=read(root/"frozen_plan.json")
        require(source["seed_inventory"]==s["seeds"] and
                source["diagnostic"] is False and
                source["manifest_sha256"]==mh,
                "modified multiedge source plan")
        revisions.add(source["source_commit"])
        rows=report["raw_results"]
        require(len(rows)==64 and
                [x["seed"] for x in rows]==s["seeds"] and
                all(x["frozen_exogenous_members"]==2 and
                    x["complete"] is True and
                    x["holdout_pairs"]==40 for x in rows),
                "early-stopped, untested or duplicate frozen member")
        count=report["nominal_familywise_rejections"]
        require(count==sum(x["familywise_any_nominal_rejection"] for x in rows),
                "familywise false-rejection count mismatch")
        if n:=len(rows):
            upper=upper_bound_if_independent(count,n)
            require(report["conditional_one_sided_95_upper_if_independent"]==upper,
                    "incorrect binomial inversion for conditional upper bound")
        scenarios.append({
          "stratum":name,"algorithm":s["algorithm"],
          "contention":s["contention"],"families":64,
          "frozen_members_per_family":2,
          "fresh_holdout_pairs":2560,
          "nominal_member_rejections":report["nominal_member_rejections"],
          "nominal_familywise_false_rejections":count,
          "nominal_familywise_rejection_fraction":count/64,
          "conditional_exact_95_upper_if_independent_families":upper,
          "families_with_run_ge_16":
             sum(any(v["risk_flag_sign_run_ge_16"] for v in x["temporal_sign_diagnostics"])
                 for x in rows),
          "families_with_abs_lag1_ge_half":
             sum(any(v["risk_flag_abs_lag1_ge_half"] for v in x["temporal_sign_diagnostics"])
                 for x in rows)
        })
        provenance[name]={
            "source_plan_sha256":sha256_file(root/"frozen_plan.json"),
            "scorecard_sha256":sha256_file(root/"scorecard.json"),
            "binary_sha256":source["binary_sha256"],
            "source_commit":source["source_commit"]
        }
    require(len(revisions)==1,"mixed scientific source revisions")
    return {
      "schema":1,"protocol":m["protocol"],"suite":m["suite"],
      "source_commit":revisions.pop(),"source_manifest_sha256":mh,
      "archived_temporal_source_run":m["read_only_predecessor_run"],
      "temporal_source_commit":temporal["source_commit"],
      "planned_families":128,"verified_complete_families":128,
      "planned_members":256,"verified_members":256,
      "planned_fresh_pairs":5120,"verified_fresh_pairs":5120,
      "nominal_familywise_false_rejections":
           sum(v["nominal_familywise_false_rejections"] for v in scenarios),
      "strata":scenarios,
      "archived_temporal_summary":[{
          "stratum":t["stratum"],
          "families":t["families"],
          "adjacent_sign_agreement_fraction":
              t["observed_adjacent_sign_agreement_fraction"],
          "families_abs_lag1_ge_half":t["families_abs_lag1_ge_half"],
          "families_sign_run_ge_16":t["families_sign_run_ge_16"],
          "families_half_shift_ge_log_1_2":
              t["families_half_shift_ge_log_1_2"]
        } for t in temporal["strata"]],
      "provenance":provenance,
      "wall_time_null_proven":False,
      "sign_independence_established":False,
      "family_independence_established":False,
      "population_fwer_certified":False,
      "external_sota_claim":False,
      "sealed_evaluation_touched":False,
      "limitations":[
          "Familywise rejections are conditional on the unvalidated one-sided sign model",
          "Each completed family tests two exogenous structural edges with Holm correction",
          "The first and second edges in one family share worker, environment and start conditions",
          "Repeated families on one CI host may be temporally correlated",
          "Equal deterministic loop-work endpoints do not prove equal wall-time distributions",
          "Temporal indicators are descriptive risk flags rather than calibrated independence tests",
          "Conditional exact 95% per-stratum upper bounds require independent families",
          "No evaluation seeds, real-world elapsed-time null calibration or SOTA claim"
      ]
    }


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--artifacts",type=Path,required=True)
    p.add_argument("--temporal",type=Path,required=True)
    p.add_argument("--out",type=Path,required=True)
    p.add_argument("--verify-only",action="store_true")
    a=p.parse_args()
    roots={}
    for folder in a.artifacts.iterdir():
        if folder.is_dir() and (folder/"frozen_plan.json").is_file():
            label=read(folder/"frozen_plan.json")["stratum"]
            require(label not in roots,"duplicate multiedge artifact")
            roots[label]=folder
    temporal=read(a.temporal)
    report=assemble(roots,temporal)
    if a.verify_only:
        require(read(a.out)==report,"aggregate scorecard not reproducible")
    else:
        with a.out.open("x") as f:
            json.dump(report,f,sort_keys=True,indent=2,allow_nan=False)
            f.write("\n")
    print(json.dumps({"status":"PASS","families":report["verified_complete_families"],
          "holdout_pairs":report["verified_fresh_pairs"],
          "nominal_familywise_rejections":report["nominal_familywise_false_rejections"],
          "fwer_certified":False},sort_keys=True),flush=True)


if __name__=="__main__":
    main()
