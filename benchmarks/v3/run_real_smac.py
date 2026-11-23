#!/usr/bin/env python3
"""Genuine SMAC3 2.4.1 ask/tell versus unchanged V18, on two real C++ workers.

Development-only independent selection plus fresh 20-pair test per nominated
edge. No claims that unlabelled real-algorithm boundaries are truth-confirmed,
or that unequal Python/worker process startup costs have been normalized.
"""
from __future__ import annotations
import argparse
import hashlib
from importlib.metadata import version as dist_version
import json
import math
from pathlib import Path
import resource
import subprocess
import time

from run_real_external import (manifest as optuna_manifest, read,write,require,
    config,allowed_edges,execute_run,audit_native_run,select,confirm,
    adjusted_pvalues,exact_sign_tail)
from protocol import sha256_file
from run_real_external import git_blob

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
PLAN=HERE/"real_smac_development.json"
POLICIES=("acquisition-v3-coverage","smac3_hpo")


def manifest():
    p=read(PLAN)
    old,by=optuna_manifest()
    require(p["schema"]==1 and
            p["protocol"]=="phaseshift.v3-external-smac24-real-algorithm-development.1" and
            p["suite"]=="development" and p["eligible_for_sota"] is False and
            p["evaluation_reused"] is False and p["sota_claim"] is False and
            p["source_manifest"]=="benchmarks/v3/real_external_development.json" and
            p["source_manifest_blob_sha1"]==git_blob(HERE/"real_external_development.json") and
            p["source_real_manifest_blob_sha1"]==old["source_real_blob_sha1"] and
            p["source_v18_code_blobs"]==old["pinned_native_source_blobs"] and
            p["smac_version"]=="2.4.1" and
            p["objective"]=="minimize 1/(1+abs(matched log speed ratio change)) on adjacent feasible structural edges; censored reward has unit cost" and
            p["policies"]==list(POLICIES) and
            p["pair_budget"]==old["pair_budget"]==12 and
            p["confirmation_pairs"]==old["confirmation_pairs"]==20 and
            p["min_practical_ratio"]==old["min_practical_ratio"]==1.2 and
            p["max_distinct_candidates"]==2 and p["alpha"]==0.05 and
            p["seeds"]==list(range(33001,33021)) and
            p["diagnostic_seeds"]==[33001] and
            p["scenarios"]==old["scenarios"] and
            p["truth_status"]=="real algorithm boundary location not independently ground-truth labeled",
            "SMAC3 real-worker frozen protocol mismatch")
    require(set(p["seeds"]).isdisjoint(old["seeds"]) and
            set(p["seeds"]).isdisjoint(range(7101,7121)),
            "opened synthetic holdout or prior Optuna seed reused")
    return p,by


def smac_loss(effect):
    require(effect is None or math.isfinite(effect),"nonfinite native worker reward")
    return 1.0 if effect is None else 1.0/(1.0+abs(effect))


def usage():
    a,b=resource.getrusage(resource.RUSAGE_SELF),resource.getrusage(resource.RUSAGE_CHILDREN)
    return (a.ru_utime+a.ru_stime,b.ru_utime+b.ru_stime)


def seconds_ns(t):return round(t*1e9)


def execute(binary,root,scenario,seed):
    p,by=manifest()
    require(seed in p["seeds"] and scenario in p["scenarios"] and
            binary.is_file() and not root.exists(),"unfrozen or overwritten SMAC native trial")
    import smac
    from ConfigSpace import ConfigurationSpace
    from smac import HyperparameterOptimizationFacade as HPOFacade,Scenario
    from smac.runhistory.dataclasses import TrialValue
    require(dist_version("smac")==p["smac_version"],"SMAC3 version not pinned")
    sc=scenario;source=by[sc["source"]]
    root.mkdir(parents=True)
    trace={
       "schema":1,"suite":"development","scenario":sc["id"],"seed":seed,
       "source_commit":subprocess.check_output(["git","rev-parse","HEAD"],cwd=ROOT,text=True).strip(),
       "binary_sha256":sha256_file(binary),"protocol_sha256":sha256_file(PLAN),
       "smac_version":dist_version("smac"),"worker":"genuine_PhaseShift_C++",
       "policies":{},"sealed_evaluation_reused":False
    }
    for policy in POLICIES:
        home=root/policy;home.mkdir()
        start=time.monotonic_ns();start_cpu=usage()
        observations=[]; proposals=[]
        if policy=="acquisition-v3-coverage":
            cfg=config(sc,source,seed,True)
            observations=execute_run(binary,home/"discovery",cfg,policy,p["pair_budget"])
        else:
            edges=allowed_edges(source)
            cs=ConfigurationSpace({"edge_index":(0,len(edges)-1)})
            scenario_object=Scenario(cs,deterministic=False,
                 n_trials=p["pair_budget"],seed=seed,
                 output_directory=home/"smac_state")
            optimizer=HPOFacade(scenario_object,None,overwrite=True)
            for n in range(p["pair_budget"]):
                info=optimizer.ask()
                idx=int(info.config["edge_index"])
                require(0<=idx<len(edges),"SMAC3 proposed illegal structural edge")
                left,right=edges[idx]
                cfg=config(sc,source,seed+100_000+1000*n,False)
                result=execute_run(binary,home/"discovery"/("query_"+str(n).zfill(2)),
                                   cfg,"optuna_random",1,(left,right))
                observed={**result[0],"pair_number":n}
                observations.append(observed)
                loss=smac_loss(observed["effect"])
                optimizer.tell(info,TrialValue(cost=loss))
                proposals.append({"pair_number":n,"edge_index":idx,"left":left,
                                  "right":right,"reported_loss":loss,
                                  "smac_trial_seed":info.seed})
            del optimizer
        end_cpu=usage()
        costs={"wall_ns":time.monotonic_ns()-start,
               "python_parent_cpu_ns":seconds_ns(end_cpu[0]-start_cpu[0]),
               "waited_native_child_cpu_ns":seconds_ns(end_cpu[1]-start_cpu[1])}
        nominated=select(observations,p)
        write(home/"discovery.json",{"observations":observations,
             "proposals":proposals,"chosen":nominated,"cost":costs})
        checked=confirm(binary,home,sc,source,seed,policy,nominated,p,observations)
        write(home/"confirmation.json",{"results":checked})
        trace["policies"][policy]={
          "query_count":len(observations),
          "candidate_count":len(nominated),
          "nominal_sign_rejections":sum(x["status"]==
                    "REJECTED_UNDER_UNVERIFIED_SIGN_INDEPENDENCE" for x in checked),
          "cost":costs
        }
    write(root/"plan.json",trace)
    score=verify(root)
    write(root/"scorecard.json",score)
    print(json.dumps({"status":"PASS","scenario":sc["id"],"seed":seed,
                      "nominal_sign_rejections":{k:v["nominal_sign_rejections"]
                        for k,v in trace["policies"].items()},
                      "not_certified":True},sort_keys=True),flush=True)


def verify(root):
    p,by=manifest()
    plan=read(root/"plan.json")
    require(plan["schema"]==1 and plan["suite"]=="development" and
            plan["sealed_evaluation_reused"] is False and
            plan["protocol_sha256"]==sha256_file(PLAN) and
            plan["smac_version"]==p["smac_version"] and
            len(plan["binary_sha256"])==64 and len(plan["source_commit"])==40 and
            plan["seed"] in p["seeds"] and set(plan["policies"])==set(POLICIES),
            "tampered SMAC3 provenance, source or policy inventory")
    sc=next((s for s in p["scenarios"] if s["id"]==plan["scenario"]),None)
    require(sc is not None,"unknown native worker scenario")
    source=by[sc["source"]]
    out={}
    for policy in POLICIES:
        home=root/policy
        frozen=read(home/"discovery.json")
        observations=[]
        if policy=="acquisition-v3-coverage":
            observations=audit_native_run(home/"discovery"/"run",
                   config(sc,source,plan["seed"],True),policy)
            require(frozen["proposals"]==[],"native policy supplied fake SMAC proposals")
        else:
            require(len(frozen["proposals"])==p["pair_budget"],
                    "missing actual SMAC3 ask/tell records")
            edges=allowed_edges(source)
            for n,log in enumerate(frozen["proposals"]):
                require(log["pair_number"]==n and
                        0<=log["edge_index"]<len(edges) and
                        (log["left"],log["right"])==edges[log["edge_index"]] and
                        (log["left"],log["right"])==
                           (frozen["observations"][n]["left"],frozen["observations"][n]["right"]),
                        "SMAC3 proposed query not legal or mismatched")
                cfg=config(sc,source,plan["seed"]+100_000+1000*n,False)
                rows=audit_native_run(home/"discovery"/("query_"+str(n).zfill(2))/"run",
                                      cfg,"optuna_random",(log["left"],log["right"]))
                require(len(rows)==1,"native SMAC3 edge query must contain exactly one pair")
                observations.append({**rows[0],"pair_number":n})
                require(math.isclose(log["reported_loss"],
                           smac_loss(observations[-1]["effect"]),abs_tol=1e-14),
                        "SMAC3 reported a reward different from native worker observations")
        require(len(observations)==p["pair_budget"] and
                observations==frozen["observations"] and
                frozen["chosen"]==select(observations,p) and
                frozen["cost"]==plan["policies"][policy]["cost"] and
                plan["policies"][policy]["query_count"]==len(observations) and
                plan["policies"][policy]["candidate_count"]==len(frozen["chosen"]),
                "independent discovery run replay and selection mismatch")
        all_seeds={x["seed"] for x in observations};candidates=frozen["chosen"]
        independently_checked=[]
        for n,item in enumerate(candidates):
            path=home/("confirmation_"+str(n))
            seed2=1_000_000_000+plan["seed"]*10+n
            raw=audit_native_run(path/"run",config(sc,source,seed2,False),
                                 "optuna_random",(item["left"],item["right"]))
            require(len(raw)==p["confirmation_pairs"] and
                    not all_seeds.intersection({v["seed"] for v in raw}),
                    "invalid 20-pair native confirmation or reused discovery seed")
            all_seeds.update(v["seed"] for v in raw)
            passed=sum(v["effect"] is not None and
                       item["direction"]*v["effect"]>math.log(p["min_practical_ratio"])
                       for v in raw)
            valid=sum(v["effect"] is not None for v in raw)
            value=exact_sign_tail(20,passed) if valid==20 else 1.
            independently_checked.append({**item,"confirmation_source_seed":seed2,
                     "valid":valid,"positive":passed,"raw_p":value,
                     "observations":raw,"fresh_seed_overlap":False})
        adjusted=adjusted_pvalues([x["raw_p"] for x in independently_checked])
        for n,x in enumerate(independently_checked):
            x["holm_p"]=adjusted[n]
            x["status"]=("ABSTAIN_CENSORED" if x["valid"]<20 else
                         "REJECTED_UNDER_UNVERIFIED_SIGN_INDEPENDENCE" if
                         x["holm_p"]<=p["alpha"] else "NOT_REJECTED")
        require(read(home/"confirmation.json")=={"results":independently_checked},
                "independent raw-worker confirmation, multiplicity or source changed")
        rejections=sum(x["status"]=="REJECTED_UNDER_UNVERIFIED_SIGN_INDEPENDENCE"
                       for x in independently_checked)
        require(plan["policies"][policy]["nominal_sign_rejections"]==rejections,
                "false SMAC3/V18 candidate confirmation summary")
        out[policy]={"queries":len(observations),
                     "unique_edges":len({(x["left"],x["right"]) for x in observations}),
                     "candidates":len(candidates),
                     "nominal_sign_rejections":rejections,
                     "censored_candidates":sum(x["valid"]<20 for x in independently_checked),
                     "discovery_wall_ns":frozen["cost"]["wall_ns"],
                     "discovery_cpu_ns":frozen["cost"]["python_parent_cpu_ns"]+
                                        frozen["cost"]["waited_native_child_cpu_ns"]}
    return {"schema":1,"suite":"development","protocol":p["protocol"],
            "source_commit":plan["source_commit"],"binary_sha256":plan["binary_sha256"],
            "scenario":sc["id"],"seed":plan["seed"],"policies":out,
            "comparability":"same_native_worker_and_query_count_not_equal_end_to_end_costs",
            "true_real_boundary_oracle":False,"sign_independence_certified":False,
            "sota_claim":False,"sealed_evaluation_reused":False}


def main():
    cli=argparse.ArgumentParser()
    cli.add_argument("--binary",type=Path)
    cli.add_argument("--out",required=True,type=Path)
    cli.add_argument("--scenario",choices=("sort_ascending","topk_uniform"))
    cli.add_argument("--seed",type=int)
    cli.add_argument("--verify-only",action="store_true")
    a=cli.parse_args()
    root=a.out.resolve()
    if a.verify_only:
        score=verify(root)
        require(read(root/"scorecard.json")==score,
                "independent SMAC3 native scorecard changed")
        print(json.dumps({"status":"PASS","scenario":score["scenario"],
              "seed":score["seed"],"native_worker_verified":True},sort_keys=True))
    else:
        p,_=manifest()
        require(a.binary is not None and a.scenario is not None and
                a.seed in p["seeds"],"missing executable or unplanned trial")
        sc=next(s for s in p["scenarios"] if s["id"]==a.scenario)
        execute(a.binary.resolve(),root,sc,a.seed)

if __name__=="__main__":
    main()
