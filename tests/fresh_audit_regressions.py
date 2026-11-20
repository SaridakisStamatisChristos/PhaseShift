#!/usr/bin/env python3
"""Independent v1.0 audit regressions. All inputs/results live in temp dirs."""
import argparse
import collections
import csv
import json
import pathlib
import sqlite3
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--binary', required=True, type=pathlib.Path)
parser.add_argument('--repo', required=True, type=pathlib.Path)
args=parser.parse_args()
binary=args.binary.resolve(); repo=args.repo.resolve()

def run(*arguments, accepted=0, timeout=120):
    x=subprocess.run([str(binary), *map(str,arguments)],capture_output=True,text=True,timeout=timeout)
    if accepted is not None and x.returncode!=accepted:
        raise AssertionError(f'command {arguments!r}, exit={x.returncode}: {x.stdout[-800:]} {x.stderr[-800:]}')
    return x

def read_db(path, sql):
    with sqlite3.connect(path/'run.sqlite') as connection:
        return connection.execute(sql).fetchall()

with tempfile.TemporaryDirectory(prefix='phaseshift-fresh-regressions-') as directory:
    root=pathlib.Path(directory)
    # Exact explicit configuration semantics: never substitute another axis.
    bad=json.loads((repo/'configs/demo_transition.json').read_text())
    bad['primary_axis']='axis_that_is_not_present'
    bfile=root/'bad_axis.json';bfile.write_text(json.dumps(bad))
    x=run('validate','--config',bfile,accepted=2)
    assert 'primary_axis' in x.stderr and 'not declared' in x.stderr
    assert not (root/'bad_run').exists()
    # Inference remains valid when and only when primary_axis is omitted.
    good=dict(bad);good.pop('primary_axis');good['search']={'method':'grid','max_cases':3}
    good['measurement']={'paired_discovery_blocks':1,'paired_confirm_blocks':20,'warmup_iterations':0}
    gfile=root/'good_axis.json';gfile.write_text(json.dumps(good))
    run('validate','--config',gfile)
    run('explore','--config',gfile,'--out',root/'good_run')
    actual=json.loads(read_db(root/'good_run','SELECT config FROM experiments')[0][0])
    assert actual['primary_axis']=='x'
    hyp=json.loads((root/'good_run'/'report.json').read_text())['summary']['hypotheses'][0]
    assert hyp['status']=='NO_CONFIRMED_BOUNDARY'
    assert hyp['confirmation_evidence_attested'] is False
    assert hyp['confirmation_evidence_applicable'] is False
    # Unsupported formats must reject; selected formats must be respected.
    run('report','--run',root/'good_run','--format','unicorn',accepted=2)
    (root/'good_run'/'report.md').unlink()
    run('report','--run',root/'good_run','--format','json')
    assert not (root/'good_run'/'report.md').exists()
    run('report','--run',root/'good_run','--format','markdown,json,csv,html')
    assert (root/'good_run'/'report.md').exists()
    # Unique-case resolution: ambiguous prefix must be rejected.
    many=dict(good)
    many['axes']={'x':{'values':list(range(17))}}
    many['search']={'method':'grid','max_cases':17}
    mfile=root/'many.json';mfile.write_text(json.dumps(many))
    run('explore','--config',mfile,'--out',root/'many')
    case_ids=[x[0] for x in read_db(root/'many','SELECT case_id FROM cases')]
    counters=collections.Counter(x[0] for x in case_ids)
    prefix=next(k for k,v in counters.items() if v>1)
    out=run('replay','--run',root/'many','--case',prefix,accepted=2)
    assert 'ambiguous' in out.stderr
    run('replay','--run',root/'many','--case',case_ids[0])

    # Poison one block in every case: reject all invalid cases and all fits.
    plugin=root/'native_algorithm.so'
    def compile_plugin(poison_call):
        subprocess.run(['cc','-std=c11','-D_DEFAULT_SOURCE','-O2','-Wall','-Werror',
                        '-shared','-fPIC',f'-DPOISON_CALL={poison_call}',
                        '-I'+str(repo/'include'),str(repo/'tests/poisoned_scaling_plugin.c'),
                        '-o',str(plugin)],check=True)
    config=json.loads((repo/'configs/sorting_quicksort.json').read_text())
    name='plugin:'+str(plugin)
    config.update({'algorithms':[name],'axes':{'n':{'values':[64]}},'primary_axis':'n',
                   'fixed':{'distribution':'ascending'},'seed':137})
    config['search']={'method':'grid','max_cases':1}
    config['measurement']={'paired_discovery_blocks':3,'paired_confirm_blocks':20,
                           'warmup_iterations':1,'minimum_batch_ms':1}
    config['limits']={'total_wall_seconds':150,'max_case_bytes':1048576,
                      'max_worker_rss_mb':1024,'max_sample_wall_ms':3000}
    conf=root/'plugin.json';conf.write_text(json.dumps(config))
    compile_plugin(5)
    first=root/'first';run('explore','--config',conf,'--out',first)
    scaling=root/'scaling';run('scaling','--run',first,'--out',scaling,'--seeds','3')
    statuses=dict(read_db(scaling,'SELECT status,COUNT(*) FROM samples GROUP BY status'))
    assert statuses.get('wrong_answer')==24, statuses
    with (scaling/'scaling_samples.csv').open(newline='') as stream:
        rows=list(csv.DictReader(stream))
    assert len(rows)==24 and all(row['status']=='invalid' for row in rows),collections.Counter(r['status'] for r in rows)
    model=json.loads((scaling/'scaling.json').read_text())['algorithms'][name]
    assert model['status']=='INSUFFICIENT_VALID_SIZES' and model['rejected_discovery_cases']==24,model

    # Recompile a *correct* implementation at the same path, then prove that
    # historic runs reject it while new runs can replay through worker isolation.
    compile_plugin(999999)
    second=root/'second';run('explore','--config',conf,'--out',second)
    version_a=read_db(first,'SELECT version FROM algorithms')[0][0]
    version_b=read_db(second,'SELECT version FROM algorithms')[0][0]
    assert version_a!=version_b
    comparison=run('compare','--a',first,'--b',second,accepted=2)
    assert 'ARTIFACT_MISMATCH' in comparison.stderr
    case_a=read_db(first,'SELECT case_id FROM cases')[0][0]
    case_b=read_db(second,'SELECT case_id FROM cases')[0][0]
    mismatched=run('replay','--run',first,'--case',case_a,accepted=2)
    assert 'ARTIFACT_MISMATCH' in mismatched.stderr
    assert 'REPLAY OK' in run('replay','--run',second,'--case',case_b).stdout
    assert 'RECORDED_ARTIFACTS_MATCH' in run('compare','--a',second,'--b',second).stdout
    relocated=plugin.with_suffix('.held')
    plugin.rename(relocated)
    missing=run('replay','--run',second,'--case',case_b,accepted=2)
    assert 'UNAVAILABLE_PLUGIN' in missing.stderr
    relocated.rename(plugin)
    # A bad plugin's wrong answer is distinguished from a missing artifact.
    compile_plugin(1)
    wrong_run=root/'wrong_answer_replay';run('explore','--config',conf,'--out',wrong_run)
    bad_case=read_db(wrong_run,'SELECT case_id FROM cases')[0][0]
    bad_replay=run('replay','--run',wrong_run,'--case',bad_case,accepted=2)
    assert 'WRONG_ANSWER' in bad_replay.stderr,bad_replay.stderr

    # Unordered contrast cannot be minimized as a fictitious numeric
    # interval. Persist exact paired fresh evidence and honest shrink status.
    cat={ 'schema_version':1,'experiment_id':'categorical-metadata-reproducer',
          'problem_family':'calibration','algorithms':['calibration'],
          'primary_axis':'negative','seed':137,'axes':{'negative':[False,True]},
          'fixed':{'x':0,'threshold':8},'search':{'method':'grid','max_cases':2},
          'measurement':{'paired_discovery_blocks':1,'paired_confirm_blocks':20,
                         'warmup_iterations':0,'minimum_batch_ms':1},
          'limits':{'total_wall_seconds':60,'max_sample_wall_ms':100}}
    cat_config=root/'categorical.json';cat_config.write_text(json.dumps(cat))
    categorical=root/'categorical'
    run('explore','--config',cat_config,'--out',categorical)
    run('confirm','--run',categorical)
    prior=read_db(categorical,"SELECT status FROM hypotheses WHERE id='H1'")[0][0]
    assert prior=='CONFIRMED',prior
    run('minimize','--run',categorical,'--hypothesis','H1')
    w=json.loads((categorical/'witness.json').read_text())
    assert w['category']=='CATEGORICAL_PAIR_WITNESS' and w['left_right_are_unordered']
    assert w['baseline_revalidated'] and w['final_revalidated']
    assert w['fresh_seed_pairs_per_evaluation']==20 and w['status'] in ('MINIMIZATION_FAILED','MINIMIZED_CONFIRMED')
    with sqlite3.connect(categorical/'minimization.sqlite') as conn:
        assert conn.execute('SELECT COUNT(*) FROM samples').fetchone()[0]>=80

    # Wrong-answer contrast: pair-preserving shrink must retain both distinct
    # statuses instead of accidentally shrinking one side into the other.
    failure_plugin=root/'categorical_fault.so'
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Werror','-shared','-fPIC',
                    '-DAUDIT_MODE=3','-I'+str(repo/'include'),
                    str(repo/'tests/audit_boundary_plugin.c'),'-o',str(failure_plugin)],check=True)
    failure_cfg={
        'schema_version':1,'experiment_id':'categorical-wrong-answer',
        'problem_family':'integer_array','algorithms':['plugin:'+str(failure_plugin)],
        'primary_axis':'distribution','seed':137,
        'axes':{'distribution':['uniform','ascending']},'fixed':{'n':128},
        'search':{'method':'grid','max_cases':2},
        'measurement':{'paired_discovery_blocks':1,'paired_confirm_blocks':20,
                       'warmup_iterations':0,'minimum_batch_ms':1},
        'limits':{'total_wall_seconds':60,'max_sample_wall_ms':1500}}
    failure_file=root/'categorical_failure.json';failure_file.write_text(json.dumps(failure_cfg))
    failure_run=root/'failure_category'
    run('explore','--config',failure_file,'--out',failure_run)
    run('confirm','--run',failure_run)
    assert read_db(failure_run,"SELECT status FROM hypotheses WHERE id='H1'")[0][0]=='CONFIRMED'
    run('minimize','--run',failure_run,'--hypothesis','H1')
    failure_witness=json.loads((failure_run/'witness.json').read_text())
    assert failure_witness['category']=='CATEGORICAL_PAIR_WITNESS'
    assert failure_witness['expected_failure_status']=='wrong_answer'
    assert failure_witness['final_revalidated']
    print('PASS: all fresh-audit reproducibility, config, CLI, scaling, categorical witness and metadata regressions')
