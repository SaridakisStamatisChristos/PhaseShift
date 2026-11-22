#!/usr/bin/env python3
"""Analytic multidimensional work-unit truth. Separate from search policy and CLI."""
import itertools
import json
import random
from pathlib import Path

from protocol import seal, sha256_file

HERE = Path(__file__).resolve().parent
POLICIES = ("conditional-tree", "acquisition-v3")
SHAPES = {"single": "calibration", "steps": "calibration_steps_v3",
          "smooth": "calibration_smooth_v3"}

def require(ok, why):
    if not ok:
        raise ValueError(why)

def label(x):
    return str(x).lower() if isinstance(x, bool) else str(x)

def config(f, seed):
    c = {"schema_version":1, "experiment_id":"v3b-"+f["id"],
         "problem_family":"calibration", "algorithms":[SHAPES[f["shape"]]],
         "primary_axis":"x", "seed":seed, "fixed":{},
         "axes":{k:{"values":f[k]} for k in ("x","threshold","negative")},
         "search":{"policy_version":"v2","method":"uniform_v2","max_cases":24},
         "measurement":{"paired_discovery_blocks":1,"paired_confirm_blocks":20,
                        "warmup_iterations":0},
         "detection":{"min_practical_ratio":1.2},
         "limits":{"total_wall_seconds":45,"max_search_queue":64,
                   "max_case_bytes":1048576,"max_worker_rss_mb":512,
                   "max_sample_wall_ms":1500}}
    if f["scopes"]:
        c["search"]["admissible_subspaces"] = f["scopes"]
    return c

def domain(c):
    keys = list(c["axes"])
    allowed = [c["axes"][k]["values"] for k in keys]
    scopes = c["search"].get("admissible_subspaces", [])
    return [dict(zip(keys, xs)) for xs in itertools.product(*allowed)
            if not scopes or any(all(dict(zip(keys,xs))[k] in vals
                                     for k,vals in s.items()) for s in scopes)]

def groups(c):
    d = {}
    for p in domain(c):
        context = tuple((k, label(v)) for k,v in p.items() if k != "x")
        d.setdefault(context, []).append(p)
    for points in d.values():
        points.sort(key=lambda p:p["x"])
    return d

def jumps(f, a, b):
    if a["negative"] or f["shape"] == "smooth":
        return []
    t = a["threshold"]
    cuts = [t,t+4] if f["shape"] == "steps" else [t]
    return [x for x in cuts if a["x"] < x <= b["x"]]

def truth(f):
    out = []
    for context, points in groups(config(f, 0)).items():
        for a,b in zip(points,points[1:]):
            found = jumps(f,a,b)
            if found:
                out.append({"context":[list(x) for x in context],
                            "left":a["x"],"right":b["x"],"jumps":found})
    return sorted(out,key=lambda x:(x["context"],x["left"],x["right"]))

def key(context,left,right):
    return (tuple((str(k),str(v)) for k,v in context),int(left),int(right))

def classify(f, nominations):
    target = {key(e["context"],e["left"],e["right"]) for e in truth(f)}
    hits=set();false=set();first=None;repeated=0;bad=0;total=0
    for p in nominations:
        if not p["nominate"]:
            continue
        total += 1
        k=key(p["context"],p["left"],p["right"])
        if k in target and p["effect"]>0:
            if k in hits:
                repeated+=1
            else:
                hits.add(k)
                if first is None:
                    first=p["pair_number"]+1
        else:
            false.add(k);bad+=1
    return {"truth_edge_count":len(target),"true_edges_hit":len(hits),
            "missed_truth_edges":len(target)-len(hits),
            "false_observations":bad,"unique_false_edges":len(false),
            "repeat_true_nominations":repeated,
            "nomination_observations":total,"first_true_pair":first}

def load(suite):
    require(suite in ("development","evaluation"),"invalid suite")
    path=HERE/("multidim_"+suite+".json")
    m=json.loads(path.read_text())
    require(m["schema"]==1 and m["suite"]==suite and
            m["eligible_for_claim"] is False and m["sota_claim"] is False,
            "manifest schema")
    require(len(m["seeds"])==len(set(m["seeds"]))==20 and
            all(type(s) is int and 0<=s<2**31 for s in m["seeds"]),
            "seed inventory")
    require(len(m["diagnostic_seeds"])==1 and
            set(m["diagnostic_seeds"])<=set(m["seeds"]),"diagnostic cohort")
    require(m["policies"]==list(POLICIES) and
            type(m["pairs"]) is int and 1<=m["pairs"]<=100,"search contract")
    require(1_000_000<=m["cpu_ceiling_ns"]<2**63 and
            1_000_000<=m["wall_ceiling_ns"]<2**63,"cost budgets")
    fs=m["fixtures"]
    require(len(fs)>=5 and len({f["id"] for f in fs})==len(fs),"fixture inventory")
    for f in fs:
        require(f["shape"] in SHAPES and f["id"].replace("_","").isalnum(),
                "fixture shape")
        xs=f["x"]; ts=f["threshold"];ns=f["negative"]
        require(3<=len(xs)<=128 and xs==sorted(set(xs)) and
                all(type(x) is int and 0<=x<=10000 for x in xs),"x domain")
        require(1<=len(ts)<=16 and ts==sorted(set(ts)) and
                all(type(t) is int and t>0 for t in ts),"threshold domain")
        require(bool(ns) and all(type(n) is bool for n in ns) and
                len(ns)==len(set(ns)),"negative contexts")
        require(type(f["scopes"]) is list and len(f["scopes"])<=64,
                "stratum scope")
        for s in f["scopes"]:
            require(type(s) is dict and s and set(s)<={"threshold","negative"},
                    "illegal stratum key")
            for k, vals in s.items():
                require(type(vals) is list and vals and len(vals)==len(set(vals))
                        and all(type(v) is type(f[k][0]) and v in f[k] for v in vals),
                        "illegal stratum values")
        c=config(f,0)
        points=domain(c)
        require(0<len(points)<=8192 and
                any(len(p)>=2 for p in groups(c).values()),"legal domain")
        require(len(truth(f))<=len(points),"truth cardinality")
    return m,sha256_file(path)

def split_check():
    a,_=load("development"); b,_=load("evaluation")
    require(set(a["seeds"]).isdisjoint(b["seeds"]),"seed leakage")
    require({seal(config(f,0)) for f in a["fixtures"]}.isdisjoint(
            {seal(config(f,0)) for f in b["fixtures"]}),"config leakage")

def order(m,diagnostic,shard=None):
    require(shard is None or shard in {f["id"] for f in m["fixtures"]},
            "unknown fixture shard")
    rows=[]
    for f in m["fixtures"]:
        if shard is not None and f["id"]!=shard:
            continue
        for seed in m["diagnostic_seeds"] if diagnostic else m["seeds"]:
            arms=m["policies"][:]
            random.Random(int(seal([m["suite"],f["id"],seed]),16)).shuffle(arms)
            rows.extend([[f["id"],p,seed] for p in arms])
    return rows
