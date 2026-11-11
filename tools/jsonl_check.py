#!/usr/bin/env python3
#
# Checks structured output of the jsonoutput module (JSON Lines) against
# the standard text output (.out file) of the same analysis:
#  - every line of the JSON Lines file must be a valid JSON object with a "kind" member,
#  - the file must start with a "header" record and end with a "footer" record,
#  - DOF values of dof managers reported in the JSON file must match the values
#    in the "Node output" sections of the text output (for each reported step).
#
# Usage: jsonl_check.py output.jsonl [output.out] [--tol 1e-6] [--validate-only]
#
# With --validate-only (or without the text output file), only the validity of the
# JSON Lines file is checked (e.g. for the output of the json export module).
#
import argparse
import json
import re
import sys


def read_jsonl(name):
    records = []
    with open(name) as f:
        for i, line in enumerate(f, 1):
            try:
                rec = json.loads(line)
            except json.JSONDecodeError as e:
                sys.exit(f'{name}:{i}: invalid JSON: {e}')
            if not isinstance(rec, dict) or 'kind' not in rec:
                sys.exit(f'{name}:{i}: record without "kind"')
            records.append(rec)
    if not records or records[0]['kind'] != 'header' or records[-1]['kind'] != 'footer':
        sys.exit(f'{name}: missing header or footer record')
    return records


def read_out(name):
    """Returns list (one item per output step) of dicts {(dofman number, dof id): {unknown: value}}."""
    steps = []
    section = None
    dman = None
    node_re = re.compile(r'^\S+\s+(\d+)\s+\(\s*(\d+)\):')
    dof_re = re.compile(r'^\s+dof\s+(\d+)\s+(.*)$')
    with open(name) as f:
        for line in f:
            if line.startswith('Output for time'):
                steps.append({})
                section = None
            elif line.startswith('Node output:') or line.startswith('DofManager output:'):
                section = 'nodes'
            elif line.startswith('Element output:'):
                section = None
            elif section == 'nodes' and steps:
                m = node_re.match(line)
                if m:
                    dman = int(m.group(2))
                    continue
                m = dof_re.match(line)
                if m and dman is not None:
                    tokens = m.group(2).split()
                    steps[-1][(dman, int(m.group(1)))] = {tokens[i]: float(tokens[i + 1]) for i in range(0, len(tokens) - 1, 2)}
    return steps


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('jsonl')
    parser.add_argument('out', nargs='?')
    parser.add_argument('--tol', type=float, default=1.e-6)
    parser.add_argument('--validate-only', action='store_true')
    args = parser.parse_args()

    records = read_jsonl(args.jsonl)
    if args.validate_only or not args.out:
        nsteps = sum(1 for r in records if r['kind'] == 'step')
        if nsteps == 0:
            sys.exit('no step records')
        print(f'OK: {len(records)} valid records, {nsteps} steps')
        return
    text_steps = read_out(args.out)

    json_steps = []
    for rec in records:
        if rec['kind'] == 'step':
            json_steps.append({})
        elif rec['kind'] == 'dofman':
            for dof in rec.get('dofs', []):
                values = {k: v for k, v in dof.items() if k not in ('id', 'name')}
                json_steps[-1][(rec['number'], dof['id'])] = values

    if len(json_steps) != len(text_steps):
        sys.exit(f'number of steps differs: json {len(json_steps)}, text {len(text_steps)}')

    checked = 0
    for istep, (js, ts) in enumerate(zip(json_steps, text_steps), 1):
        for key, tvals in ts.items():
            if key not in js:
                sys.exit(f'step {istep}: dofman {key[0]} dof {key[1]} missing in json output')
            for unknown, tval in tvals.items():
                jval = js[key].get(unknown)
                if jval is None or abs(jval - tval) > args.tol * max(1., abs(tval)):
                    sys.exit(f'step {istep}: dofman {key[0]} dof {key[1]} {unknown}: json {jval}, text {tval}')
                checked += 1
    if checked == 0:
        sys.exit('no values compared')
    print(f'OK: {len(records)} records, {len(json_steps)} steps, {checked} values compared')


if __name__ == '__main__':
    main()
