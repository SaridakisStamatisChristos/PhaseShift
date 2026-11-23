#!/usr/bin/env python3
"""Schema-v13 frozen implications: independent finite truth, replay, worker and tamper gate."""
import argparse
import json
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess

def check(cond, detail):
    if not cond:
        raise AssertionError(detail)

def invoke(binary, *arguments, expect=0, env=None):
    p=subprocess.run([binary,*map(str,arguments)],text=True,capture_output=True,
                     timeout=120,env=env)
    check(p.returncode==expect,
          f"{arguments} returned {p.returncode}; expected {expect}\n{p.stdout}\n{p.stderr}")
    return p

def decode(text):
    pos=0
    output={}
    while pos<len(text):
        colon=text.index(":",pos)
        size=int(text[pos:colon])
        key=text[colon+1:colon+1+size]
        pos=colon+1+size
        colon=text.index(":",pos)
        size=int(text[pos:colon])
        value=text[colon+1:colon+1+size]
        pos=colon+1+size
        check(key not in output,"duplicate encoded axis")
        output[key]=value
    return output

# Independent exhaustive oracle over 16 legal/illegal candidate tuples.
ALL={(n,t,x) for n in ("false","true")
               for t in ("2","3")
               for x in ("0","1","2","3")}
TRUTH={(n,t,x) for n,t,x in ALL
       if ((n=="false" and x in ("0","1")) or
           (n=="true" and t=="3" and x in ("2","3")))}
check(len(TRUTH)==6,"independent oracle setup")

def inspect(root,count):
    with sqlite3.connect(str(root/"run.sqlite")) as db:
        check(db.execute("PRAGMA user_version").fetchone()[0]==13,
              "new opt-in schema must be 13")
        rows=db.execute("""
          SELECT p.pair_number,p.seed,p.left_first,p.status,p.log_contrast,
                 d.axis,d.left_coords,d.right_coords,d.arm,d.prior_evidence_digest
          FROM v3_pairs p JOIN v3_conditional_decisions d USING(pair_number)
          ORDER BY p.pair_number""").fetchall()
        check(len(rows)==count,"atomic pair+decision count")
        check([x[0] for x in rows]==list(range(count)),"discovery ordinal gap")
        check(len({x[1] for x in rows})==count,"fresh matched seed reused")
        inspected=[]
        for i,seed,order,status,metric,axis,l,r,arm,digest in rows:
            left,right=decode(l),decode(r)
            check(set(left)==set(right)=={"negative","threshold","x"},
                  "missing or invented structural coordinates")
            check(axis=="x","ordered primary axis changed")
            a=(left["negative"],left["threshold"],left["x"])
            b=(right["negative"],right["threshold"],right["x"])
            check(a in TRUTH and b in TRUTH,"uncompiled or forbidden implication candidate")
            check(a[:2]==b[:2],"paired comparison changed categorical context")
            check((a[2],b[2]) in (("0","1"),("2","3")),
                  "nonadjacent or illegal edge")
            check(order in (0,1) and arm in ("explore","replicate") and
                  len(digest)==64,"invalid deterministic design")
            check((status=="ok")== (metric is not None),
                  "wrong status promoted into finite contrast")
            samples=db.execute("""
               SELECT s.side,c.seed,s.status,s.ns,s.case_id,length(c.input),c.input_sha256
               FROM v3_samples s JOIN v3_cases c USING(case_id)
               WHERE s.pair_number=? ORDER BY s.side,s.ordinal""",(i,)).fetchall()
            check(len(samples)==2 and [x[0] for x in samples]==[0,1],
                  "not two raw worker observations")
            check(all(sample[1]==seed and sample[5]>0 and len(sample[6])==64
                      for sample in samples),"worker input/seed provenance missing")
            check(all((sample[2]=="ok")== (sample[3] is not None) for sample in samples),
                  "failed worker got numeric speed")
            inspected.append((a[:2],arm))
        return rows,inspected

def tamper(binary,source,root,name,sql):
    dest=root/name
    shutil.copytree(source,dest)
    with sqlite3.connect(str(dest/"run.sqlite")) as db:
        db.execute(sql)
    invoke(binary,"verify","--run",dest,expect=2)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--binary",required=True)
    ap.add_argument("--config",required=True)
    ap.add_argument("--old-config",required=True)
    ap.add_argument("--out",required=True)
    args=ap.parse_args()
    root=Path(args.out)
    shutil.rmtree(root,ignore_errors=True)
    root.mkdir(parents=True)
    common=("--config",args.config,"--pairs","6")
    run=root/"resumed"
    invoke(args.binary,"conditional-rules",*common,"--out",run,"--pause-after","2")
    before,contexts=inspect(run,2)
    check([x[1] for x in contexts]==["explore","explore"],
          "initial frozen rare-stratum screening missing")
    invoke(args.binary,"verify","--run",run)
    invoke(args.binary,"resume","--run",run)
    final,contexts=inspect(run,6)
    check(final[:2]==before,"resume changed committed worker evidence")
    check({context for context,arm in contexts if arm=="explore"}==
          {("false","2"),("false","3"),("true","3")},
          "rare logical-implication stratum skipped")
    check(any(arm=="replicate" for context,arm in contexts),
          "no independent seed-matched replication")
    invoke(args.binary,"verify","--run",run)
    invoke(args.binary,"resume","--run",run)
    check(inspect(run,6)[0]==final,"completed resume not idempotent")
    other=root/"fresh"
    invoke(args.binary,"conditional-rules",*common,"--out",other)
    repeated=inspect(other,6)[0]
    check([(x[1],x[2],x[5],x[6],x[7],x[8]) for x in before]==
          [(x[1],x[2],x[5],x[6],x[7],x[8]) for x in repeated[:2]],
          "fresh workers altered deterministic pre-feedback schedule")

    for failpoint,exit_code,pairs in (("before_pair_commit",88,0),
                                      ("after_pair_commit",89,1)):
        target=root/failpoint
        env=dict(os.environ,PHASESHIFT_V3_FAILPOINT=failpoint)
        invoke(args.binary,"conditional-rules",*common,"--out",target,
               expect=exit_code,env=env)
        inspect(target,pairs)
        invoke(args.binary,"resume","--run",target)
        inspect(target,6)
        invoke(args.binary,"verify","--run",target)

    tamper(args.binary,run,root,"tampered_arm",
           "UPDATE v3_conditional_decisions SET arm='replicate' WHERE pair_number=0")
    tamper(args.binary,run,root,"tampered_context",
           "UPDATE v3_conditional_decisions SET left_coords='1:x1:8' WHERE pair_number=0")
    tamper(args.binary,run,root,"tampered_digest",
           "UPDATE v3_conditional_decisions SET prior_evidence_digest=hex(zeroblob(32)) WHERE pair_number=0")
    tamper(args.binary,run,root,"tampered_raw",
           "UPDATE v3_samples SET ns=ns+100000 WHERE pair_number=0 AND side=0")
    tamper(args.binary,run,root,"tampered_seed",
           "UPDATE v3_pairs SET seed=seed+101 WHERE pair_number=0")

    # Never reinterpret legacy v12 conditional protocol as implication capable.
    invoke(args.binary,"conditional",*common,"--out",root/"not_v12",expect=2)
    # Nor allow an empty rule set to smuggle a schema-v13 provenance label.
    invoke(args.binary,"conditional-rules","--config",args.old_config,
           "--pairs","3","--out",root/"empty_rules",expect=2)
    # Fail closed on malformed frozen rule definitions.
    broken=json.loads(Path(args.config).read_text(encoding="utf-8"))
    broken["search"]["conditional_rules"][0]["when"]["axis"]="nonexistent"
    bad=root/"bad_rule.json"
    bad.write_text(json.dumps(broken),encoding="utf-8")
    invoke(args.binary,"conditional-rules","--config",bad,
           "--pairs","3","--out",root/"illegal_rule",expect=2)
    print("PASS v13 real-worker implication compiler, six-cell truth, rare screening, replay, crash and tampering")

if __name__=="__main__":
    main()
