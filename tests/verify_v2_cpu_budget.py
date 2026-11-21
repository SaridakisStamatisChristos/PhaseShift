#!/usr/bin/env python3
import argparse,json,pathlib,sqlite3,subprocess,tempfile
p=argparse.ArgumentParser()
p.add_argument("--run",required=True);p.add_argument("--binary",required=True);p.add_argument("--config",required=True)
a=p.parse_args()
with sqlite3.connect(pathlib.Path(a.run)/"run.sqlite") as db:
 assert db.execute("PRAGMA user_version").fetchone()[0]==8
 n=db.execute("SELECT COUNT(*) FROM scheduler_decisions").fetchone()[0]
 limits=db.execute("SELECT detail FROM events WHERE event='v2_sampled_cpu_limit_reached'").fetchall()
 events=db.execute("SELECT detail FROM events WHERE event='v2_sampled_cpu_budget_ledger'").fetchall()
 complete=db.execute("SELECT COUNT(*) FROM events WHERE event='search_complete'").fetchone()[0]
 assert n==1,(n,limits)
 assert len(limits)==1 and complete==1,(limits,complete)
 assert len(events)==1 and json.loads(events[0][0])["accounting_complete"],events
 ledger=db.execute("SELECT cumulative_sampled_cpu_ns,max_sampled_cpu_ns,accounting_complete FROM budget_ledger").fetchall()
 assert len(ledger)==1 and ledger[0][0]>=ledger[0][1] and ledger[0][2]==1,ledger
 before=(n,limits,complete)
 result=subprocess.run([a.binary,"resume","--run",a.run],capture_output=True,text=True,timeout=20)
 assert result.returncode==0,(result.stdout,result.stderr)
 n2=db.execute("SELECT COUNT(*) FROM scheduler_decisions").fetchone()[0]
 complete2=db.execute("SELECT COUNT(*) FROM events WHERE event='search_complete'").fetchone()[0]
 assert (n2,complete2)==(n,complete),(n2,complete2)
# A fully finalized, apparently idempotent run must still validate its
# exact sample-derived CPU ledger. Tamper a transactionally backed-up copy.
with tempfile.TemporaryDirectory() as corrupt_root:
 bad_run=pathlib.Path(corrupt_root)/"bad"
 bad_run.mkdir()
 with sqlite3.connect(bad_run/"run.sqlite") as copy_db:
  db.backup(copy_db)
  copy_db.execute("UPDATE budget_ledger SET cumulative_sampled_cpu_ns=cumulative_sampled_cpu_ns+1")
  copy_db.commit()
 tamper=subprocess.run([a.binary,"resume","--run",str(bad_run)],
                       capture_output=True,text=True,timeout=20)
 assert tamper.returncode!=0 and "budget ledger" in tamper.stderr.lower(),(tamper.stdout,tamper.stderr)
base=json.loads(pathlib.Path(a.config).read_text())
with tempfile.TemporaryDirectory() as temp:
 def bad(edit):
  cfg=json.loads(json.dumps(base));edit(cfg)
  path=pathlib.Path(temp)/"config.json";path.write_text(json.dumps(cfg))
  res=subprocess.run([a.binary,"validate","--config",str(path)],capture_output=True,text=True,timeout=15)
  assert res.returncode!=0,cfg
 bad(lambda c:c["search"].update({"policy_version":"v1"}))
 bad(lambda c:c["limits"].update({"max_sampled_cpu_ns":-1}))
 bad(lambda c:c["limits"].update({"max_sampled_cpu_ns":1.3}))
print("verified per-case timed CPU ledger, cap, terminal resume and fail-closed config")
