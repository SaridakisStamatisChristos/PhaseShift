#!/usr/bin/env python3
"""Independent cache/replay/tamper/repair verification for v2 model sidecar."""
import argparse
import copy
import json
import pathlib
import sqlite3
import subprocess
import tempfile

p=argparse.ArgumentParser()
p.add_argument("--run",type=pathlib.Path,required=True)
p.add_argument("--binary",type=pathlib.Path,required=True)
p.add_argument("--config",type=pathlib.Path,required=True)
a=p.parse_args()
dbpath=a.run/"run.sqlite"
cachepath=a.run/"model_cache_v2.sqlite"
assert cachepath.exists(),"opt-in model sidecar was not created"


def count(path,query):
    with sqlite3.connect(path) as db:
        return db.execute(query).fetchone()[0]


def resume(expect_success):
    p=subprocess.run([str(a.binary),"verify-model-cache","--run",str(a.run)],
                     capture_output=True,text=True,timeout=90)
    assert (p.returncode==0)==expect_success,(p.returncode,p.stdout,p.stderr)


def snapshot():
    with sqlite3.connect(dbpath) as db:
        return (
            db.execute("SELECT COUNT(*) FROM scheduler_decisions").fetchone()[0],
            db.execute("SELECT COUNT(*) FROM samples WHERE holdout=0").fetchone()[0],
            db.execute("SELECT pool_index,case_id,rng_state FROM scheduler_decisions ORDER BY seq").fetchall()
        )


with sqlite3.connect(cachepath) as db:
    assert db.execute("PRAGMA user_version").fetchone()[0]==1
    assert db.execute("SELECT COUNT(*) FROM checkpoints").fetchone()[0]==12
    assert db.execute("SELECT COUNT(DISTINCT case_id) FROM checkpoints").fetchone()[0]==12
    rows=db.execute("SELECT ordinal,pool_index,case_id,rng_hex,cpu_ms,sample_sha256 FROM checkpoints ORDER BY ordinal").fetchall()
    assert [x[0] for x in rows]==list(range(1,13))
    assert all(x[4]>=1 and len(x[5])==64 for x in rows)
before=snapshot()
resume(True)
assert snapshot()==before
# Missing derived records may result from interruption between the
# authoritative COMMIT and nonauthoritative cache write: rebuild on replay.
with sqlite3.connect(cachepath) as db:
    db.execute("DELETE FROM checkpoints WHERE ordinal=12")
resume(True)
assert count(cachepath,"SELECT COUNT(*) FROM checkpoints")==12
assert snapshot()==before
# Tampering must fail closed without modifying the source evidence.
with sqlite3.connect(cachepath) as db:
    prior=db.execute("SELECT sample_sha256 FROM checkpoints WHERE ordinal=12").fetchone()[0]
    db.execute("UPDATE checkpoints SET sample_sha256='corrupt' WHERE ordinal=12")
resume(False)
assert snapshot()==before
with sqlite3.connect(cachepath) as db:
    db.execute("UPDATE checkpoints SET sample_sha256=? WHERE ordinal=12",(prior,))
resume(True)
assert snapshot()==before
# Future records with no authoritative case are never trusted.
with sqlite3.connect(cachepath) as db:
    db.execute("""INSERT INTO checkpoints
      (ordinal,pool_index,case_id,rng_hex,metric,cpu_ms,sample_sha256)
      VALUES (99,99,'fake-future','a',1.,1.,'fake')""")
resume(False)
assert snapshot()==before
with sqlite3.connect(cachepath) as db:
    db.execute("DELETE FROM checkpoints WHERE ordinal=99")
resume(True)
assert snapshot()==before
# V1 and malformed opt-in remain rejected by the executable validation path.
original=json.loads(a.config.read_text())
with tempfile.TemporaryDirectory(prefix="phaseshift-model-v2-invalid-") as scratch:
    legacy=copy.deepcopy(original)
    legacy["search"]={"policy_version":"v1","method":"grid","persist_model_cache_v2":True}
    wrong=copy.deepcopy(original)
    wrong["search"]["persist_model_cache_v2"]=1
    for idx,conf in enumerate((legacy,wrong)):
        path=pathlib.Path(scratch)/(str(idx)+".json")
        path.write_text(json.dumps(conf)+"\n")
        proc=subprocess.run([str(a.binary),"validate","--config",str(path)],
                            capture_output=True,text=True,timeout=20)
        assert proc.returncode!=0,(idx,proc.stdout,proc.stderr)
print("PASS complete durable model cache, repeated read-only replay, missing checkpoint repair, tamper and ahead rejection")
