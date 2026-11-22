#!/usr/bin/env python3
"""V3-01f: two forced null edges per family, matched-seed Holm stress.

The two nominated structural edges are fixed by the 3-point axis, not by
the sign, size or significance of their discovery contrasts. The study
has 2 * 64 complete two-member families; noncompletion is a failure.
The real C++ schema-16 sidecar computes Holm; an independent Python
auditor reconstructs raw timings, work, source identity and p-values.
"""
from __future__ import annotations
import argparse
import json
import math
from pathlib import Path
import platform
import sqlite3
import subprocess

from confirmation_protocol import db_ro, decode, independent_sidecar, read, require
from protocol import sha256_file
from run_forced_null_calibration import (
    invoke, expected_work, upper_bound_if_independent, write)
from run_null_calibration import load_stress
from temporal_null_audit import temporal_metrics

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
MANIFEST=HERE/"multimember_null_manifest.json"
ALLOWED={"flat_two_edge","seed_shared_two_edge_contended"}


def manifest():
    m=read(MANIFEST)
    require(m["schema"]==1 and m["protocol"]=="phaseshift.v3-01f.multimember-null.1" and
            m["suite"]=="development_multimember_forced_null" and
            m["eligible_for_claim"] is False and m["sota_claim"] is False and
            m["independent_sign_assumptions_certified"] is False and
            m["wall_time_null_certified"] is False and
            m["population_fwer_certified"] is False and
            m["read_only_predecessor_run"]==45038398487 and
            m["families_per_stratum"]==64 and
            m["members_per_family"]==2 and
            m["discovery_pairs_per_family"]==2 and
            m["fresh_pairs_per_member"]==20 and
            m["alpha"]==.05 and m["min_practical_ratio"]==1.2 and
            m["selection"]=="exogenous_all_two_structural_edges;no_effect_threshold_or_sign_used" and
            m["direction_rule"]=="member0_even_positive_odd_negative;member1_opposite" and
            m["stop_rule"]=="fixed_complete_40_pairs_no_optional_early_stopping",
            "non-predeclared multi-member protocol")
    strata=m["strata"]
    require(len(strata)==2 and {s["id"] for s in strata}==ALLOWED,
            "wrong multi-family strata")
    seen=set()
    for s in strata:
        require(s["algorithm"] in ("calibration","calibration_seed_shared_null_v3") and
                s["contention"]==(s["id"]=="seed_shared_two_edge_contended") and
                (s["algorithm"]=="calibration")== (s["id"]=="flat_two_edge") and
                s["x_values"] in ([1,2,3],[3,4,5]) and
                len(s["x_values"])==3 and
                s["threshold"] in (8,10),
                "altered frozen multiedge geometry")
        seeds=s["seeds"]
        require(len(seeds)==64 and len(set(seeds))==64 and
                all(type(n) is int and 100000<=n<=102000 for n in seeds) and
                not (set(seeds)&seen),"missing/repeated multi-family seed")
        seen.update(seeds)
    return m,sha256_file(MANIFEST)


def config(m,s,seed):
    return {
       "schema_version":1,"experiment_id":"v3f-"+s["id"],
       "problem_family":"calibration","algorithms":[s["algorithm"]],
       "seed":seed,"primary_axis":"x",
       "fixed":{"negative":True},
       "axes":{"x":{"values":s["x_values"]},
               "threshold":{"values":[s["threshold"]]}},
       "search":{"policy_version":"v2","method":"uniform_v2",
                 "max_cases":2*m["discovery_pairs_per_family"]},
       "measurement":{"paired_discovery_blocks":1,
                      "paired_confirm_blocks":m["fresh_pairs_per_member"],
                      "warmup_iterations":0,"minimum_batch_ms":1},
       "detection":{"min_practical_ratio":m["min_practical_ratio"]},
       "limits":{"total_wall_seconds":90,"max_search_queue":64,
                 "max_case_bytes":1048576,"max_worker_rss_mb":512,
                 "max_sample_wall_ms":1500}
    }


def directions(i):
    return (1,-1) if i%2==0 else (-1,1)


def family_spec(m,s,seed,i):
    signs=directions(i)
    return {
       "family_id":"v3f-"+s["id"]+"-"+str(seed),
       "seed_pairs":m["fresh_pairs_per_member"],
       "alpha":m["alpha"],"sign_model":"independent_seed_signs_declared",
       "members":[{"discovery_pair":0,"direction":signs[0]},
                  {"discovery_pair":1,"direction":signs[1]}]
    }


def independent_work_and_edges(path,s,expected_pairs,require_all_edges):
    """Raw sample oracle. No reliance on the runner's reported work or truth."""
    with db_ro(path) as db:
        db.execute("PRAGMA query_only=ON")
        rows=db.execute("""SELECT p.pair_number,p.seed,p.status,p.log_contrast,
                                  lc.params,rc.params
            FROM v3_pairs p JOIN v3_cases lc ON lc.case_id=p.left_case_id
            JOIN v3_cases rc ON rc.case_id=p.right_case_id
            ORDER BY p.pair_number""").fetchall()
        require(len(rows)==expected_pairs,"incomplete two-edge null input")
        seen=set();effects=[]
        allowed_edges={(a,b) for a,b in zip(s["x_values"],s["x_values"][1:])}
        for i,(number,seed,status,effect,left_params,right_params) in enumerate(rows):
            require(number==i and status in ("ok","censored") and
                    ((effect is None)==(status=="censored")),
                    "noncontiguous / false-promotion evidence")
            left,right=json.loads(left_params),json.loads(right_params)
            require(left["negative"] is True and right["negative"] is True and
                    left["threshold"]==right["threshold"]==s["threshold"] and
                    (left["x"],right["x"]) in allowed_edges,
                    "unexpected null geometry")
            seen.add((left["x"],right["x"]))
            raw=db.execute("""SELECT s.side,s.algorithm,s.status,s.work,s.ns,
                                    s.batch,c.seed,c.params
                    FROM v3_samples s JOIN v3_cases c USING(case_id)
                    WHERE s.pair_number=? ORDER BY s.side,s.ordinal""",(i,)).fetchall()
            require(len(raw)==2 and [x[0] for x in raw]==[0,1],
                    "raw sample side count")
            levels=[]
            for j,(side,algorithm,st,work,ns,batch,case_seed,params) in enumerate(raw):
                want=left if side==0 else right
                case=json.loads(params)
                require(case_seed==seed and algorithm==s["algorithm"] and
                        case["x"]==want["x"] and
                        case["negative"] is True and
                        case["threshold"]==s["threshold"],
                        "wrong case seed, sample algorithm or side structure")
                if st=="ok":
                    require(work==expected_work(s["algorithm"],seed) and
                            ns is not None and ns>0 and
                            batch is not None and batch>0,
                            "not equal independent seeded CPU work")
                    levels.append(math.log(ns/batch))
                else:
                    require(ns is None and work is None and batch is None,
                            "invalid failed sample promoted")
                    levels.append(None)
            ok=all(x is not None for x in levels)
            require(ok==(status=="ok"),"pair/sample status mismatch")
            observed=(levels[1]-levels[0]) if ok else None
            require(observed is None and effect is None or
                    observed is not None and
                    math.isclose(observed,effect,rel_tol=1e-12,abs_tol=1e-12),
                    "stored contrast differs from independent raw oracle")
            effects.append(observed)
        if require_all_edges:
            require(seen==allowed_edges and (not require_all_edges or
                    len(rows)>=len(allowed_edges)),
                    "both predeclared edges absent or modified")
        return effects


def verify_trial(folder,m,scene,seed,index,revision):
    require(read(folder/"config.json")==config(m,scene,seed),"frozen input modified")
    family=family_spec(m,scene,seed,index)
    require(read(folder/"family.json")==family,"family nomination tuned post hoc")
    source=folder/"run"/"run.sqlite"
    with db_ro(source) as db:
        require(db.execute("PRAGMA user_version").fetchone()[0]==14,
                "wrong conditional-tree discovery schema")
        source_cfg,n=db.execute("SELECT source_json,max_pairs FROM v3_plan WHERE id=1").fetchone()
        require(json.loads(source_cfg)==config(m,scene,seed) and n==2,
                "source plan silently changed")
    independent_work_and_edges(source,scene,2,True)
    report=read(folder/"family_report.json")
    dest=folder/"run"/"confirmation"/"family.sqlite"
    independent=independent_sidecar(source,dest,family,report)
    require(independent["source_revision"]==revision and
            independent["planned_holdout_pairs"]==40 and
            independent["observed_holdout_pairs"]==40 and
            len(independent["members"])==2,
            "mismatched confirmation worker revision/size")
    effects=independent_work_and_edges(dest,scene,40,True)
    # Family sidecar interleaves member 0 and member 1 by pair_number.
    temporal=[temporal_metrics(effects[j::2]) for j in range(2)]
    commands=read(folder/"commands.json")
    cmd=[v["argv"][1] for v in commands]
    require(cmd[0]=="conditional-tree" and
            cmd[1]=="verify" and
            cmd[2]=="family-freeze" and
            "family-confirm" in cmd and
            cmd[-1]=="family-report" and
            all(v["returncode"]==0 for v in commands),
            "missing or failed real C++ family execution")
    return {
        "seed":seed,"stratum":scene["id"],"complete":True,
        "frozen_exogenous_members":2,"holdout_pairs":40,
        "predeclared_directions":list(directions(index)),
        "independent_holm_nominal_rejections":independent["nominal_rejections"],
        "familywise_any_nominal_rejection":independent["nominal_rejections"]>0,
        "independently_verified_member_results":independent["members"],
        "temporal_sign_diagnostics":temporal,
        "discovery_snapshot_sha256":independent["source_snapshot_sha256"],
        "confirmation_snapshot_sha256":independent["holdout_snapshot_sha256"],
        "commands_sha256":sha256_file(folder/"commands.json"),
        "source_commit":revision,
        "statistical_null_certified":False
    }


def reconstruct(root):
    m,mh=manifest();plan=read(root/"frozen_plan.json")
    require(plan["schema"]==1 and plan["manifest_sha256"]==mh and
            plan["manifest"]==m and
            plan["suite"]=="development_multimember_forced_null" and
            plan["diagnostic"] in (False,True) and
            plan["sota_claim"] is False and
            plan["wall_time_null_certified"] is False and
            plan["familywise_error_certified"] is False and
            len(plan["source_commit"])==40,
            "unknown or mutable 2-edge experiment")
    scene=next((s for s in m["strata"] if s["id"]==plan["stratum"]),None)
    require(scene is not None,"not a predeclared 2-edge stratum")
    seeds=scene["seeds"][:1] if plan["diagnostic"] else scene["seeds"]
    require(plan["seed_inventory"]==seeds and len(seeds) in (1,64),
            "modified 2-edge seed cohort")
    rows=[]
    for i,seed in enumerate(seeds):
        row=verify_trial(root/str(seed),m,scene,seed,i,plan["source_commit"])
        require(read(root/str(seed)/"terminal.json")==row,
                "tampered trial scorecard")
        rows.append(row)
    rejections=sum(v["familywise_any_nominal_rejection"] for v in rows)
    return {
        "schema":1,"protocol":m["protocol"],"suite":m["suite"],
        "stratum":scene["id"],"source_commit":plan["source_commit"],
        "manifest_sha256":mh,"diagnostic":plan["diagnostic"],
        "complete_families":len(seeds),"planned_families":len(seeds),
        "nominal_familywise_rejections":rejections,
        "conditional_one_sided_95_upper_if_independent":
            upper_bound_if_independent(rejections,len(seeds)) if len(seeds)==64 else None,
        "nominal_member_rejections":sum(v["independent_holm_nominal_rejections"] for v in rows),
        "planned_holdout_pairs":40*len(seeds),"observed_holdout_pairs":40*len(seeds),
        "raw_results":rows,
        "null_is_equal_deterministic_work_only":True,
        "population_fwer_certified":False,"independence_empirically_validated":False,
        "sota_claim":False
    }


def execute(binary,root,target,diagnostic):
    m,mh=manifest()
    scene=next((s for s in m["strata"] if s["id"]==target),None)
    require(scene is not None and binary.is_file() and not root.exists(),
            "unknown 2-edge null or destination exists")
    revision=subprocess.check_output(["git","rev-parse","HEAD"],cwd=ROOT,text=True).strip()
    require(len(revision)==40,"source provenance unavailable")
    root.mkdir(parents=True)
    seeds=scene["seeds"][:1] if diagnostic else scene["seeds"]
    write(root/"frozen_plan.json",{
        "schema":1,"suite":m["suite"],"stratum":scene["id"],
        "manifest":m,"manifest_sha256":mh,
        "binary_sha256":sha256_file(binary),"source_commit":revision,
        "seed_inventory":seeds,"diagnostic":diagnostic,"host":platform.platform(),
        "sota_claim":False,"wall_time_null_certified":False,
        "familywise_error_certified":False})
    failures=[]
    for index,seed in enumerate(seeds):
        folder=root/str(seed);folder.mkdir()
        write(folder/"config.json",config(m,scene,seed))
        write(folder/"family.json",family_spec(m,scene,seed,index))
        commands=[]
        try:
            with load_stress(scene["contention"]):
                def call(*args):
                    record=invoke(binary,*args)
                    commands.append(record)
                    (folder/"commands.json").write_text(
                        json.dumps(commands,sort_keys=True,indent=2,allow_nan=False)+"\n")
                    return json.loads(record["stdout"])
                call("conditional-tree","--config",folder/"config.json",
                     "--out",folder/"run","--pairs","2")
                call("verify","--run",folder/"run")
                independent_work_and_edges(folder/"run"/"run.sqlite",scene,2,True)
                initial=call("family-freeze","--run",folder/"run",
                             "--manifest",folder/"family.json")
                require(initial["complete"] is False,"premature family conclusion")
                previous=-1
                for _ in range(8):
                    result=call("family-confirm","--run",folder/"run")
                    if result["complete"]:
                        break
                    require(result["committed_pairs"]>previous,
                            "unbounded family resumption without progress")
                    previous=result["committed_pairs"]
                final=call("family-report","--run",folder/"run")
                require(final["complete"],"40 planned fresh pairs not measured")
                write(folder/"family_report.json",final)
            row=verify_trial(folder,m,scene,seed,index,revision)
            write(folder/"terminal.json",row)
            print(json.dumps({"seed":seed,"stratum":target,
                              "familywise_nominal_rejection":
                                   row["familywise_any_nominal_rejection"]}),
                  flush=True)
        except (ValueError,KeyError,TypeError,RuntimeError,OSError,
                sqlite3.Error,subprocess.TimeoutExpired) as error:
            failed={"seed":seed,"status":"FAILED","error":repr(error)}
            failures.append(failed)
            write(folder/"terminal.json",failed)
            print(json.dumps(failed),flush=True)
    if failures:
        write(root/"failures.json",failures)
        raise ValueError(f"{len(failures)} forced multiedge families failed; all evidence retained")
    report=reconstruct(root)
    write(root/"scorecard.json",report)
    print(json.dumps({"status":"PASS","complete_families":report["complete_families"],
        "holdout_pairs":report["observed_holdout_pairs"],
        "familywise_nominal_rejections":report["nominal_familywise_rejections"],
        "independence_certified":False},sort_keys=True),flush=True)


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--out",required=True,type=Path)
    parser.add_argument("--binary",type=Path)
    parser.add_argument("--stratum")
    parser.add_argument("--diagnostic",action="store_true")
    parser.add_argument("--verify-only",action="store_true")
    args=parser.parse_args()
    root=args.out.resolve()
    if args.verify_only:
        report=reconstruct(root)
        require(read(root/"scorecard.json")==report,"scorecard not reproduced")
        print("PASS independent source/cases/40-pair Holm/temporal replay")
    else:
        require(args.binary and args.stratum,
                "execution needs real worker and declared stratum")
        execute(args.binary.resolve(),root,args.stratum,args.diagnostic)


if __name__=="__main__":
    main()
