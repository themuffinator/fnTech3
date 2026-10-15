#!/usr/bin/env python3
"""Collect the shader warnings in a map sweep's console logs, grouped by kind.

Reads the `logs/*-console.log` files that `probe_retail_map_loads.py` keeps
(run it with `--developer` so the developer-only lines are there), or any
console logs named on the command line, and prints every distinct shader
warning with the maps that printed it:

  keyword     a keyword or value the parser does not know
  image       an image a shader names that could not be loaded
  default     a shader with no script whose image could not be found either,
              drawn with the default shader
  malformed   a script the parser could not read as written
  lightmap    a lightmap/stage mismatch FinishShader reports
  other       any other warning that names a shader

`--json PATH` also writes the same grouping as JSON. Only reads its inputs.
"""
from pathlib import Path
import argparse
import collections
import json
import re
import sys

COLOR = re.compile(r'\^[0-9a-zA-Z]')
KINDS = (
    ('keyword', re.compile(
        r"unknown general shader parameter|unknown parameter '|unknown tcMod|unknown rgbGen parameter|"
        r"unknown alphaGen parameter|unknown texgen parm|unknown deformVertexes subtype|unknown blend mode|"
        r"invalid alphaFunc name|invalid genfunc name|unknown depthfunc|invalid if argument|"
        r"invalid alphatest function|invalid cull parm|flap requires", re.I)),
    ('image', re.compile(
        r"R_FindImageFile could not find|for 'videoMap' keyword|q3map_lightImage '.*' could not be loaded", re.I)),
    ('default', re.compile(r'Shader image unavailable for:', re.I)),
    ('lightmap', re.compile(r'forced lightmap|has lightmap but no lightmap stage', re.I)),
    ('malformed', re.compile(
        r"missing .*(?:parm|parameter|element|parenthesis|argument)|no concluding '}'|expecting '\{'|"
        r"Ignoring shader file|Ignoring entire file|missing opening brace|too many stages|too many nextBundle|"
        r"nextBundle without an image|has stages|no matching endif|endif without if|out of range in shader|"
        r"illegal div value|ignoring excess images", re.I)),
)
# A warning about a shader names it in one of these ways.
SHADER_LINE = re.compile(r"shader|R_FindImageFile|Shader image unavailable", re.I)
WARNING = re.compile(r'WARNING|unavailable', re.I)


def classify(line):
    for kind, pattern in KINDS:
        if pattern.search(line):
            return kind
    return 'other' if WARNING.search(line) and SHADER_LINE.search(line) else None


def map_name(path):
    """`mohaa_DM_mohdm1-console.log` -> `mohaa_DM_mohdm1`."""
    name = path.name
    return name[:-len('-console.log')] if name.endswith('-console.log') else path.stem


def scan(paths):
    found = collections.defaultdict(lambda: collections.defaultdict(set))
    for path in paths:
        text = path.read_text(encoding='latin-1')
        for raw in text.splitlines():
            line = COLOR.sub('', raw).strip()
            kind = classify(line)
            if kind:
                found[kind][line].add(map_name(path))
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('inputs', nargs='+', type=Path, help='sweep output folders or console logs')
    parser.add_argument('--json', type=Path)
    args = parser.parse_args()
    paths = []
    for item in args.inputs:
        if item.is_dir():
            logs = item / 'logs' if (item / 'logs').is_dir() else item
            paths += sorted(logs.glob('*-console.log'))
        elif item.is_file():
            paths.append(item)
        else:
            parser.error(f'no such log or folder: {item}')
    if not paths:
        parser.error('no console logs found')
    found = scan(paths)
    report = {kind: [dict(line=line, maps=sorted(maps)) for line, maps in
                     sorted(lines.items(), key=lambda item: (-len(item[1]), item[0]))]
              for kind, lines in found.items()}
    print(f'{len(paths)} logs')
    for kind in [k for k, _ in KINDS] + ['other']:
        rows = report.get(kind, [])
        print(f'{kind}: {len(rows)} distinct')
        for row in rows:
            maps = row['maps']
            print(f'  [{len(maps):3}] {row["line"]}' + (f'  ({maps[0]})' if len(maps) == 1 else ''))
    if args.json:
        args.json.write_text(json.dumps(dict(logs=len(paths), kinds=report), indent=1) + '\n', encoding='utf-8')
    return 0


if __name__ == '__main__':
    sys.exit(main())
