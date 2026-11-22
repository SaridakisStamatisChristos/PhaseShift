#!/usr/bin/env python3
"""V3-01d complete six-shard family replay with post-hoc truth labels.

The selection and family freeze NEVER see analytic truth; only this terminal
development report joins known work-unit jump labels for error diagnostics.
These labels are NOT calibrated null distributions for elapsed time.
"""
import argparse
import json
from pathlib import Path

from confirmation_protocol import read,require,db_ro,decode
from multidim_truth import key,load,truth
from protocol import sha256_file
from run_confirmation_development import (
    SOURCE_COMMIT,SOURCE_RUN_ID,verify as verify_family)

def structural_truth_labels(source,fixture,trial,choices):
    """Post-selection audit only. It must not feed the family freeze."""
    fid,policy,seed=trial["fixture"],trial["policy"],trial["seed"]
    path=source/(fid+"_"+policy+"_"+str(seed))/"run"/"run.sqlite"
    with db_ro(path) as db:
        source_edges=db.execute("""SELECT p.pair_number,d.left_coords,d.right_coords
             FROM v3_pairs p JOIN v3_conditional_decisions d USING(pair_number)
             ORDER BY p.pair_number""").fetchall()
    gold={key(e["context"],e["left"],e["right"]) for e in truth(fixture)}
    labels=[]
    for member in choices:
        edge=source_edges[member["discovery_pair"]]
        left,right=decode(edge[1]),decode(edge[2])
        ctx=[(k,v) for k,v in left.items() if k!="x"]
        structural=key(ctx,int(left["x"]),int(right["x"])) in gold
        labels.append({"member_index":len(labels),
                       "work_unit_jump":structural,
                       "nominated_direction":member["direction"],
                       "positive_jump_with_matching_direction":
                           structural and member["direction"]==1})
    return labels

def compose(manifest,per_shard,source_roots,confirmation_roots):
    ids={f["id"] for f in manifest["fixtures"]}
    require(set(per_shard)==ids and set(source_roots)==ids and
            set(confirmation_roots)==ids,
            "missing/duplicate confirmation or source shard")
    all_rows=[]
    per_fixture=[]
    for f in manifest["fixtures"]:
        fid=f["id"]
        data=per_shard[fid]
        require(data["fixture"]==fid and data["suite"]=="development" and
                data["source_commit"]==SOURCE_COMMIT and
                data["source_run_id"]==SOURCE_RUN_ID and data["trials"]==40 and
                data["familywise_error_certified"] is False and
                data["sota_claim"] is False and
                data["model_assumptions_validated"] is False,
                "incompatible unsealed family report")
        require(len(data["raw_results"])==40,"partial trial inventory")
        bypolicy={}
        for policy in manifest["policies"]:
            rs=[r for r in data["raw_results"] if r["policy"]==policy]
            require(len(rs)==20 and
                    {r["seed"] for r in rs}==set(manifest["seeds"]),
                    "not a full 20-seed matched policy cohort")
            statuses={s:sum(r["status"]==s for r in rs) for s in
                      ("NO_NOMINATION","FAMILY_COMPLETE_UNDER_UNVERIFIED_MODEL","FAILED")}
            require(sum(statuses.values())==20,"unknown family status")
            bypolicy[policy]={
                "trials":20,
                "no_nomination":statuses["NO_NOMINATION"],
                "completed_families":statuses["FAMILY_COMPLETE_UNDER_UNVERIFIED_MODEL"],
                "failed":statuses["FAILED"],
                "selected_members":sum(r["selected_members"] for r in rs),
                "holdout_pairs":sum(r.get("audit",{}).get("observed_holdout_pairs",0) for r in rs),
                "nominal_sign_model_rejections":sum(
                    r.get("audit",{}).get("nominal_rejections",0) for r in rs),
                "trials_with_nominal_rejection":sum(
                    r.get("audit",{}).get("nominal_rejections",0)>0 for r in rs)
            }
        diagnostic={"selected_positive_planted_jump_edges":0,
                    "selected_edges_outside_planted_jump_truth":0,
                    "nominal_model_rejections_at_positive_planted_jumps":0,
                    "nominal_model_rejections_outside_planted_jump_truth":0}
        for row in data["raw_results"]:
            dest=confirmation_roots[fid]/(fid+"_"+row["policy"]+"_"+str(row["seed"]))
            choices=read(dest/"selection.json")["selected"]
            labels=structural_truth_labels(source_roots[fid],f,row,choices)
            require(len(labels)==row["selected_members"],
                    "post-hoc selected-edge count drift")
            members=row.get("audit",{}).get("members",[])
            if row["status"]=="FAMILY_COMPLETE_UNDER_UNVERIFIED_MODEL":
                require(len(members)==len(labels),"missing holdout member verdicts")
            for label in labels:
                match=label["positive_jump_with_matching_direction"]
                if match:
                    diagnostic["selected_positive_planted_jump_edges"]+=1
                else:
                    diagnostic["selected_edges_outside_planted_jump_truth"]+=1
                if members and members[label["member_index"]]["state"]=="REJECTED_UNDER_DECLARED_MODEL":
                    name=("nominal_model_rejections_at_positive_planted_jumps" if match else
                          "nominal_model_rejections_outside_planted_jump_truth")
                    diagnostic[name]+=1
            all_rows.append(row)
        per_fixture.append({"fixture":fid,"planted_work_unit_edges":len(truth(f)),
                            "policies":bypolicy,
                            "posthoc_work_unit_labels_not_time_null":diagnostic})
    require(len(all_rows)==240 and
            len({(r["fixture"],r["policy"],r["seed"]) for r in all_rows})==240,
            "incomplete/duplicated full confirmation inventory")
    return {"schema":1,"protocol":"v3-01d.frozen-development-holdout.1",
            "source_commit":SOURCE_COMMIT,"source_run_id":SOURCE_RUN_ID,
            "suite":"development","trials":240,
            "planned_source_pairs_per_trial":12,"fresh_holdout_pairs_per_member":20,
            "outcomes":per_fixture,"failed":sum(r["status"]=="FAILED" for r in all_rows),
            "completed_families":sum(r["status"]=="FAMILY_COMPLETE_UNDER_UNVERIFIED_MODEL" for r in all_rows),
            "no_nomination":sum(r["status"]=="NO_NOMINATION" for r in all_rows),
            "observed_fresh_holdout_pairs":sum(r.get("audit",{}).get("observed_holdout_pairs",0) for r in all_rows),
            "nominal_rejections_under_declared_unverified_sign_model":sum(
                r.get("audit",{}).get("nominal_rejections",0) for r in all_rows),
            "model_assumptions_validated":False,"population_fwer_certified":False,
            "eligible_for_sota":False,"sota_claim":False,
            "limitations":["Selection is truth-blind; work-unit jump labels are only descriptive",
              "Holdout samples are fresh per frozen family, but sign independence is unverified",
              "Holm is per selected family, not an across-campaign population FWER guarantee",
              "Smooth gradients can have legitimate timed differences without work-unit jumps",
              "Family inference is distinct from confirmed algorithmic speedup",
              "No sealed evaluation seeds or external competitors were used"]}

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--sources",required=True,type=Path)
    p.add_argument("--confirmations",required=True,type=Path)
    p.add_argument("--out",required=True,type=Path)
    p.add_argument("--verify-only",action="store_true")
    a=p.parse_args()
    manifest,_=load("development")
    sources={};confirmations={}
    for path in a.sources.iterdir():
        if (path/"frozen_plan.json").is_file():
            fid=read(path/"frozen_plan.json")["shard"]
            require(fid not in sources,"duplicate source evidence shard")
            sources[fid]=path
    for path in a.confirmations.iterdir():
        if (path/"plan.json").is_file():
            src=path/"plan.json"
            # The folder's provenance is checked by replaying the complete trial.
            fid=path.name.removeprefix("v3d-confirmation-")
            require(fid not in confirmations,"duplicate confirmation evidence shard")
            confirmations[fid]=path
    require(set(sources)==set(confirmations)=={f["id"] for f in manifest["fixtures"]},
            "missing full freeze/source evidence")
    verified={}
    provenance={}
    for f in manifest["fixtures"]:
        fid=f["id"]
        source=sources[fid]
        root=confirmations[fid]
        score=verify_family(root,source)
        require(read(root/"scorecard.json")==score,"confirmation scorecard changed")
        verified[fid]=score
        provenance[fid]={"source_plan_sha256":sha256_file(source/"frozen_plan.json"),
                         "confirmation_plan_sha256":sha256_file(root/"plan.json"),
                         "scorecard_sha256":sha256_file(root/"scorecard.json")}
    final=compose(manifest,verified,sources,confirmations)
    final["shard_provenance"]=provenance
    if a.verify_only:
        require(read(a.out)==final,"aggregate replay mismatch")
    else:
        with a.out.open("x") as fd:
            json.dump(final,fd,indent=2,sort_keys=True,allow_nan=False);fd.write("\n")
    require(final["failed"]==0,"failed fresh-seed campaigns retained, not accepted")
    print(json.dumps({"status":"PASS","trials":final["trials"],
                      "completed_families":final["completed_families"],
                      "fresh_holdout_pairs":final["observed_fresh_holdout_pairs"],
                      "nominal_model_rejections":final["nominal_rejections_under_declared_unverified_sign_model"],
                      "sota_claim":False},sort_keys=True),flush=True)
if __name__=="__main__":
    main()
