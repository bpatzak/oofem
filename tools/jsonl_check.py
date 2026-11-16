#!/usr/bin/env python3
#
# Checks structured output of the json output/export modules (JSON Lines) against
# the standard text output (.out file) of the same analysis:
#  - every line of the JSON Lines file must be a valid JSON object with a "kind" member,
#  - the file must start with a "header" line and end with a "footer" line,
#  - every "step" line must be nested as step -> domains[] -> sections (dofmans[], elements[],
#    reactions[]) -> records, elements -> integration_rules[] -> gps[],
#  - DOF values of dof managers and reaction forces reported in the JSON file must match
#    the values in the "Node output" and "R E A C T I O N S" sections of the text output
#    (for each reported step).
#
# Usage: jsonl_check.py output.jsonl [output.out] [--tol 1e-6] [--subset] [--validate-only]
#
# By default, every DOF and reaction of the text output must be present in the JSON file
# (output of the jsonoutput module). With --subset, only the values present in the JSON file
# are compared (output of the json export module, reporting selected quantities).
# With --validate-only (or without the text output file), only the validity and the structure
# of the JSON Lines file are checked.
#
import argparse
import json
import re
import sys


def check_structure(name, lineno, step):
    """Checks the nesting of a step line, exits on error."""
    def fail(msg):
        sys.exit(f'{name}:{lineno}: {msg}')
    for key in ('step', 'time', 'metastep', 'solution_time'):
        if key not in step:
            fail(f'step without "{key}"')
    for domain in step.get('domains', []):
        if not isinstance(domain.get('number'), int):
            fail('domain without number')
        for section in ('dofmans', 'elements', 'reactions'):
            records = domain.get(section, [])
            if not isinstance(records, list):
                fail(f'section {section} is not a list')
            for rec in records:
                if not isinstance(rec, dict) or 'number' not in rec:
                    fail(f'record of section {section} without number')
        for elem in domain.get('elements', []):
            for ir in elem.get('integration_rules', []):
                if 'number' not in ir or not isinstance(ir.get('gps'), list):
                    fail(f'element {elem["number"]}: integration rule without number or gps')
                for gp in ir['gps']:
                    if 'number' not in gp:
                        fail(f'element {elem["number"]}: integration point without number')


def read_jsonl(name):
    records = []
    with open(name) as f:
        for i, line in enumerate(f, 1):
            try:
                rec = json.loads(line)
            except json.JSONDecodeError as e:
                sys.exit(f'{name}:{i}: invalid JSON: {e}')
            if not isinstance(rec, dict) or 'kind' not in rec:
                sys.exit(f'{name}:{i}: line without "kind"')
            if rec['kind'] == 'step':
                check_structure(name, i, rec)
            records.append(rec)
    if not records or records[0]['kind'] != 'header' or records[-1]['kind'] != 'footer':
        sys.exit(f'{name}: missing header or footer line')
    return records


def read_out(name):
    """Returns list (one item per output step) of tuples (dofs, reactions), where
    dofs is a dict {(dofman number, dof id): {unknown: value}} and
    reactions is a dict {(dofman label, dof id): value}."""
    steps = []
    section = None
    dman = None
    node_re = re.compile(r'^\S+\s+(\d+)\s+\(\s*(\d+)\):')
    dof_re = re.compile(r'^\s+dof\s+(\d+)\s+(.*)$')
    reaction_re = re.compile(r'^\s+Node\s+(\d+)\s+iDof\s+(\d+)\s+reaction\s+(\S+)')
    with open(name) as f:
        for line in f:
            if line.startswith('Output for time'):
                steps.append(({}, {}))
                section = None
            elif line.startswith('Node output:') or line.startswith('DofManager output:'):
                section = 'nodes'
            elif line.startswith('Element output:'):
                section = None
            elif 'R E A C T I O N S' in line:
                section = 'reactions'
            elif section == 'nodes' and steps:
                m = node_re.match(line)
                if m:
                    dman = int(m.group(2))
                    continue
                m = dof_re.match(line)
                if m and dman is not None:
                    tokens = m.group(2).split()
                    steps[-1][0][(dman, int(m.group(1)))] = {tokens[i]: float(tokens[i + 1]) for i in range(0, len(tokens) - 1, 2)}
            elif section == 'reactions' and steps:
                m = reaction_re.match(line)
                if m:
                    steps[-1][1][(int(m.group(1)), int(m.group(2)))] = float(m.group(3))
    return steps


def json_step_values(step):
    """Returns tuple (dofs, reactions) of a step line, keyed as in read_out."""
    dofs, reactions = {}, {}
    for domain in step.get('domains', []):
        for rec in domain.get('dofmans', []):
            for dof in rec.get('dofs', []):
                dofs[(rec['number'], dof['id'])] = {k: v for k, v in dof.items() if k not in ('id', 'name')}
        for rec in domain.get('reactions', []):
            for dof in rec['dofs']:
                # the text output identifies the node by its label
                reactions[(rec.get('label', rec['number']), dof['id'])] = dof['value']
    return dofs, reactions


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('jsonl')
    parser.add_argument('out', nargs='?')
    parser.add_argument('--tol', type=float, default=1.e-6)
    parser.add_argument('--subset', action='store_true', help='compare only values present in the JSON file')
    parser.add_argument('--validate-only', action='store_true')
    args = parser.parse_args()

    records = read_jsonl(args.jsonl)
    json_steps = [json_step_values(r) for r in records if r['kind'] == 'step']
    if args.validate_only or not args.out:
        if not json_steps:
            sys.exit('no step lines')
        print(f'OK: {len(records)} valid lines, {len(json_steps)} steps')
        return
    text_steps = read_out(args.out)

    if len(json_steps) != len(text_steps):
        sys.exit(f'number of steps differs: json {len(json_steps)}, text {len(text_steps)}')

    checked = 0
    for istep, ((jdofs, jreact), (tdofs, treact)) in enumerate(zip(json_steps, text_steps), 1):
        # DOF values (the text output has 8 decimal digits, compared with tolerance)
        keys = jdofs.keys() if args.subset else tdofs.keys()
        for key in keys:
            if key not in jdofs or key not in tdofs:
                where = 'json' if key not in jdofs else 'text'
                sys.exit(f'step {istep}: dofman {key[0]} dof {key[1]} missing in {where} output')
            for unknown, tval in tdofs[key].items():
                if args.subset and unknown not in jdofs[key]:
                    continue
                jval = jdofs[key].get(unknown)
                if jval is None or abs(jval - tval) > args.tol * max(1., abs(tval)):
                    sys.exit(f'step {istep}: dofman {key[0]} dof {key[1]} {unknown}: json {jval}, text {tval}')
                checked += 1
        # reactions: the text output has 5 significant digits, the exact json value
        # must give the same number when formatted in the same way
        keys = jreact.keys() if args.subset else treact.keys()
        for key in keys:
            if key not in jreact or key not in treact:
                where = 'json' if key not in jreact else 'text'
                sys.exit(f'step {istep}: reaction of node {key[0]} dof {key[1]} missing in {where} output')
            if float(f'{jreact[key]:.4e}') != treact[key]:
                sys.exit(f'step {istep}: reaction of node {key[0]} dof {key[1]}: json {jreact[key]}, text {treact[key]}')
            checked += 1
    if checked == 0:
        sys.exit('no values compared')
    print(f'OK: {len(records)} lines, {len(json_steps)} steps, {checked} values compared')


if __name__ == '__main__':
    main()
