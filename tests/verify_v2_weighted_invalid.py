#!/usr/bin/env python3
"""Fail-closed CLI contract tests for v2 weighted configuration."""
import argparse
import copy
import json
import pathlib
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument("--binary", required=True)
p.add_argument("--config", required=True)
a = p.parse_args()
original = json.loads(pathlib.Path(a.config).read_text())
cases = []

def mutation(name, transform):
    cfg = copy.deepcopy(original)
    transform(cfg)
    cases.append((name, cfg))

mutation("missing_prior", lambda c: c["search"].pop("domain_priors"))
mutation("prior_list", lambda c: c["search"].update(domain_priors=[]))
mutation("prior_empty", lambda c: c["search"].update(domain_priors={}))
mutation("axis_empty", lambda c: c["search"]["domain_priors"].update(negative={}))
mutation("boolean_weight", lambda c: c["search"]["domain_priors"]["negative"].update({"true": True}))
mutation("string_weight", lambda c: c["search"]["domain_priors"]["negative"].update({"true": "9"}))
mutation("zero_weight", lambda c: c["search"]["domain_priors"]["negative"].update({"true": 0}))
mutation("negative_weight", lambda c: c["search"]["domain_priors"]["negative"].update({"true": -1}))
mutation("too_small", lambda c: c["search"]["domain_priors"]["negative"].update({"true": 1e-12}))
mutation("too_large", lambda c: c["search"]["domain_priors"]["negative"].update({"true": 1e12}))
mutation("unknown_axis", lambda c: c["search"]["domain_priors"].update(unknown={"true": 3.}))
mutation("unknown_label", lambda c: c["search"]["domain_priors"]["negative"].update(unknown=3.))
mutation("prior_other_policy", lambda c: c["search"].update(method="uniform_v2"))
mutation("legacy_policy", lambda c: c["search"].update(method="grid", policy_version="v1"))
mutation("legacy_default", lambda c: c["search"].pop("policy_version"))
mutation("zero_replicates", lambda c: c["search"].update(replicates=0))
mutation("nonobject_search_prior", lambda c: c["search"].update(domain_priors="bad"))
with tempfile.TemporaryDirectory(prefix="phaseshift-v2-priors-") as directory:
    for name, config in cases:
        path = pathlib.Path(directory) / (name + ".json")
        path.write_text(json.dumps(config, sort_keys=True) + "\n")
        p = subprocess.run([a.binary, "validate", "--config", str(path)],
                           capture_output=True, text=True, timeout=20)
        assert p.returncode != 0, (name, p.stdout, p.stderr)
print("PASS", len(cases), "weighted prior malformed/legacy contract rejections")
