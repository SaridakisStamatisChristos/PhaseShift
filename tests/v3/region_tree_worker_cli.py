#!/usr/bin/env python3
"""V3-04 independent schema-v14 legal region-tree real-worker falsifiers."""
import argparse
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess

def check(ok, message):
    if not ok:
        raise AssertionError(message)

def invoke(binary,*args,expect=0,env=None):
    p=subprocess.run([binary,*map(str,args)],capture_output=True,text=True,
                     env=env,timeout=120)
    check(p.returncode==expect,
          f"{args}: rc={p.returncode} expected {expect}\n{p.stdout}\n{p.stderr}")
    return p

def coords(s):
    out={}
    p=0
    for _ in range(12):
        if p==len(s):
            return out
        j=s.index(":",p)
        size=int(s[p:j])
        key=s[j+1:j+1+size]
        p=j+1+size
        j=s.index(":",p)
        size=int(s[p:j])
        val=s[j+1:j+1+size]
        p=j+1+size
        check(key not in out,"duplicate encoded axis")
        out[key]=val
    raise AssertionError("oversized coordinates")

def inspect(folder,count):
    with sqlite3.connect(str(folder/"run.sqlite")) as db:
        check(db.execute("PRAGMA user_version").fetchone()[0]==14,"schema v14")
        rows=db.execute("""
          SELECT p.pair_number,p.seed,p.left_first,d.axis,
                 d.left_coords,d.right_coords,d.arm,d.prior_evidence_digest,
                 p.status,p.log_contrast
          FROM v3_pairs p INNER JOIN v3_conditional_decisions d USING(pair_number)
          ORDER BY p.pair_number""").fetchall()
        check(len(rows)==count,"atomic decision/worker count mismatch")
        check([r[0] for r in rows]==list(range(count)),"pair numbering gap")
        check(len(set(r[1] for r in rows))==count,"reused discovery seed")
        contexts=[]
        for number,seed,order,axis,l,r,arm,digest,status,metric in rows:
            left,right=coords(l),coords(r)
            check(left and right and len(left)==3 and len(right)==3,"missing axes")
            check(axis=="x" and left["x"]!=right["x"],"not a structural primary pair")
            check(left["negative"]==right["negative"] and
                  left["threshold"]==right["threshold"],"context changed within pair")
            check((left["negative"]=="false" and
                  left["x"] in ("0","1") and right["x"] in ("0","1")) or
                  (left["negative"]=="true" and
                  left["threshold"]=="3" and
                  left["x"] in ("2","3") and right["x"] in ("2","3")),
                  "proposed forbidden coordinate under disjoint rectangle contract")
            check(arm in ("explore","replicate") and len(digest)==64 and
                  order in (0,1),"adaptive design invalid")
            check((status=="ok")== (metric is not None),"failed status promoted to finite effect")
            samples=db.execute("""
              SELECT s.side,c.seed,s.status,s.ns,length(c.input),c.input_sha256
              FROM v3_samples s JOIN v3_cases c USING(case_id)
              WHERE s.pair_number=? ORDER BY s.side,s.ordinal""",(number,)).fetchall()
            check(len(samples)==2 and [r[0] for r in samples]==[0,1],"real two-sided worker")
            check(all(s[1]==seed and s[4]>0 and len(s[5])==64 for s in samples),
                  "raw samples not bound to one fresh matched seed")
            check(all((s[2]=="ok")== (s[3] is not None) for s in samples),
                  "invalid sample speed promotion")
            contexts.append((left["negative"],left["threshold"],arm))
        return rows,contexts

def tamper(binary,normal,root,name,sql):
    dest=root/name
    shutil.copytree(normal,dest)
    with sqlite3.connect(str(dest/"run.sqlite")) as db:
        db.execute(sql)
    invoke(binary,"verify","--run",dest,expect=2)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--binary",required=True)
    parser.add_argument("--config",required=True)
    parser.add_argument("--unsupported-config",required=True)
    parser.add_argument("--out",required=True)
    args=parser.parse_args()
    root=Path(args.out)
    shutil.rmtree(root,ignore_errors=True)
    root.mkdir(parents=True)
    common=("--config",args.config,"--pairs","6")
    run=root/"resumed"
    invoke(args.binary,"conditional-tree",*common,"--out",run,"--pause-after","2")
    before,contexts=inspect(run,2)
    check([r[-4] for r in before]==["explore","explore"],"first two must discover")
    check(contexts[0][:2]!=contexts[1][:2],"rare strata were ignored")
    invoke(args.binary,"verify","--run",run)
    invoke(args.binary,"resume","--run",run)
    after,contexts=inspect(run,6)
    check(after[:2]==before,"resume rewrote committed evidence")
    check(any(c[2]=="replicate" for c in contexts[2:]),"no independent matched replication")
    check({c[:2] for c in contexts if c[2]=="explore"}==
          {("false","2"),("true","3"),("false","3")},
          "all admissible category contexts must be screened")
    invoke(args.binary,"verify","--run",run)
    invoke(args.binary,"resume","--run",run)
    check(inspect(run,6)[0]==after,"idempotent completed resume drift")
    invoke(args.binary,"conditional-tree",*common,"--out",run,expect=2)

    fresh=root/"independent"
    invoke(args.binary,"conditional-tree",*common,"--out",fresh)
    b=inspect(fresh,6)[0]
    check([(x[1],x[2],x[3],x[4],x[5],x[6]) for x in b[:2]]==
          [(x[1],x[2],x[3],x[4],x[5],x[6]) for x in before],
          "first two planned pairs differ with worker scheduling")

    for fault,code,count in (("before_pair_commit",88,0),("after_pair_commit",89,1)):
        target=root/fault
        env=dict(os.environ,PHASESHIFT_V3_FAILPOINT=fault)
        invoke(args.binary,"conditional-tree",*common,"--out",target,env=env,expect=code)
        inspect(target,count)
        invoke(args.binary,"resume","--run",target)
        inspect(target,6)
        invoke(args.binary,"verify","--run",target)

    tamper(args.binary,run,root,"arm",
           "UPDATE v3_conditional_decisions SET arm=CASE WHEN arm='explore' THEN 'replicate' ELSE 'explore' END WHERE pair_number=2")
    tamper(args.binary,run,root,"prior",
           "UPDATE v3_conditional_decisions SET prior_evidence_digest=hex(zeroblob(32)) WHERE pair_number=1")
    tamper(args.binary,run,root,"coordinate",
           "UPDATE v3_conditional_decisions SET left_coords='1:x1:9' WHERE pair_number=0")
    tamper(args.binary,run,root,"seed",
           "UPDATE v3_pairs SET seed=seed+1 WHERE pair_number=0")
    tamper(args.binary,run,root,"effect",
           "UPDATE v3_pairs SET log_contrast=log_contrast+0.6 WHERE pair_number=0")
    # Old v12 must continue to reject rules while v14 accepts them.
    invoke(args.binary,"conditional","--config",args.unsupported_config,
           "--pairs","3","--out",root/"v12_implication_rejected",expect=2)
    rule_run=root/"v14_rules"
    invoke(args.binary,"conditional-tree","--config",args.unsupported_config,
           "--pairs","6","--out",rule_run)
    with sqlite3.connect(str(rule_run/"run.sqlite")) as db:
        check(db.execute("PRAGMA user_version").fetchone()[0]==14,
              "v14 must preserve independent rule-aware run identity")
    invoke(args.binary,"verify","--run",rule_run)
    invoke(args.binary,"resume","--run",rule_run)
    invoke(args.binary,"conditional-tree",*common,"--out",root/"illegal",
           "--left","0",expect=2)
    print("PASS v14 region tree: disjoint and implication legal spaces, real workers, rare strata, crash, replay, tampering")

if __name__=="__main__":
    main()
