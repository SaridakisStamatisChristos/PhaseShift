#!/usr/bin/env python3
"""Full CLI smoke for opt-in cliff bracketing, no fabricated SOTA claim."""
import argparse
import json
import pathlib
import shutil
import sqlite3
import subprocess

def run(binary, root, name, negative, persistent_detector=False):
    cfg = {
        "schema_version":1, "experiment_id":"cliff-bracket-"+name,
        "problem_family":"calibration","algorithms":["calibration"],
        "seed":137, "primary_axis":"x",
        "axes":{"x":{"values":list(range(64))}},
        "fixed":{"threshold":24,"negative":negative},
        "search":{"method":"cliff_bracket_v2","policy_version":"v2",
                  "max_cases":8,"exploration_fraction":0.0},
        "measurement":{"paired_discovery_blocks":1,"paired_confirm_blocks":20,
                       "warmup_iterations":0},
        "limits":{"total_wall_seconds":90,"max_search_queue":80},
    }
    if persistent_detector:
        cfg["detection"]={"detector_version":"multi_change_jump_v2",
                          "min_practical_ratio":1.2}
    manifest=root/(name+".json")
    manifest.write_text(json.dumps(cfg,sort_keys=True)+"\n")
    folder=root/name
    for command in ([str(binary),"validate","--config",str(manifest)],
                    [str(binary),"explore","--config",str(manifest),"--out",str(folder)]):
        result=subprocess.run(command,capture_output=True,text=True,timeout=60,check=False)
        if result.returncode:
            raise AssertionError(f"{command} exited {result.returncode}: {result.stderr}")
    with sqlite3.connect(folder/"run.sqlite") as db:
        decisions=db.execute("SELECT pool_index FROM scheduler_decisions ORDER BY seq").fetchall()
        hypothesis=db.execute("SELECT status,left_value,right_value FROM hypotheses WHERE id='H1'").fetchone()
    assert len(decisions)==8,decisions
    assert len({x[0] for x in decisions})==8,decisions
    assert decisions[:2]==[(0,),(63,)],decisions
    if negative:
        assert hypothesis is None or hypothesis[0]!="HYPOTHESIZED",hypothesis
    else:
        assert hypothesis and hypothesis[0]=="HYPOTHESIZED",hypothesis
        assert hypothesis[1]==23 and hypothesis[2]==24,hypothesis
    return [x[0] for x in decisions]

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--binary",required=True,type=pathlib.Path)
    p.add_argument("--out",required=True,type=pathlib.Path)
    a=p.parse_args()
    shutil.rmtree(a.out,ignore_errors=True)
    a.out.mkdir(parents=True)
    first=run(a.binary.resolve(),a.out,"threshold",False)
    repeated=run(a.binary.resolve(),a.out,"threshold_repeat",False)
    other=run(a.binary.resolve(),a.out,"negative",True)
    # Exercise the complete opt-in detector through real workers and SQLite,
    # not merely its numerical unit helper. Detection versions do not influence
    # the search proposal order, but they can change the final hypothesis.
    guarded=run(a.binary.resolve(),a.out,"guarded_threshold",False,True)
    guarded_null=run(a.binary.resolve(),a.out,"guarded_negative",True,True)
    assert guarded==first,(guarded,first)
    assert len(guarded_null)==8
    # An adaptive policy MUST be permitted to change its decision sequence
    # when the observed performance signal changes. Determinism compares
    # equal experiments, not a planted cliff with an independent null.
    assert first==repeated,(first,repeated)
    assert len(other)==8 and len(set(other))==8,other
    print("PASS cliff bracketing and persistent-jump detector: threshold 23..24, both null controls, same-signal replay")

if __name__=="__main__":
    main()
