#!/usr/bin/env python3
"""V3 discovery policy diagnosis on the archived COMPLETE 240-run development campaign.

This is NOT a new benchmarking protocol or SOTA comparison. It replays the
original immutable SQLite + logs, then classifies *why* a policy missed known
synthetic work-unit edges: non-exposure versus non-nomination after exposure.
Neither classifier nor these post-hoc findings may touch sealed evaluation.
"""
from __future__ import annotations
import argparse
import json
import math
from pathlib import Path
import sqlite3

from aggregate_full_development import audit_inventory
from multidim_truth import key,load,order,truth,config
from protocol import sha256_file
from run_multidim import verify,read
from run_paired_development import decode

BASELINE_COMMIT="b8a713f2430578f3ae7ec5e00cadb348348bbcd6"
BASELINE_RUN=45026057899


def require(ok,reason):
    if not ok:
        raise ValueError(reason)


def trajectory(path,fixture,policy,seed):
    """Independent raw policy diagnostic; only labels after trajectory frozen."""
    cfg=config(fixture,seed)
    axes=tuple(cfg["axes"])
    true={key(e["context"],e["left"],e["right"]) for e in truth(fixture)}
    discovered=set()
    visited=set()
    context_set=set()
    first_exposure=None
    first_nomination=None
    first_qualifying=None
    exposed_true=set()
    signed_valid=0
    qualified_true=set()
    false_nominated=0
    repeat_true=0
    raw_total=0
    rows=[]
    with sqlite3.connect(path.resolve().as_uri()+"?mode=ro",uri=True) as db:
        db.execute("PRAGMA query_only=ON")
        tuples=db.execute("""SELECT p.pair_number,p.status,p.log_contrast,
                      d.arm,d.axis,d.left_coords,d.right_coords
                      FROM v3_pairs p JOIN v3_conditional_decisions d
                      USING(pair_number) ORDER BY p.pair_number""").fetchall()
        require(len(tuples)==12,"partial old development trial")
        for i,(number,status,effect,arm,axis,left,right) in enumerate(tuples):
            require(number==i and axis=="x" and
                    arm in ("explore","replicate") and
                    status in ("ok","censored") and
                    (effect is None)==(status=="censored"),
                    "invalid raw trajectory inventory")
            a,b=decode(left),decode(right)
            require(set(a)==set(b)==set(axes) and
                    all(a[k]==b[k] for k in a if k!="x"),
                    "invalid cross-context pair")
            c=tuple((k,a[k]) for k in axes if k!="x")
            ident=key(c,int(a["x"]),int(b["x"]))
            context_set.add(c)
            same=ident in visited
            if arm=="explore":
                require(not same,"explore arm repeats observed structural edge")
            else:
                require(same,"replication arm without observed edge")
            visited.add(ident)
            is_truth=ident in true
            if is_truth:
                exposed_true.add(ident)
                if first_exposure is None:first_exposure=i+1
            nominate=(effect is not None and
                      abs(effect)>=math.log(cfg["detection"]["min_practical_ratio"]))
            match=(nominate and is_truth and effect>0)
            if nominate and first_nomination is None:first_nomination=i+1
            if match:
                if ident in qualified_true: repeat_true+=1
                else:qualified_true.add(ident)
                if first_qualifying is None:first_qualifying=i+1
            if nominate and not match:
                false_nominated+=1
            if is_truth and effect is not None and effect>0:signed_valid+=1
            rows.append({
                "pair_number":i,"arm":arm,"valid":effect is not None,
                "structural_truth_exposed":is_truth,
                "signed_true_nomination":match
            })
    require(len(visited)<=12,"more unique edges than budget")
    return {
        "fixture":fixture["id"],"policy":policy,"seed":seed,
        "budgeted_pairs":12,
        "distinct_edges_visited":len(visited),
        "replication_queries":sum(x["arm"]=="replicate" for x in rows),
        "new_edge_queries":sum(x["arm"]=="explore" for x in rows),
        "distinct_contexts":len(context_set),
        "true_edges_in_fixture":len(true),
        "distinct_true_edges_visited":len(exposed_true),
        "distinct_true_edges_nominated":len(qualified_true),
        "true_edge_exposure_events":sum(x["structural_truth_exposed"] for x in rows),
        "first_true_exposure_pair":first_exposure,
        "first_true_nomination_pair":first_qualifying,
        "false_nomination_events":false_nominated,
        "repeat_true_nomination_events":repeat_true,
        "miss_mechanism":("no_true_boundary" if not true else
                         "hit" if qualified_true else
                         "never_visited_true_edge" if not exposed_true else
                         "visited_true_edge_no_qualified_nomination"),
        "trajectory":rows
    }


def analyze(artifacts):
    m,mh=load("development")
    require(set(m["policies"])=={"conditional-tree","acquisition-v3"} and
            m["pairs"]==12 and len(m["seeds"])==20,
            "not the frozen development experiment")
    names={f["id"] for f in m["fixtures"]}
    paths={}
    for folder in artifacts.iterdir():
        if folder.is_dir() and (folder/"frozen_plan.json").is_file():
            plan=read(folder/"frozen_plan.json")
            name=plan["shard"]
            require(name in names and name not in paths,
                    "duplicate/unexpected development artifact shard")
            paths[name]=folder
    require(set(paths)==names,"missing fixture shard")
    entries={}
    rows=[]
    provenance={}
    for f in m["fixtures"]:
        root=paths[f["id"]]
        checked=verify(root)
        require(read(root/"scorecard.json")==checked and
                checked["source_commit"]==BASELINE_COMMIT and
                checked["suite"]=="development" and
                checked["diagnostic"] is False,
                "unverified historical source")
        plan=read(root/"frozen_plan.json")
        require(plan["source_commit"]==BASELINE_COMMIT and
                plan["manifest"]==m and
                plan["manifest_sha256"]==mh and
                plan["run_order"]==order(m,False,f["id"]),
                "source input/seed order drift")
        original=read(root/"results.json")["rows"]
        entries[f["id"]]=(BASELINE_COMMIT,plan,original)
        provenance[f["id"]]={
            "source_plan_sha256":sha256_file(root/"frozen_plan.json"),
            "source_scorecard_sha256":sha256_file(root/"scorecard.json")
        }
        for row in original:
            trial=root/(row["fixture"]+"_"+row["policy"]+"_"+str(row["seed"]))
            diag=trajectory(trial/"run"/"run.sqlite",f,row["policy"],row["seed"])
            require(diag["distinct_true_edges_nominated"]==row["adjudication"]["true_edges_hit"] and
                    diag["false_nomination_events"]==row["adjudication"]["false_observations"] and
                    diag["first_true_nomination_pair"]==row["adjudication"]["first_true_pair"],
                    "diagnostic contradicts independent truth adjudication")
            diag["eligible"]=row["eligible"]
            rows.append(diag)
    all_raw,sha=audit_inventory(m,entries)
    require(sha==BASELINE_COMMIT and len(all_raw)==len(rows)==240,
            "not the complete historic 240-trial campaign")
    by={(r["fixture"],r["seed"],r["policy"]):r for r in rows}
    per_fixture=[]
    for f in m["fixtures"]:
        summary={}
        for p in m["policies"]:
            a=[r for r in rows if r["fixture"]==f["id"] and r["policy"]==p]
            require(len(a)==20 and {r["seed"] for r in a}==set(m["seeds"]),
                    "policy/fixture seed mismatch")
            summary[p]={
                "trials":20,
                "hit":sum(r["miss_mechanism"]=="hit" and r["eligible"] for r in a),
                "miss_due_to_unvisited_boundary":sum(
                    r["miss_mechanism"]=="never_visited_true_edge" for r in a),
                "miss_due_to_non_nominated_exposed_boundary":sum(
                    r["miss_mechanism"]=="visited_true_edge_no_qualified_nomination" for r in a),
                "replication_queries":sum(r["replication_queries"] for r in a),
                "unique_edge_queries":sum(r["new_edge_queries"] for r in a),
                "structural_true_edge_exposures":sum(r["distinct_true_edges_visited"] for r in a),
                "distinct_true_edges_nominated":sum(r["distinct_true_edges_nominated"] for r in a),
                "false_nomination_events":sum(r["false_nomination_events"] for r in a),
                "median_contexts":sorted(r["distinct_contexts"] for r in a)[9:11]
            }
        paired={
            "v15_more_replication_seeds":sum(by[(f["id"],s,"acquisition-v3")]["replication_queries"]>
                                             by[(f["id"],s,"conditional-tree")]["replication_queries"]
                                             for s in m["seeds"]),
            "v15_fewer_true_exposure_seeds":sum(by[(f["id"],s,"acquisition-v3")]["distinct_true_edges_visited"]<
                                                by[(f["id"],s,"conditional-tree")]["distinct_true_edges_visited"]
                                                for s in m["seeds"]),
            "v15_loses_detection_seed_count":sum(
                by[(f["id"],s,"conditional-tree")]["distinct_true_edges_nominated"]>0 and
                by[(f["id"],s,"acquisition-v3")]["distinct_true_edges_nominated"]==0
                for s in m["seeds"]),
            "v15_gains_detection_seed_count":sum(
                by[(f["id"],s,"conditional-tree")]["distinct_true_edges_nominated"]==0 and
                by[(f["id"],s,"acquisition-v3")]["distinct_true_edges_nominated"]>0
                for s in m["seeds"])
        }
        per_fixture.append({"fixture":f["id"],"planted_truth_edges":len(truth(f)),
                            "policies":summary,"matched_seed_differences":paired})
    return {
        "schema":1,"protocol":"phaseshift.v3-policy-trajectory-posthoc-development.1",
        "source_commit":BASELINE_COMMIT,"source_run_id":BASELINE_RUN,
        "suite":"development","observed_trials":240,
        "seed_clusters":20,"fixture_count":6,"pair_budget":12,
        "diagnostic_only":True,"eligible_for_claim":False,"sota_claim":False,
        "evaluation_opened":False,"scenario_results":per_fixture,
        "source_artifacts":provenance,
        "raw_per_trial_diagnostics":rows,
        "limits":[
            "Truth is planted synthetic work-unit boundary, not confirmed timing truth",
            "V15 and v14 raw observations occur on potentially different edges; no counterfactual policy truth inference",
            "All attribution is post-hoc on the completed original development cohort",
            "No fresh evaluation data and no external SOTA candidate",
            "A nomination is unconfirmed timing evidence, not a proven performance cliff"
        ]
    }


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--artifacts",required=True,type=Path)
    p.add_argument("--out",required=True,type=Path)
    p.add_argument("--verify-only",action="store_true")
    args=p.parse_args()
    report=analyze(args.artifacts)
    if args.verify_only:
        require(json.loads(args.out.read_text())==report,"posthoc diagnostic scorecard drift")
    else:
        with args.out.open("x") as out:
            json.dump(report,out,sort_keys=True,indent=2,allow_nan=False)
            out.write("\n")
    print(json.dumps({
        "status":"PASS","original_trials":report["observed_trials"],
        "source_commit":BASELINE_COMMIT,"observations_only":True,
        "per_fixture":[{
            "fixture":s["fixture"],
            "v14":s["policies"]["conditional-tree"]["hit"],
            "v15":s["policies"]["acquisition-v3"]["hit"],
            "v15_exploration_fewer_than_v14":
                s["policies"]["acquisition-v3"]["unique_edge_queries"]<
                s["policies"]["conditional-tree"]["unique_edge_queries"],
            "v15_extra_replications":
                s["policies"]["acquisition-v3"]["replication_queries"]-
                s["policies"]["conditional-tree"]["replication_queries"]
        } for s in report["scenario_results"]]
    },sort_keys=True),flush=True)


if __name__=="__main__":
    main()
