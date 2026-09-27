#!/usr/bin/env python3
"""Measure every index an installed map gives from one lump into another.

Each map the installations carry (scripts/probe_retail_map_loads.py finds them)
is presented as IBSP 46 through the engine's own translators, as the loaders
present it: code/qcommon/bsp_v43.h, bsp_rbsp.h, bsp_fakk.h, bsp_mohaa.h and
bsp_q3test.h are compiled into a small shim with Clang, and a FAKK map is
translated both as the collision loader asks and as the renderers do. Every
index the collision loader and the renderers' world loaders follow is then
measured against the table it names: the checks of
code/qcommon/bsp_index_checks.h, and a few more the loaders deliberately leave
alone, which show why (`informational`).

The node tree is measured as every descent walks it, from node 0: how deep it
goes, whether any node or leaf is reached twice or not at all, and whether a
child leads back above itself (`tree`); and the loaders' own walk,
BSP_NodeTreeFault, compiled into the shim, runs over it (`node.tree`).

Nothing is run and nothing is written to an installation. The report is JSON:
per map, the record counts, the tree's shape and every check a record fails,
with examples.

  --installation KEY=PATH  as for probe_retail_map_loads.py
"""
from collections import Counter, defaultdict
from pathlib import Path
import argparse
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
sys.path.insert(0, str(ROOT / 'tests'))
from compiler_support import compiler_target_args  # noqa: E402
from probe_retail_map_loads import TITLES, discover  # noqa: E402

SHIM = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qfiles.h"
#include "code/qcommon/bsp_v43.h"
#include "code/qcommon/bsp_rbsp.h"
#include "code/qcommon/bsp_fakk.h"
#include "code/qcommon/bsp_mohaa.h"
#include "code/qcommon/bsp_q3test.h"
#include "code/qcommon/bsp_index_checks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void Q_strncpyz( char *dest, const char *src, int destsize ) {
	if ( destsize <= 0 ) return;
	strncpy( dest, src, destsize - 1 );
	dest[destsize - 1] = 0;
}
static void *Alloc( int size ) { return calloc( 1, size > 0 ? (size_t)size : 1 ); }
static void Release( void *p ) { free( p ); }
/* shim tree <in>: the loaders' walk of an IBSP 46 map's nodes, as
 * "<fault> <node> <child>" */
static int Tree( const char *name ) {
	FILE *f = fopen( name, "rb" ); long length; unsigned char *data; dheader_t header;
	dnode_t *nodes; void *scratch; int numNodes, numLeafs, node, child; bspTreeFault_t fault;
	if ( !f ) return 2;
	fseek( f, 0, SEEK_END ); length = ftell( f ); fseek( f, 0, SEEK_SET );
	data = malloc( length ? length : 1 );
	if ( length < (long)sizeof( header ) || fread( data, 1, length, f ) != (size_t)length ) return 2;
	fclose( f );
	memcpy( &header, data, sizeof( header ) );
	if ( !BSP_RangeInside( header.lumps[LUMP_NODES].fileofs, header.lumps[LUMP_NODES].filelen, (int)length ) ) return 2;
	numNodes = header.lumps[LUMP_NODES].filelen / (int)sizeof( dnode_t );
	numLeafs = header.lumps[LUMP_LEAFS].filelen / (int)sizeof( dleaf_t );
	nodes = malloc( numNodes ? numNodes * sizeof( dnode_t ) : 1 );
	memcpy( nodes, data + header.lumps[LUMP_NODES].fileofs, numNodes * sizeof( dnode_t ) );
	scratch = malloc( BSP_NodeTreeScratchSize( numNodes ) );
	fault = BSP_NodeTreeFault( nodes, numNodes, numLeafs, scratch, &node, &child );
	printf( "%d %d %d\n", (int)fault, node, child );
	return 0;
}
/* shim <in> <out> <renderer 0|1> <fakk2 0|1>: the map as the loaders see it */
int main( int argc, char **argv ) {
	FILE *f; long length; unsigned char *data; void *out = NULL; int outLength = 0;
	char error[256] = ""; unsigned ident, version; int renderer, fakk2;
	if ( argc == 3 && !strcmp( argv[1], "tree" ) ) return Tree( argv[2] );
	if ( argc != 5 || !( f = fopen( argv[1], "rb" ) ) ) return 2;
	renderer = atoi( argv[3] ); fakk2 = atoi( argv[4] );
	fseek( f, 0, SEEK_END ); length = ftell( f ); fseek( f, 0, SEEK_SET );
	data = malloc( length ? length : 1 );
	if ( fread( data, 1, length, f ) != (size_t)length ) return 2;
	fclose( f );
	memcpy( &ident, data, 4 ); memcpy( &version, data + 4, 4 );
	if ( ident == BSP_IDENT && version == BSP_VERSION_IHV ) {
		bsp43_translation_t t = { 0 };
		if ( !BSP43_TranslateToV46( data, (int)length, Alloc, Release, &t, error, sizeof( error ) ) ) goto failed;
		out = t.data; outLength = t.length;
	} else if ( ident == RBSP_IDENT ) {
		rbsp_translation_t t = { 0 };
		if ( !RBSP_TranslateToV46( data, (int)length, Alloc, Release, &t, error, sizeof( error ) ) ) goto failed;
		out = t.data; outLength = t.length;
	} else if ( ident == FAKK_IDENT ) {
		fakk_translation_t t = { 0 };
		const int flags = ( renderer ? FAKK_TRANSLATE_LIGHTGRID : FAKK_TRANSLATE_SHADER_SUBDIVISIONS )
			| ( fakk2 ? FAKK_TRANSLATE_VERSION_FAKK2 : 0 );
		if ( !FAKK_TranslateToV46Ex( data, (int)length, Alloc, Release, &t, error, sizeof( error ), flags ) ) goto failed;
		out = t.data; outLength = t.length;
	} else if ( ident == MOHAA_BSP_IDENT ) {
		mohaa_translation_t t;
		memset( &t, 0, sizeof( t ) );
		if ( !MOHAA_TranslateToV46( data, (int)length, Alloc, Release, &t, error, sizeof( error ) ) ) goto failed;
		out = t.data; outLength = t.length;
	} else if ( ident == BSP_IDENT && ( version == 44 || version == 45 ) ) {
		q3tBspTranslation_t t = { 0 };
		if ( !Q3TestBSP_TranslateToV46( data, (int)length, Alloc, Release, &t, error, sizeof( error ) ) ) goto failed;
		out = t.data; outLength = t.length;
	} else {
		out = data; outLength = (int)length;
	}
	if ( !( f = fopen( argv[2], "wb" ) ) ) return 2;
	fwrite( out, 1, outLength, f );
	fclose( f );
	return 0;
failed:
	fprintf( stderr, "%s\n", error );
	return 3;
}
'''

MAX_MAP_AREAS = 256
# Checks the loaders make; every other name is informational.
CHECKED = {
    'node.plane', 'node.child', 'leaf.area', 'leaf.cluster', 'leaf.surfaceRange', 'leaf.brushRange',
    'leafSurface.surface', 'model.surfaceRange', 'model.brushRange', 'brush.sideRange', 'brush.sixSides',
    'brushSide.plane', 'fog.sidePlane', 'face.vertRange', 'face.indexRange', 'face.index', 'tri.vertRange',
    'tri.indexRange', 'tri.index', 'patch.size', 'patch.vertRange', 'vis.partialHeader', 'node.tree',
}
# BSP_NodeTreeFault's results, in the order of bspTreeFault_t.
TREE_FAULTS = ('valid', 'cycle', 'shared', 'tooDeep', 'outside')


def build_shim(directory):
    compiler = shutil.which('clang')
    if not compiler:
        raise SystemExit('Clang is required to build the translation shim')
    source, exe = directory / 'shim.c', directory / 'shim.exe'
    source.write_text(SHIM, encoding='utf-8')
    target = compiler_target_args(compiler)
    abi = (['-DFNTECH3_Q3_ONLY'] if not target else []) if os.name == 'nt' else ['-m32']
    subprocess.run([compiler, *target, *abi, '-std=c11', '-O1', '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT),
                    str(source), '-o', str(exe)], check=True, cwd=ROOT)
    return exe


def engine_tree(shim, directory, data):
    """The loaders' own walk of the map's nodes (BSP_NodeTreeFault)."""
    path = directory / 'tree.bsp'
    path.write_bytes(data)
    run = subprocess.run([str(shim), 'tree', str(path)], capture_output=True, text=True, check=True)
    fault, node, child = (int(value) for value in run.stdout.split())
    return TREE_FAULTS[fault], node, child


def translate(shim, directory, data, renderer, fakk2):
    if data[:4] == b'IBSP' and struct.unpack_from('<i', data, 4)[0] in (46, 47):
        return data, None
    source, target = directory / 'in.bsp', directory / 'out.bsp'
    source.write_bytes(data)
    run = subprocess.run([str(shim), str(source), str(target), str(int(renderer)), str(int(fakk2))],
                         capture_output=True, text=True)
    return (target.read_bytes(), None) if run.returncode == 0 else (None, run.stderr.strip())


def lump(data, index, dtype):
    offset, length = struct.unpack_from('<ii', data, 8 + 8 * index)
    if length % dtype.itemsize:
        raise ValueError(f'lump {index} is not a whole number of records')
    return np.frombuffer(data, dtype=dtype, count=length // dtype.itemsize, offset=offset)


def outside(first, count, total):
    first, count = first.astype(np.int64), count.astype(np.int64)
    return (first < 0) | (count < 0) | (first > total) | (count > total - first)


def node_tree(children, numLeafs):
    """The shape of the tree every descent walks from node 0.

    `depth` counts the nodes on the longest path from node 0 to a leaf, and
    `descent` the nodes a walk of every branch visits, which is the number of
    nodes reached only when none is reached twice. A node or leaf is `shared`
    when two of the nodes reached name it; `cycle` names a node that leads back
    to one of its ancestors. Children outside their table are left out: the
    loaders refuse them before this.
    """
    numNodes = len(children)
    kids = [[int(c) for c in pair if c < numNodes and -1 - c < numLeafs] for pair in children.tolist()]
    if not numNodes:
        return dict(nodes=0, reached=0, depth=0, descent=0, sharedNodes=0, sharedLeafs=0, unreachedNodes=0,
                    unreachedLeafs=max(numLeafs - 1, 0), cycle=None, rootNamed=False)
    seen = bytearray(numNodes)
    seen[0] = 1
    stack = [0]
    while stack:
        for child in kids[stack.pop()]:
            if child >= 0 and not seen[child]:
                seen[child] = 1
                stack.append(child)
    nodeRefs, leafRefs = Counter(), Counter()
    for node in range(numNodes):
        if seen[node]:
            for child in kids[node]:
                (nodeRefs if child >= 0 else leafRefs)[child if child >= 0 else -1 - child] += 1
    # a cycle below node 0, by colouring each node while its branch is open
    state, cycle, height, descent = bytearray(numNodes), None, [0] * numNodes, [0] * numNodes
    state[0], path = 1, [[0, 0]]
    while path and cycle is None:
        node, side = path[-1]
        if side == len(kids[node]):
            below = [c for c in kids[node] if c >= 0]
            height[node] = 1 + max((height[c] for c in below), default=0)
            descent[node] = 1 + sum(descent[c] for c in below)
            state[node] = 2
            path.pop()
            continue
        path[-1][1] += 1
        child = kids[node][side]
        if child < 0:
            continue
        if state[child] == 1:
            cycle = dict(node=node, child=child, depth=len(path))
        elif not state[child]:
            state[child] = 1
            path.append([child, 0])
    return dict(nodes=numNodes, reached=sum(seen), depth=None if cycle else height[0],
                descent=None if cycle else descent[0],
                sharedNodes=sum(1 for count in nodeRefs.values() if count > 1),
                sharedLeafs=sum(1 for count in leafRefs.values() if count > 1),
                unreachedNodes=numNodes - sum(seen), unreachedLeafs=numLeafs - len(leafRefs), cycle=cycle,
                rootNamed=bool(nodeRefs[0]))


def audit(data):
    i4 = np.dtype('<i4')
    planes = lump(data, 2, np.dtype([('normal', '<f4', 3), ('dist', '<f4')]))
    nodes = lump(data, 3, np.dtype([('plane', '<i4'), ('children', '<i4', 2), ('bounds', '<i4', 6)]))
    leafs = lump(data, 4, np.dtype([('cluster', '<i4'), ('area', '<i4'), ('bounds', '<i4', 6), ('firstSurface', '<i4'),
                                     ('numSurfaces', '<i4'), ('firstBrush', '<i4'), ('numBrushes', '<i4')]))
    leafSurfaces, leafBrushes = lump(data, 5, i4), lump(data, 6, i4)
    models = lump(data, 7, np.dtype([('bounds', '<f4', 6), ('firstSurface', '<i4'), ('numSurfaces', '<i4'),
                                      ('firstBrush', '<i4'), ('numBrushes', '<i4')]))
    brushes = lump(data, 8, np.dtype([('firstSide', '<i4'), ('numSides', '<i4'), ('shader', '<i4')]))
    sides = lump(data, 9, np.dtype([('plane', '<i4'), ('shader', '<i4')]))
    numVerts = struct.unpack_from('<ii', data, 8 + 8 * 10)[1] // 44
    indexes = lump(data, 11, i4)
    fogs = lump(data, 12, np.dtype([('shader', 'S64'), ('brush', '<i4'), ('side', '<i4')]))
    surfaces = lump(data, 13, np.dtype([('shader', '<i4'), ('fog', '<i4'), ('type', '<i4'), ('firstVert', '<i4'),
                                        ('numVerts', '<i4'), ('firstIndex', '<i4'), ('numIndexes', '<i4'),
                                        ('lightmap', '<i4'), ('rest', 'V64'), ('width', '<i4'), ('height', '<i4')]))
    lightmaps = struct.unpack_from('<ii', data, 8 + 8 * 14)[1] // (128 * 128 * 3)
    visOffset, visLength = struct.unpack_from('<ii', data, 8 + 8 * 16)
    found, notes = {}, {}

    def check(name, bad, values):
        count = int(np.count_nonzero(bad))
        if count:
            found[name] = dict(count=count, first=int(np.flatnonzero(bad)[0]),
                               examples=np.asarray(values)[bad][:3].tolist())

    numNodes, numLeafs = len(nodes), len(leafs)
    check('node.plane', (nodes['plane'] < 0) | (nodes['plane'] >= len(planes)), nodes['plane'])
    children = nodes['children'].astype(np.int64)
    check('node.child', ((children >= 0) & (children >= numNodes)) | ((children < 0) & (-1 - children >= numLeafs)),
          children)
    order = (children >= 0) & (children <= np.arange(numNodes)[:, None])
    check('node.childBeforeParent (informational)', order, children)
    tree = node_tree(nodes['children'], numLeafs)
    check('leaf.area', (leafs['area'] < -1) | (leafs['area'] >= MAX_MAP_AREAS), leafs['area'])
    check('leaf.cluster', leafs['cluster'] < -1, leafs['cluster'])
    check('leaf.surfaceRange', outside(leafs['firstSurface'], leafs['numSurfaces'], len(leafSurfaces)),
          np.stack([leafs['firstSurface'], leafs['numSurfaces']], 1))
    check('leaf.brushRange', outside(leafs['firstBrush'], leafs['numBrushes'], len(leafBrushes)),
          np.stack([leafs['firstBrush'], leafs['numBrushes']], 1))
    notes['leafsWithClusterButNoArea'] = int(np.count_nonzero((leafs['cluster'] >= 0) & (leafs['area'] == -1)))
    check('leafSurface.surface', (leafSurfaces < 0) | (leafSurfaces >= len(surfaces)), leafSurfaces)
    # repaired, not refused, by CMod_CheckLeafBrushes
    check('leafBrush.brush (informational)', (leafBrushes < 0) | (leafBrushes >= len(brushes)), leafBrushes)
    check('model.surfaceRange', outside(models['firstSurface'], models['numSurfaces'], len(surfaces)),
          np.stack([models['firstSurface'], models['numSurfaces']], 1))
    check('model.brushRange', outside(models['firstBrush'], models['numBrushes'], len(brushes)),
          np.stack([models['firstBrush'], models['numBrushes']], 1))
    check('brush.sideRange', outside(brushes['firstSide'], brushes['numSides'], len(sides)),
          np.stack([brushes['firstSide'], brushes['numSides']], 1))
    check('brush.sixSides', outside(brushes['firstSide'], np.full(len(brushes), 6), len(sides)),
          np.stack([brushes['firstSide'], brushes['numSides']], 1))
    check('brush.fewerThanSixSides (informational)', brushes['numSides'] < 6, brushes['numSides'])
    check('brushSide.plane', (sides['plane'] < 0) | (sides['plane'] >= len(planes)), sides['plane'])
    fogPlanes = 0
    for fog in fogs:
        if 0 <= fog['brush'] < len(brushes):
            first = int(brushes['firstSide'][fog['brush']])
            for side in list(range(6)) + ([int(fog['side'])] if fog['side'] >= 0 else []):
                if 0 <= first + side < len(sides) and not 0 <= sides['plane'][first + side] < len(planes):
                    fogPlanes += 1
    if fogPlanes:
        found['fog.sidePlane'] = dict(count=fogPlanes)
    kind = surfaces['type']
    # Flares carry fog 0 in maps with no fog volume; the flare code bounds it.
    check('surface.fog (informational)', (surfaces['fog'] < -1) | (surfaces['fog'] >= len(fogs)),
          np.stack([kind, surfaces['fog']], 1))
    check('surface.lightmap (informational)', (kind != 4) & (surfaces['lightmap'] >= lightmaps),
          np.stack([kind, surfaces['lightmap']], 1))
    for name, value in (('face', 1), ('tri', 3)):
        subset = surfaces[kind == value]
        check(f'{name}.vertRange', outside(subset['firstVert'], subset['numVerts'], numVerts),
              np.stack([subset['firstVert'], subset['numVerts']], 1))
        check(f'{name}.indexRange', outside(subset['firstIndex'], subset['numIndexes'], len(indexes)),
              np.stack([subset['firstIndex'], subset['numIndexes']], 1))
        bad = 0
        for surface in subset:
            first, count = int(surface['firstIndex']), int(surface['numIndexes'])
            if first >= 0 and count > 0 and first + count <= len(indexes):
                values = indexes[first:first + count]
                bad += bool(np.count_nonzero((values < 0) | (values >= surface['numVerts'])))
        if bad:
            found[f'{name}.index'] = dict(count=bad)
    faces = surfaces[kind == 1]
    check('face.overVertexLimit (informational)', faces['numVerts'] > 64, faces['numVerts'])
    patches = surfaces[kind == 2]
    width, height = patches['width'].astype(np.int64), patches['height'].astype(np.int64)
    check('patch.size', (width < 1) | (height < 1) | (width > 65) | (height > 65) | (width * height > 1024),
          np.stack([width, height], 1))
    check('patch.vertRange', outside(patches['firstVert'], (width * height).clip(-1, 2 ** 31 - 1), numVerts),
          np.stack([patches['firstVert'], width, height], 1))
    if visLength:
        if visLength < 8:
            found['vis.partialHeader'] = dict(count=1)
        else:
            clusters, rowBytes = struct.unpack_from('<ii', data, visOffset)
            leafClusters = int(leafs['cluster'].max()) + 1 if numLeafs else 0
            fits = clusters >= 0 and rowBytes > 0 and clusters <= (visLength - 8) // rowBytes
            covers = fits and clusters >= leafClusters and rowBytes * 8 >= clusters
            if not covers:
                notes['visTableUnused'] = dict(clusters=clusters, rowBytes=rowBytes, leafClusters=leafClusters)
    counts = dict(planes=len(planes), nodes=numNodes, leafs=numLeafs, leafSurfaces=len(leafSurfaces),
                  leafBrushes=len(leafBrushes), models=len(models), brushes=len(brushes), brushSides=len(sides),
                  verts=numVerts, indexes=len(indexes), fogs=len(fogs), surfaces=len(surfaces))
    return counts, found, notes, tree


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--installation', action='append', default=[], metavar='KEY=PATH')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    installations = {}
    for item in args.installation:
        key, _, path = item.partition('=')
        if key not in TITLES or not path:
            parser.error(f'unknown installation {item!r}')
        installations[key] = Path(path).resolve(strict=True)
    (ROOT / '.tmp').mkdir(exist_ok=True)
    maps, totals, trees = {}, defaultdict(Counter), {}
    with tempfile.TemporaryDirectory(dir=ROOT / '.tmp') as scratch:
        scratch = Path(scratch)
        shim = build_shim(scratch)
        for title, path in sorted(installations.items()):
            roots = sorted(p for p in path.iterdir() if p.is_dir()) if title == 'q3test' else [path]
            for root in roots:
                for (folder, name), source in sorted(discover(title, root).items()):
                    where, _, entry = source.partition(':')
                    if where == 'loose':
                        data = (root / folder / entry).read_bytes()
                    else:
                        with zipfile.ZipFile(root / folder / where) as archive:
                            data = archive.read(entry)
                    label = f'{title}:{folder}/{name}' if title != 'q3test' else f'q3test-{root.name}:{name}'
                    for renderer in (0, 1) if data[:4] == b'FAKK' else (0,):
                        key = label + (' (as the renderers translate it)' if renderer else '')
                        translated, error = translate(shim, scratch, data, renderer, title == 'fakk2')
                        if translated is None:
                            maps[key] = dict(translationRefused=error)
                            totals[title]['translationRefused'] += 1
                            continue
                        counts, found, notes, tree = audit(translated)
                        fault, node, child = engine_tree(shim, scratch, translated)
                        tree['loaderWalk'] = fault
                        if fault != 'valid':
                            found['node.tree'] = dict(count=1, first=node, examples=[[node, child, fault]])
                        maps[key] = dict(source=source, counts=counts, fails=found, notes=notes, tree=tree)
                        totals[title]['maps'] += 1
                        for name_ in found:
                            totals[title][name_] += 1
                        shape = trees.setdefault(title, dict(deepest=None, depth=0, sharedNodes=0, sharedLeafs=0,
                                                             unreachedNodes=0, unreachedLeafs=0, cycles=0,
                                                             loaderWalkFaults=0))
                        shape['loaderWalkFaults'] += fault != 'valid'
                        if tree['depth'] is not None and tree['depth'] > shape['depth']:
                            shape['depth'], shape['deepest'] = tree['depth'], key
                        for name_ in ('sharedNodes', 'sharedLeafs', 'unreachedNodes', 'unreachedLeafs'):
                            shape[name_] += bool(tree[name_])
                        shape['cycles'] += tree['cycle'] is not None
            print(title, dict(totals[title]), trees.get(title), flush=True)
    failures = sum(1 for row in maps.values() for name in row.get('fails', {}) if name in CHECKED)
    report = dict(checks=sorted(CHECKED), titles={k: dict(v) for k, v in sorted(totals.items())},
                  trees=dict(sorted(trees.items())), loaderCheckFailures=failures, maps=maps)
    args.output.write_text(json.dumps(report, indent=1) + '\n', encoding='utf-8')
    print('checks the loaders make that a map fails:', failures)
    return 1 if failures else 0


if __name__ == '__main__':
    raise SystemExit(main())
