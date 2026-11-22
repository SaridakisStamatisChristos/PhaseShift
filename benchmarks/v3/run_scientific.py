#!/usr/bin/env python3
"""Version 2 incumbent scientific benchmark; real CLI only, append-only evidence.
Existing v1/v2 generators, policies, v3 diagnostic manifests remain untouched.
"""
import argparse
import copy
import hashlib
import json
import math
import pathlib
import platform
import random
import re
import resource
import signal
import sqlite3
import subprocess
import time

from protocol import canonical, seal, sha256_file, sqlite_snapshot_sha256

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
POLICIES = ("grid", "random", "stratified", "adaptive_partition", "uniform_v2",
            "stratified_v2", "weighted_v2", "hierarchical_boundary_v2",
            "cliff_bracket_v2", "hybrid_mixed_v2")

def need(ok, message):
    if not ok:
        raise ValueError(message)

def read(path):
    return json.loads(path.read_text())

def write(path, value):
    with path.open("x") as handle:
        json.dump(value, handle, indent=2, sort_keys=True, allow_nan=False)
        handle.write("\n")

def manifest(suite):
    path = HERE / ("scientific_" + suite + ".json")
    m = read(path)
    need(m["schema"] == 2 and m["suite"] == suite and
         m["eligible_for_claim"] is False and m["sota_claim"] is False, "manifest version")
    need(len(m["seeds"]) == len(set(m["seeds"])) == 20 and
         all(type(s) is int and 0 <= s < 2**31 for s in m["seeds"]), "seed inventory")
    need(len(m["scenarios"]) == len({f["id"] for f in m["scenarios"]}) == 9, "scenario inventory")
    for f in m["scenarios"]:
        need(re.fullmatch(r"[a-z_]+", f["id"]) and
             f["truth"] in ("single_cliff", "null", "unlabelled") and
             4 <= f["cases"] <= 64 and len(f["policies"]) == len(set(f["policies"])) and
             set(f["policies"]) <= set(POLICIES), "scenario contract")
        if f["truth"] != "unlabelled":
            truth_edges(f)
    return m, sha256_file(path)

def check_splits():
    a, _ = manifest("development")
    b, _ = manifest("evaluation")
    need(not set(a["seeds"]) & set(b["seeds"]), "seed leakage")
    need(not {seal(f["config"]) for f in a["scenarios"]} &
         {seal(f["config"]) for f in b["scenarios"]}, "identical scenario parameters")

def truth_edges(f):
    """Independent discrete oracle from declared calibration parameters.
    Work-unit regime changes only, not elapsed-time or smooth-shape claims.
    """
    if f["truth"] == "unlabelled":
        return None
    c = f["config"]
    xs = c["axes"]["x"]["values"]
    t = c["fixed"]["threshold"]
    need(xs == sorted(set(xs)) and all(type(x) is int and x >= 0 for x in xs)
         and type(t) is int and t >= 1, "invalid numeric truth domain")
    neg = c["fixed"]["negative"]
    need(type(neg) is bool and neg == (f["truth"] == "null"), "null mismatch")
    regime = lambda x: 0 if neg or x < t else 1
    edges = [(l, r) for l, r in zip(xs, xs[1:]) if regime(l) != regime(r)]
    need(len(edges) == (0 if neg else 1), "planted threshold outside legal domain")
    return edges

def config(f, policy, seed):
    c = copy.deepcopy(f["config"])
    c["seed"] = seed
    c["experiment_id"] = f["id"] + "-" + policy + "-" + str(seed)
    c["search"] = {"method": policy, "max_cases": f["cases"]}
    if policy.endswith("_v2"):
        c["search"]["policy_version"] = "v2"
    if policy == "weighted_v2":
        # Neutral sealed priors: no ground-truth-dependent ranking advantage.
        axis = c["primary_axis"]
        c["search"]["domain_priors"] = {axis: {str(v): 1.0 for v in c["axes"][axis]["values"]}}
    return c

def order(m, diagnostic, shard=None):
    rows = []
    for f in m["scenarios"]:
        if shard and f["id"] != shard:
            continue
        for seed in m["seeds"][:1] if diagnostic else m["seeds"]:
            arms = [p for p in f["policies"] if not diagnostic or p in m["diagnostic_policies"]]
            random.Random(int(seal([m["suite"], f["id"], seed]), 16)).shuffle(arms)
            rows.extend([[f["id"], p, seed] for p in arms])
    need(bool(rows), "empty scenario shard")
    return rows

def clock():
    def cpu(who):
        x = resource.getrusage(who)
        return round((x.ru_utime + x.ru_stime) * 1e9)
    return {"wall": time.perf_counter_ns(), "parent": cpu(resource.RUSAGE_SELF),
            "child": cpu(resource.RUSAGE_CHILDREN)}

def delta(a, b):
    need(set(a) == set(b) == {"wall", "parent", "child"}, "clock fields")
    need(all(type(a[k]) is int and type(b[k]) is int and 0 <= a[k] <= b[k] for k in a),
         "non-monotone or invalid counters")
    return {k: b[k] - a[k] for k in a}

def call(args, timeout):
    start = clock()
    with subprocess.Popen(list(map(str, args)), stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          start_new_session=True) as proc:
        try:
            out, err = proc.communicate(timeout=max(.001, timeout))
            rc = proc.returncode
        except subprocess.TimeoutExpired:
            try:
                import os
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            out, err = proc.communicate()
            rc = 124
    return {"argv": list(map(str,args)), "returncode": rc, "start": start, "end": clock(),
            "stdout": out.decode(errors="replace"), "stderr": err.decode(errors="replace")}

def database(path, cfg, f):
    if not path.is_file():
        return None
    with sqlite3.connect(path.resolve().as_uri() + "?mode=ro", uri=True) as db:
        db.execute("PRAGMA query_only=ON")
        need(db.execute("PRAGMA integrity_check").fetchone()[0] == "ok", "database integrity")
        stored, digest = db.execute("SELECT config,config_hash FROM experiments WHERE id=1").fetchone()
        need(hashlib.sha256(stored.encode()).hexdigest() == digest, "source config digest")
        c = json.loads(stored)
        need(c["experiment_id"] == cfg["experiment_id"] and c["seed"] == cfg["seed"] and
             c["search_method"] == cfg["search"]["method"] and c["max_cases"] == f["cases"] and
             c["primary_axis"] == cfg["primary_axis"] and c["problem_family"] == cfg["problem_family"],
             "persisted run identity")
        need(c["axes"] == {k:[str(v).lower() if isinstance(v,bool) else str(v) for v in x["values"]]
                          for k,x in cfg["axes"].items()}, "persisted legal axes")
        discovery = db.execute("SELECT COUNT(*) FROM scheduler_decisions").fetchone()[0]
        need(0 <= discovery <= f["cases"], "case budget exceeded")
        terminal = bool(db.execute("SELECT EXISTS(SELECT 1 FROM events WHERE event='search_complete')").fetchone()[0])
        cases = db.execute("SELECT case_id,input,input_sha256 FROM cases").fetchall()
        for cid, raw, digest in cases:
            need(re.fullmatch(r"[0-9a-f]{64}",cid) and hashlib.sha256(raw).hexdigest()==digest,
                 "raw input identity")
        rows = db.execute("SELECT case_id,status,holdout,cpu_ns,peak_rss_bytes FROM samples").fetchall()
        need(all(h in (0,1) and (cpu is None or cpu >= 0) for _,_,h,cpu,_ in rows),"raw cost/status")
        hypotheses = db.execute("SELECT id,status,kind,left_value,right_value FROM hypotheses ORDER BY id").fetchall()
        seals = dict(db.execute("SELECT hypothesis_id,seed_pairs FROM confirmation_seals"))
        held = sum(h == 1 for _,_,h,_,_ in rows)
        confirmed = []
        for hid,status,kind,left,right in hypotheses:
            if status == "CONFIRMED":
                need(hid in seals and seals[hid]>=20 and held>=40,"unsealed positive verdict")
                confirmed.append([hid,kind,left,right])
        # Unknown counters cannot silently become a zero-cost worker win.
        worker = None if not rows or any(x[3] is None for x in rows) else sum(x[3] for x in rows)
        return {"cases":discovery,"terminal":terminal,"confirmed":confirmed,"held":held,
                "invalid_samples":sum(x[1]!="ok" for x in rows),"samples":len(rows),
                "timed_sample_cpu_ns":worker,
                "peak_worker_rss_bytes":max((x[4] for x in rows if x[4] is not None),default=None),
                "hypotheses":[list(x) for x in hypotheses]}

def matches(edges, confirmed):
    """Maximum-cardinality one-to-one match, preferring narrower intervals
    deterministically. Every extra/duplicate promotion remains a false positive.
    """
    ordered = sorted(confirmed, key=lambda h:(h[3]-h[2],h[2],h[3],h[0]))
    adjacency = [[i for i,(l,r) in enumerate(edges) if h[2] <= l < r <= h[3]]
                 if h[2] < h[3] and h[1] == "SYNTHETIC_CALIBRATION" else [] for h in ordered]
    match = {}
    def augment(j, seen):
        for i in adjacency[j]:
            if i in seen:
                continue
            seen.add(i)
            if i not in match or augment(match[i],seen):
                match[i]=j
                return True
        return False
    for j in range(len(ordered)):
        augment(j,set())
    return {"true": len(match),"false":len(confirmed)-len(match),
            "missed":len(edges)-len(match),
            "widths":[ordered[j][3]-ordered[j][2] for i,j in sorted(match.items())]}

def run_trial(binary, folder, f, policy, seed, m):
    start = clock()
    folder.mkdir()
    cfg = config(f,policy,seed)
    write(folder/"config.json",cfg)
    run = folder/"run"
    events=[]
    reason="case_ceiling"
    for i in range(f["cases"]+1):
        d=delta(start,clock())
        if d["parent"]+d["child"]>=m["cpu_ceiling_ns"] or d["wall"]>=m["wall_ceiling_ns"]:
            reason="budget";break
        args=([binary,"explore","--config",folder/"config.json","--out",run,"--pause-after","1"]
              if i==0 else [binary,"resume","--run",run,"--pause-after","1"])
        e=call(args,(m["wall_ceiling_ns"]-d["wall"])/1e9)
        events.append(e)
        if e["returncode"]!=0:
            reason="failed_command";break
        with sqlite3.connect((run/"run.sqlite").resolve().as_uri()+"?mode=ro",uri=True) as db:
            n=db.execute("SELECT COUNT(*) FROM scheduler_decisions").fetchone()[0]
            terminal=bool(db.execute("SELECT EXISTS(SELECT 1 FROM events WHERE event='search_complete')").fetchone()[0])
        if terminal:
            reason="terminal";break
        if n>=f["cases"]:
            reason="case_ceiling";break
        if n<=i:
            reason="no_progress";break
    # Real independent confirmation, logged and charged. Keep every attempt.
    path=run/"run.sqlite"
    if path.is_file() and reason not in ("failed_command","no_progress"):
        with sqlite3.connect(path.resolve().as_uri()+"?mode=ro",uri=True) as db:
            pending=db.execute("SELECT status FROM hypotheses WHERE id='H1'").fetchone()
        d=delta(start,clock())
        if pending==("HYPOTHESIZED",) and d["parent"]+d["child"]<m["cpu_ceiling_ns"] and d["wall"]<m["wall_ceiling_ns"]:
            e=call([binary,"confirm","--run",run],(m["wall_ceiling_ns"]-d["wall"])/1e9)
            events.append(e)
            if e["returncode"]!=0:
                reason="failed_command"
    end=clock()
    write(folder/"commands.json",{"start":start,"end":end,"events":events,"stop":reason})
    # Scoring is independent and outside trial cost. The command/terminal clock
    # ledger includes config setup, process startup, DB progress checks and confirmation.

def row(folder, f, policy, seed, m):
    cfg=config(f,policy,seed)
    need(read(folder/"config.json")==cfg,"changed config")
    log=read(folder/"commands.json")
    cost=delta(log["start"],log["end"])
    events=log["events"]
    need(len(events)<=f["cases"]+2 and log["stop"] in
         ("case_ceiling","budget","terminal","failed_command","no_progress"),"command inventory")
    prev=log["start"]
    failed=False
    command_cost={"wall":0,"parent":0,"child":0}
    for i,e in enumerate(events):
        delta(prev,e["start"])
        d=delta(e["start"],e["end"])
        delta(e["end"],log["end"])
        prev=e["end"]
        cmd=e["argv"][1]
        if i==0:
            need(cmd=="explore" and e["argv"][2:]==["--config",str(folder/"config.json"),"--out",str(folder/"run"),"--pause-after","1"],"initial command")
        elif cmd=="confirm":
            need(i==len(events)-1 and e["argv"][2:]==["--run",str(folder/"run")],"confirmation sequence")
        else:
            need(cmd=="resume" and e["argv"][2:]==["--run",str(folder/"run"),"--pause-after","1"],"resume sequence")
        need(not failed and type(e["returncode"]) is int,"command after failure")
        failed=e["returncode"]!=0
        for k in command_cost:
            command_cost[k]+=d[k]
    need(all(command_cost[k]<=cost[k] for k in cost) and cost["wall"]>0,"invalid total resource ledger")
    failed = failed or log["stop"] in ("failed_command","no_progress")
    path=folder/"run"/"run.sqlite"
    state=database(path,cfg,f)
    need(state is not None or failed or not events,"successful missing SQLite evidence")
    within=cost["parent"]+cost["child"]<=m["cpu_ceiling_ns"] and cost["wall"]<=m["wall_ceiling_ns"]
    actual=state["confirmed"] if state is not None else []
    # Invalid/over-cap campaigns cannot earn a resource-budget success.
    # Still count every observed false promotion, including over-cap results.
    truth=truth_edges(f)
    matching=matches(truth,actual) if truth is not None else None
    eligible=not failed and within and state is not None
    return {"fixture":f["id"],"policy":policy,"seed":seed,"failed":failed,
            "within_ceiling":within,"stop":log["stop"],"cases":state["cases"] if state else 0,
            "parent_cpu_ns":cost["parent"],"waited_cli_cpu_ns":cost["child"],
            "full_cpu_ns":cost["parent"]+cost["child"],"wall_ns":cost["wall"],
            "timed_sample_cpu_ns":state["timed_sample_cpu_ns"] if state else None,
            "raw_state":state,"matching":matching,
            "true_within_budget":matching["true"] if eligible and matching is not None else 0,
            "commands_sha256":sha256_file(folder/"commands.json"),
            "config_sha256":sha256_file(folder/"config.json"),
            "sqlite_sha256":sqlite_snapshot_sha256(path) if path.is_file() else None}

def curve(rows,key,cap):
    # Right-censor failures and nondetections. Terminal time is conservative.
    horizons=[cap*i//20 for i in range(21)]
    values=[sum(r["true_within_budget"]>0 and r[key] is not None and r[key]<=h
                for r in rows)/len(rows) for h in horizons]
    return {"horizons":horizons,"success":values,
            "auc":sum((a+b)/40 for a,b in zip(values,values[1:]))}

def summary(rows,m):
    out=[]
    for f in m["scenarios"]:
        for policy in f["policies"]:
            rs=[r for r in rows if r["fixture"]==f["id"] and r["policy"]==policy]
            if not rs:
                continue
            labelled=f["truth"]!="unlabelled"
            positive=f["truth"]=="single_cliff"
            n=len(rs)
            out.append({"fixture":f["id"],"policy":policy,"trials":n,
                        "failed":sum(r["failed"] for r in rs),
                        "over_ceiling":sum(not r["within_ceiling"] for r in rs),
                        "known_truth_matches":sum(r["matching"]["true"] for r in rs) if labelled else None,
                        "false_promotions":sum(r["matching"]["false"] for r in rs) if labelled else None,
                        "censored_no_true":sum(not r["true_within_budget"] for r in rs) if positive else None,
                        "matched_widths":[w for r in rs for w in r["matching"]["widths"]] if labelled else None,
                        "mean_cpu_utilization":sum(r["full_cpu_ns"]/m["cpu_ceiling_ns"] for r in rs)/n,
                        "mean_wall_utilization":sum(r["wall_ns"]/m["wall_ceiling_ns"] for r in rs)/n,
                        "timed_cpu_missing":sum(r["timed_sample_cpu_ns"] is None for r in rs),
                        "curves":{k:curve(rs,k,cap) for k,cap in
                                  (("cases",f["cases"]),("full_cpu_ns",m["cpu_ceiling_ns"]),
                                   ("wall_ns",m["wall_ceiling_ns"]),("timed_sample_cpu_ns",m["cpu_ceiling_ns"]))} if positive else None})
    return out

def verify(root):
    plan=read(root/"frozen_plan.json")
    digest=plan.pop("sha256")
    m, mh=manifest(plan["suite"])
    check_splits()
    need(seal(plan)==digest and plan["manifest_sha256"]==mh and
         plan["run_order"]==order(m,plan["diagnostic"],plan["shard"]) and
         plan["eligible_for_claim"] is False,"plan mismatch")
    by={f["id"]:f for f in m["scenarios"]}
    rows=[row(root/f"{fid}_{p}_{s}",by[fid],p,s,m) for fid,p,s in plan["run_order"]]
    stored=read(root/"results.json")
    need(stored=={"plan_sha256":digest,"rows":rows},"raw/summary mismatch")
    return {"schema":2,"source_commit":plan["source_commit"],"plan_sha256":digest,
            "diagnostic":plan["diagnostic"],"shard":plan["shard"],"eligible_for_claim":False,
            "sota_claim":False,"summary":summary(rows,m),
            "limitations":["Engine-confirmed calibration work-unit boundaries, not timing truth",
                           "Real families unlabelled; raw confirmed statuses retained",
                           "Existing legacy and v2 seed semantics differ; equivalent legal spaces, not equivalent initial evidence",
                           "V3 paired acquisition uses different timing/confirmation contract and is not ranked here",
                           "Full measured trial CPU counts waited CLI subtree and parent; escaped descendants not certified",
                           "Timed-sample CPU is a separate partial measurement, missing counters not imputed",
                           "Resource horizons separate; no equal-consumption or independence claim",
                           "No independent population FWER, multi-cliff/smooth or external competitiveness verdict"]}

def main():
    a=argparse.ArgumentParser()
    a.add_argument("--binary",type=pathlib.Path)
    a.add_argument("--out",type=pathlib.Path,required=True)
    a.add_argument("--suite",choices=("development","evaluation"),default="development")
    a.add_argument("--diagnostic",action="store_true")
    a.add_argument("--shard")
    a.add_argument("--verify-only",action="store_true")
    args=a.parse_args()
    root=args.out.resolve()
    if args.verify_only:
        need(read(root/"scorecard.json")==verify(root),"scorecard mismatch")
        return
    need(args.binary is not None and args.binary.is_file(),"binary required")
    check_splits()
    m,mh=manifest(args.suite)
    revision=subprocess.check_output(["git","rev-parse","HEAD"],cwd=ROOT,text=True).strip()
    plan={"suite":args.suite,"manifest_sha256":mh,"source_commit":revision,
          "binary_sha256":sha256_file(args.binary),"diagnostic":args.diagnostic,"shard":args.shard,
          "host":platform.platform(),"run_order":order(m,args.diagnostic,args.shard),
          "eligible_for_claim":False}
    digest=seal(plan)
    root.mkdir(parents=True,exist_ok=False)
    write(root/"frozen_plan.json",{**plan,"sha256":digest})
    by={f["id"]:f for f in m["scenarios"]}
    rows=[]
    for fid,p,s in plan["run_order"]:
        folder=root/f"{fid}_{p}_{s}"
        run_trial(args.binary.resolve(),folder,by[fid],p,s,m)
        result=row(folder,by[fid],p,s,m)
        rows.append(result)
        print(json.dumps({"fixture":fid,"policy":p,"seed":s,"failed":result["failed"],
                          "cases":result["cases"],"full_cpu_ns":result["full_cpu_ns"]}),flush=True)
    write(root/"results.json",{"plan_sha256":digest,"rows":rows})
    write(root/"scorecard.json",verify(root))
    need(not any(r["failed"] for r in rows),"failed trials retained")

if __name__=="__main__":
    main()
