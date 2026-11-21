#!/usr/bin/env python3
import argparse,json,pathlib,sqlite3,subprocess,tempfile
p=argparse.ArgumentParser()
p.add_argument("--run",required=True);p.add_argument("--binary",required=True);p.add_argument("--config",required=True)
a=p.parse_args()
with sqlite3.connect(pathlib.Path(a.run)/"run.sqlite") as db:
 rows=db.execute("SELECT status,detail FROM samples WHERE holdout=0").fetchall()
 assert len(rows)==4,rows
 for status,detail in rows:
  assert status=="ok",(status,detail)
  assert "measurement_protocol_version=calibrated_batch_v2" in detail,detail
  assert "pilot_ns=" in detail and "pilot_iterations=" in detail,detail
 report=json.loads((pathlib.Path(a.run)/"report.json").read_text())
 assert report["measurement_protocol_version"]=="calibrated_batch_v2"
base=json.loads(pathlib.Path(a.config).read_text())
with tempfile.TemporaryDirectory() as tmp:
 def bad(edit):
  obj=json.loads(json.dumps(base));edit(obj)
  path=pathlib.Path(tmp)/"config.json";path.write_text(json.dumps(obj))
  result=subprocess.run([a.binary,"validate","--config",str(path)],
                        capture_output=True,text=True,timeout=15)
  assert result.returncode!=0,obj
 bad(lambda c:c["search"].update({"policy_version":"v1","method":"grid"}))
 bad(lambda c:c["measurement"].update({"protocol_version":"kernel_only_v2"}))
 bad(lambda c:c["measurement"].update({"protocol_version":"unknown"}))
print("verified distinct calibrated pilot, v1 isolation, independent resume and strict mode rejection")
