#!/usr/bin/env python3
"""Independent v11 adaptive real-worker and immutable-transcript falsification."""
import argparse
import json
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess

def invoke(binary, *args, expect=0, env=None):
    p = subprocess.run([binary, *map(str,args)], capture_output=True,
                       text=True, timeout=120, env=env)
    assert p.returncode == expect, (args, p.returncode, p.stdout, p.stderr)
    return p

def extract(root, count):
    with sqlite3.connect(str(root/"run.sqlite")) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0] == 11
        data = db.execute("""
          SELECT p.pair_number,p.seed,p.left_first,d.left_label,d.right_label,
                 d.arm,d.prior_evidence_digest,p.status,p.log_contrast
          FROM v3_pairs p JOIN v3_adaptive_decisions d USING(pair_number)
          ORDER BY p.pair_number
        """).fetchall()
        assert len(data)==count
        assert [r[0] for r in data]==list(range(count))
        assert len(set(r[1] for r in data))==count
        edges={"0":0,"1":1,"2":2,"3":3}
        for i,seed,order,left,right,arm,digest,status,value in data:
            assert order in (0,1)
            assert edges[right] == edges[left]+1
            assert arm in ("explore","replicate")
            assert len(digest)==64
            assert (status=="ok") == (value is not None)
            samples=db.execute("""
               SELECT s.side,c.seed,s.status,s.ns,c.input_sha256
               FROM v3_samples s JOIN v3_cases c USING(case_id)
               WHERE s.pair_number=? ORDER BY s.side,s.ordinal
            """,(i,)).fetchall()
            assert len(samples)==2
            assert [r[0] for r in samples]==[0,1]
            assert all(r[1]==seed and len(r[4])==64 for r in samples)
            assert all((r[2]=="ok")==(r[3] is not None) for r in samples)
        return data

def corrupted_copy(binary,root,field,query):
    corrupted=root.parent/field
    shutil.copytree(root,corrupted)
    with sqlite3.connect(str(corrupted/"run.sqlite")) as db:
        db.execute(query)
    invoke(binary,"verify","--run",corrupted,expect=2)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--binary",required=True)
    parser.add_argument("--config",required=True)
    parser.add_argument("--out",required=True)
    args=parser.parse_args()
    root=Path(args.out)
    shutil.rmtree(root,ignore_errors=True)
    root.mkdir(parents=True)
    common=("--config",args.config,"--pairs","6")
    normal=root/"resumed"
    invoke(args.binary,"adaptive",*common,"--out",normal,"--pause-after","2")
    before=extract(normal,2)
    assert [r[5] for r in before]==["explore","explore"]
    invoke(args.binary,"verify","--run",normal)
    invoke(args.binary,"resume","--run",normal)
    full=extract(normal,6)
    assert full[:2]==before
    assert full[2][5]=="replicate" # admissible matched evidence after two worker pairs
    assert full[4][5]=="explore" # forced 25% new-structure reserve
    assert len(set((r[3],r[4]) for r in full if r[5]=="explore"))==3
    invoke(args.binary,"verify","--run",normal)
    invoke(args.binary,"resume","--run",normal)
    assert extract(normal,6)==full
    # Fresh-run seeds and first two structural choices are independent of
    # timings. Later adaptive actions may legitimately differ between runs.
    independent=root/"independent"
    invoke(args.binary,"adaptive",*common,"--out",independent)
    again=extract(independent,6)
    assert [(r[1],r[3],r[4],r[5]) for r in again[:2]]==[
        (r[1],r[3],r[4],r[5]) for r in before]
    invoke(args.binary,"verify","--run",independent)

    # Recover both crash windows via immutable pair checkpointing.
    for where,exit_code,committed in (
        ("before_pair_commit",88,0),
        ("after_pair_commit",89,1)):
        run=root/where
        env=dict(os.environ,PHASESHIFT_V3_FAILPOINT=where)
        invoke(args.binary,"adaptive",*common,"--out",run,env=env,expect=exit_code)
        extract(run,committed)
        invoke(args.binary,"resume","--run",run)
        extract(run,6)
        invoke(args.binary,"verify","--run",run)

    # Mutating the decision transcript, not only raw case status, is detected.
    corrupted_copy(args.binary,normal,"bad_arm",
        "UPDATE v3_adaptive_decisions SET arm='replicate' WHERE pair_number=0")
    corrupted_copy(args.binary,normal,"bad_digest",
        "UPDATE v3_adaptive_decisions SET prior_evidence_digest=hex(zeroblob(32)) WHERE pair_number=0")
    corrupted_copy(args.binary,normal,"bad_endpoint",
        "UPDATE v3_adaptive_decisions SET right_label='3' WHERE pair_number=0")
    corrupted_copy(args.binary,normal,"bad_seed",
        "UPDATE v3_pairs SET seed=seed+1000 WHERE pair_number=0")
    corrupted_copy(args.binary,normal,"bad_contrast",
        "UPDATE v3_pairs SET log_contrast=log_contrast+0.9 WHERE pair_number=0")
    # Configuration/order validation and contract bounds: no silent fallback.
    invoke(args.binary,"adaptive",*common,"--out",normal,expect=2)
    invoke(args.binary,"adaptive","--config",args.config,"--pairs","101",
           "--out",root/"limit",expect=2)
    invoke(args.binary,"adaptive",*common,"--out",root/"unknown",
           "--left","0",expect=2)
    # Explicitly illegal descending structural coordinate order.
    raw=json.loads(Path(args.config).read_text(encoding="utf-8"))
    raw["axes"]["x"]["values"]=[3,2,1,0]
    malformed=root/"descending.json"
    malformed.write_text(json.dumps(raw),encoding="utf-8")
    invoke(args.binary,"adaptive","--config",malformed,"--out",
           root/"descending","--pairs","3",expect=2)
    print("PASS v11 adaptive worker, forced exploration, replica, deterministic replay, crash and tamper")

if __name__=="__main__":
    main()
