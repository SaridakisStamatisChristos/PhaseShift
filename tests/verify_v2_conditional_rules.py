#!/usr/bin/env python3
"""Independent SQLite oracle: conditional implication feasibility and replay."""
import argparse
import copy
import json
import pathlib
import sqlite3
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument("--paused", required=True)
p.add_argument("--full", required=True)
p.add_argument("--config", required=True)
p.add_argument("--binary", required=True)
args = p.parse_args()


def observe(root):
    with sqlite3.connect(str(pathlib.Path(root) / "run.sqlite")) as db:
        rows = db.execute(
            "SELECT seq,pool_index,case_id,rng_state FROM scheduler_decisions ORDER BY seq"
        ).fetchall()
        cases = db.execute(
            "SELECT c.case_id,c.params,c.seed FROM cases c "
            "JOIN scheduler_decisions d ON d.case_id=c.case_id ORDER BY d.seq"
        ).fetchall()
        config = json.loads(db.execute(
            "SELECT config FROM experiments WHERE id=1"
        ).fetchone()[0])
        return rows, cases, config


paused, full = observe(args.paused), observe(args.full)
assert paused == full, "pause-resume altered immutable decisions, cases or config"
decisions, cases, config = paused
assert len(decisions) == len(cases) == 12
assert len({row[1] for row in decisions}) == 12
assert config["policy_version"] == "v2"
assert len(config["search"]["conditional_rules"]) == 3
assert config["search"]["admissible_subspaces"]
for case_id, params, seed in cases:
    j = json.loads(params)
    x, negative, threshold = int(j["x"]), bool(j["negative"]), int(j["threshold"])
    assert (not negative and x in (0, 1)) or (
        negative and x in (2, 3) and threshold == 3
    ), (case_id, j)
    assert seed in (337, 338)

initial = json.loads(pathlib.Path(args.config).read_text())
cases_bad = []

def change(name, fn):
    config = copy.deepcopy(initial)
    fn(config)
    cases_bad.append((name, config))

change("v1_forbidden",lambda c:c["search"].update(policy_version="v1",method="grid"))
change("missing_rule_clause",lambda c:c["search"]["conditional_rules"][0].pop("require"))
change("extra_rule_clause",lambda c:c["search"]["conditional_rules"][0].update(other={}))
change("missing_axis",lambda c:c["search"]["conditional_rules"][0]["when"].pop("axis"))
change("extra_clause_key",lambda c:c["search"]["conditional_rules"][0]["when"].update(extra=True))
change("unknown_axis",lambda c:c["search"]["conditional_rules"][0]["when"].update(axis="not_real"))
change("unknown_label",lambda c:c["search"]["conditional_rules"][0]["when"].update(values=["nope"]))
change("empty_labels",lambda c:c["search"]["conditional_rules"][0]["when"].update(values=[]))
change("duplicate_labels",lambda c:c["search"]["conditional_rules"][0]["when"].update(values=[True,True]))
change("nonnumeric_struct",lambda c:c["search"].update(conditional_rules={}))
change("empty_rules",lambda c:c["search"].update(conditional_rules=[]))
change("too_many_rules",lambda c:c["search"].update(conditional_rules=c["search"]["conditional_rules"]*11))
change("unavailable_constraint",lambda c:c["search"].update(
    conditional_rules=[
        {"when":{"axis":"negative","values":[True]},
         "require":{"axis":"x","values":[0]}},
        {"when":{"axis":"negative","values":[True]},
         "require":{"axis":"x","values":[1]}},
        {"when":{"axis":"negative","values":[False]},
         "require":{"axis":"negative","values":[True]}}
    ]))
# Falsify nested typed schema boundaries independently of successful sampling.
change("when_clause_wrong_type",
       lambda c:c["search"]["conditional_rules"][0].update(when="x"))
change("when_clause_null",
       lambda c:c["search"]["conditional_rules"][0].update(when=None))
change("require_clause_wrong_type",
       lambda c:c["search"]["conditional_rules"][0].update(require=["x"]))
change("require_clause_null",
       lambda c:c["search"]["conditional_rules"][0].update(require=None))
change("when_clause_empty",
       lambda c:c["search"]["conditional_rules"][0].update(when={}))
change("require_clause_empty",
       lambda c:c["search"]["conditional_rules"][0].update(require={}))
change("empty_when_axis",
       lambda c:c["search"]["conditional_rules"][0]["when"].update(axis=""))
change("integer_when_axis",
       lambda c:c["search"]["conditional_rules"][0]["when"].update(axis=100))
change("missing_when_values",
       lambda c:c["search"]["conditional_rules"][0]["when"].pop("values"))
change("missing_required_values",
       lambda c:c["search"]["conditional_rules"][0]["require"].pop("values"))
change("when_values_wrong_type",
       lambda c:c["search"]["conditional_rules"][0]["when"].update(values="false"))
change("require_values_wrong_type",
       lambda c:c["search"]["conditional_rules"][0]["require"].update(values={}))
change("when_nested_object_label",
       lambda c:c["search"]["conditional_rules"][0]["when"].update(values=[{"fake":True}]))
change("require_nested_array_label",
       lambda c:c["search"]["conditional_rules"][0]["require"].update(values=[[0]]))
change("when_null_label",
       lambda c:c["search"]["conditional_rules"][0]["when"].update(values=[None]))
change("require_unknown_axis",
       lambda c:c["search"]["conditional_rules"][0]["require"].update(axis="missing"))
change("require_unknown_label",
       lambda c:c["search"]["conditional_rules"][0]["require"].update(values=["never"]))
change("top_level_conditional_unknown",
       lambda c:c["search"].update(unknown_conditional_flag=True))
change("single_non_object_rule",
       lambda c:c["search"].update(conditional_rules=[False]))
change("single_missing_require",
       lambda c:c["search"].update(conditional_rules=[{"when":{"axis":"x","values":[0]}}]))
change("single_missing_when",
       lambda c:c["search"].update(conditional_rules=[{"require":{"axis":"x","values":[0]}}]))
change("wrong_conditional_value_boolean",
       lambda c:c["search"].update(conditional_rules=True))
with tempfile.TemporaryDirectory(prefix="phaseshift-conditional-contract-") as tmp:
    for label,cfg in cases_bad:
        path=pathlib.Path(tmp)/(label+".json")
        path.write_text(json.dumps(cfg, sort_keys=True)+"\n")
        result=subprocess.run([args.binary,"validate","--config",str(path)],
                              capture_output=True,text=True,timeout=20)
        assert result.returncode != 0, (label,result.stdout,result.stderr)
print("PASS exact conditional rule replay, 12 feasible cases and",
      len(cases_bad),"invalid-config rejections")
