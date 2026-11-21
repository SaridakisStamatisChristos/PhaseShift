#!/usr/bin/env python3
"""Independent SQLite replay oracle for v2 weighted priors with admissible rules."""
import argparse
import json
import sqlite3

parser = argparse.ArgumentParser()
parser.add_argument("--resumed", required=True)
parser.add_argument("--uninterrupted", required=True)
args = parser.parse_args()


def read(path):
    with sqlite3.connect(path + "/run.sqlite") as db:
        conf = json.loads(db.execute("SELECT config FROM experiments WHERE id=1").fetchone()[0])
        decisions = db.execute(
            "SELECT seq,pool_index,case_id,rng_state "
            "FROM scheduler_decisions ORDER BY seq"
        ).fetchall()
        events = db.execute(
            "SELECT detail FROM events WHERE event='v2_acquisition' ORDER BY id"
        ).fetchall()
        cases = db.execute(
            "SELECT c.case_id,c.params,c.seed FROM cases c JOIN samples s "
            "ON c.case_id=s.case_id WHERE s.holdout=0 GROUP BY c.case_id ORDER BY c.case_id"
        ).fetchall()
        return conf, decisions, events, cases

first, second = read(args.resumed), read(args.uninterrupted)
assert first == second, "weighted scheduler resume differs from uninterrupted transcript"
conf, decisions, events, cases = first
assert conf["policy_version"] == "v2" and conf["search_method"] == "weighted_v2"
assert conf["search"]["domain_priors"]["negative"]["false"] == 9.0
assert len(decisions) == 12 == len(cases) == len(events), (len(decisions), len(cases), len(events))
assert len({d[1] for d in decisions}) == 12
assert all("origin=weighted_domain_prior" in e[0] for e in events)
for _id, params, seed in cases:
    j = json.loads(params)
    x, negative, threshold = int(j["x"]), bool(j["negative"]), int(j["threshold"])
    assert (not negative and x in (0, 1)) or (
        negative and x in (2, 3) and threshold == 3
    ), j
    assert seed in (334, 335)
for run in (args.resumed, args.uninterrupted):
    with sqlite3.connect(run + "/run.sqlite") as db:
        phases = [json.loads(x[0]) for x in db.execute(
            "SELECT detail FROM events WHERE event='v2_phase_budget' ORDER BY id"
        )]
        decisions = [x[0] for x in db.execute(
            "SELECT pool_index FROM scheduler_decisions ORDER BY seq"
        )]
    assert len(phases) == len(decisions) == 12
    assert [p["virtual_index"] for p in phases] == decisions
    for event in phases:
        assert event["accounting_model"] == "descriptive_v2_case_boundary_v2"
        assert event["cpu_accounting_scope"] in (
            "linux_getrusage_per_case_proposal_and_worker", "unavailable")
        if event["cpu_accounting_scope"] != "unavailable":
            assert event["parent_process_cpu_ns"] >= 0
            assert event["waited_child_cpu_ns"] >= 0
        assert event["proposal_wall_ns"] >= 0 and event["worker_invocation_wall_ns"] >= 0
        assert event["valid_timed_cpu_ns"] >= 0
        assert isinstance(event["timed_cpu_complete"], bool)
print("PASS weighted admissible space exactly-once replay and independent SQLite evidence")
