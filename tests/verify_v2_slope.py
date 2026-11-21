#!/usr/bin/env python3
"""No significance claims: audit persisted exploratory slope-model telemetry."""
import argparse, json, pathlib, sqlite3
p=argparse.ArgumentParser()
p.add_argument("--run",required=True)
r=pathlib.Path(p.parse_args().run)
with sqlite3.connect(r/"run.sqlite") as db:
    records=db.execute(
        "SELECT detail FROM events WHERE event='v2_slope_model_discovery_only'"
    ).fetchall()
assert records, "slope model was never executed"
for (entry,) in records:
    doc=json.loads(entry)
    assert doc["sample_count"]>=6 and doc["model_version"]=="multi_slope_v2",doc
    assert doc["residual_sse"]>=0 and doc["penalized_sse"]>=doc["residual_sse"]
    assert doc["changes"]<=3,doc
    assert doc["confirmatory_significance"] is False
print("PASS v2 slope discovery-only audit",len(records))
