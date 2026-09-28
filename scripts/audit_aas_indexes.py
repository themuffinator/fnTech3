#!/usr/bin/env python3
"""Measure every index an installed bot navigation file gives into its own tables.

Every `.aas` file the installations carry, in every pak and loose, is read as
the botlib reads it: Quake III's EAAS versions 4 and 5 (Quake III, Team Arena,
Quake Live, Elite Force) and RTCW's version 8. Each number one table gives into
another is measured against the table it names, and the node tree is walked
from node 1 as every descent walks it: how deep it goes, whether any node is
reached twice and whether a child leads back above itself. A few properties
the loaders deliberately leave alone are measured too, to show why
(`informational`), with the sizes Quake III's routing lays out.

The loaders' own checks, code/botlib/aas_index_checks.hpp, are also compiled
into a small shim with Clang (run in the VS x86 dev shell) and run over each
file as its loader would run them (`loaderCheck`).

Nothing is run and nothing is written to an installation. The report is JSON:
per file, the record counts, the tree's shape and every check a record fails,
with examples.

  --installation KEY=PATH  q3, ql, rtcw, wolfet or stvef, as for
                           probe_retail_map_loads.py
"""
from collections import Counter, defaultdict
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from compiler_support import compiler_target_args  # noqa: E402

SHIM = r'''
#include "code/qcommon/q_shared.h"
#include "code/botlib/aasfile.h"
#include "code/botlib/aas_index_checks.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

/* The tables a loader hands the checks, under the names of aas_t. */
struct World {
	int numvertexes, numplanes;
	aas_edge_t *edges; int numedges;
	aas_edgeindex_t *edgeindex; int edgeindexsize;
	aas_face_t *faces; int numfaces;
	aas_faceindex_t *faceindex; int faceindexsize;
	aas_area_t *areas; int numareas;
	aas_areasettings_t *areasettings; int numareasettings;
	aas_reachability_t *reachability; int reachabilitysize;
	aas_node_t *nodes; int numnodes;
	aas_portal_t *portals; int numportals;
	aas_portalindex_t *portalindex; int portalindexsize;
	aas_cluster_t *clusters; int numclusters;
};

/* A lump's records, `stride` bytes apart on disk, as the engine's tables. */
template <class T> static std::vector<T> Table( const std::vector<unsigned char> &data, const aas_lump_t &lump,
	size_t stride ) {
	std::vector<T> out( (size_t)lump.filelen / stride );
	for ( size_t i = 0; i < out.size(); i++ ) {
		memcpy( &out[i], data.data() + lump.fileofs + i * stride, sizeof( T ) );
	}
	return out;
}

/* shim <file>: "<fault> <words>" for the navigation file, checked as its
 * loader checks it: with Quake III's routing limits unless it is RTCW's
 * version 8. A lump outside the file is "-1". */
int main( int argc, char **argv ) {
	std::vector<unsigned char> data;
	aas_header_t header;
	FILE *f;
	long length;
	int i;

	if ( argc != 2 || !( f = fopen( argv[1], "rb" ) ) ) return 2;
	fseek( f, 0, SEEK_END ); length = ftell( f ); fseek( f, 0, SEEK_SET );
	data.resize( length > 0 ? (size_t)length : 1 );
	if ( length < (long)sizeof( header ) || fread( data.data(), 1, (size_t)length, f ) != (size_t)length ) return 2;
	fclose( f );
	memcpy( &header, data.data(), sizeof( header ) );
	if ( header.version != AASVERSION_OLD ) {
		unsigned char *bytes = (unsigned char *)&header;
		for ( i = 8; i < (int)sizeof( header ); i++ ) bytes[i] ^= (unsigned char)( ( i - 8 ) * 119 );
	}
	for ( i = 0; i < AAS_LUMPS; i++ ) {
		if ( !AAS_RangeInside( header.lumps[i].fileofs, header.lumps[i].filelen, (int)length ) ) {
			printf( "-1 lump %d is outside the file\n", i );
			return 0;
		}
	}
	const bool rtcw = header.version == 8;
	std::vector<aas_edge_t> edges = Table<aas_edge_t>( data, header.lumps[AASLUMP_EDGES], sizeof( aas_edge_t ) );
	std::vector<aas_edgeindex_t> edgeindex = Table<aas_edgeindex_t>( data, header.lumps[AASLUMP_EDGEINDEX], sizeof( aas_edgeindex_t ) );
	std::vector<aas_face_t> faces = Table<aas_face_t>( data, header.lumps[AASLUMP_FACES], sizeof( aas_face_t ) );
	std::vector<aas_faceindex_t> faceindex = Table<aas_faceindex_t>( data, header.lumps[AASLUMP_FACEINDEX], sizeof( aas_faceindex_t ) );
	std::vector<aas_area_t> areas = Table<aas_area_t>( data, header.lumps[AASLUMP_AREAS], sizeof( aas_area_t ) );
	std::vector<aas_areasettings_t> settings = Table<aas_areasettings_t>( data, header.lumps[AASLUMP_AREASETTINGS],
		rtcw ? 32 : sizeof( aas_areasettings_t ) );
	std::vector<aas_reachability_t> reach = Table<aas_reachability_t>( data, header.lumps[AASLUMP_REACHABILITY], 44 );
	std::vector<aas_node_t> nodes = Table<aas_node_t>( data, header.lumps[AASLUMP_NODES], sizeof( aas_node_t ) );
	std::vector<aas_portal_t> portals = Table<aas_portal_t>( data, header.lumps[AASLUMP_PORTALS], sizeof( aas_portal_t ) );
	std::vector<aas_portalindex_t> portalindex = Table<aas_portalindex_t>( data, header.lumps[AASLUMP_PORTALINDEX], sizeof( aas_portalindex_t ) );
	std::vector<aas_cluster_t> clusters = Table<aas_cluster_t>( data, header.lumps[AASLUMP_CLUSTERS], sizeof( aas_cluster_t ) );
	World w;
	w.numvertexes = header.lumps[AASLUMP_VERTEXES].filelen / (int)sizeof( aas_vertex_t );
	w.numplanes = header.lumps[AASLUMP_PLANES].filelen / (int)sizeof( aas_plane_t );
	w.edges = edges.data(); w.numedges = (int)edges.size();
	w.edgeindex = edgeindex.data(); w.edgeindexsize = (int)edgeindex.size();
	w.faces = faces.data(); w.numfaces = (int)faces.size();
	w.faceindex = faceindex.data(); w.faceindexsize = (int)faceindex.size();
	w.areas = areas.data(); w.numareas = (int)areas.size();
	w.areasettings = settings.data(); w.numareasettings = (int)settings.size();
	w.reachability = reach.data(); w.reachabilitysize = (int)reach.size();
	w.nodes = nodes.data(); w.numnodes = (int)nodes.size();
	w.portals = portals.data(); w.numportals = (int)portals.size();
	w.portalindex = portalindex.data(); w.portalindexsize = (int)portalindex.size();
	w.clusters = clusters.data(); w.numclusters = (int)clusters.size();
	std::vector<unsigned char> scratch( AAS_IndexScratchSize( w ) );
	char words[256];
	const aasIndexFault_t fault = AAS_IndexFault( w, !rtcw, scratch.data(), words, sizeof( words ) );
	printf( "%d %s\n", (int)fault, words );
	return 0;
}
'''


def build_shim(directory):
    compiler = shutil.which('clang++')
    if not compiler:
        raise SystemExit('Clang is required to build the checking shim')
    source, exe = directory / 'shim.cpp', directory / 'shim.exe'
    source.write_text(SHIM, encoding='utf-8')
    target = compiler_target_args(compiler)
    abi = (['-DFNTECH3_Q3_ONLY'] if not target else []) if sys.platform == 'win32' else []
    subprocess.run([compiler, *target, *abi, '-std=c++17', '-O1', '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT),
                    str(source), '-o', str(exe)], check=True, cwd=ROOT)
    return exe


def loader_check(shim, directory, data):
    """The loaders' own checks over the file: (fault number, words); 0 when it passes."""
    path = directory / 'file.aas'
    path.write_bytes(data)
    run = subprocess.run([str(shim), str(path)], capture_output=True, text=True, check=True)
    fault, _, words = run.stdout.strip().partition(' ')
    return int(fault), words


# The whole loader, AAS_LoadAASFile from code/botlib/be_aas_file.cpp as the
# botlib fixtures compile it (tests/aas_index_checks_tests.py), loading a file
# under a profile with the map checksum its header carries.
LOADER_MAIN = r'''
#include <fstream>
#include <iterator>
int main(int argc,char **argv){
 if(argc!=3)return 2;
 profile=nullptr;
 for(const auto &p:profiles)if(!strcmp(p.key,argv[1]))profile=&p;
 if(!profile)return 2;
 std::ifstream in(argv[2],std::ios::binary);
 std::vector<byte> data((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
 if(data.size()<124)return 2;
 unsigned char header[124];std::memcpy(header,data.data(),124);
 if(header[4]!=4)for(unsigned i=8;i<124;++i)header[i]^=static_cast<byte>((i-8)*119);
 int checksum;std::memcpy(&checksum,header+8,4);
 initialize();LibVarSet("sv_mapChecksum",std::to_string(Com_GameIsRTCW()?(long long)(unsigned)checksum:(long long)checksum).c_str());
 files["maps/probe.aas"]=data;
 const int result=AAS_LoadAASFile("maps/probe.aas");
 printf("%d %d %d %s\n",result,aasworld.numareas,aasworld.reachabilitysize,(Com_GameIsRTCW()?printed:lastError).c_str());
 shutdown();
 return 0;
}
'''
# Each title's files load under this profile (the profile table's own keys).
PROFILES = dict(q3='quake3', ql='quakelive', rtcw='rtcw-sp', stvef='stvef-mp', wolfet='wolfet')


def build_loader(directory):
    from aas_index_checks_tests import loader_source
    compiler = shutil.which('clang++')
    source, exe = directory / 'loader.cpp', directory / 'loader.exe'
    source.write_text(loader_source() + LOADER_MAIN, encoding='utf-8')
    target = compiler_target_args(compiler)
    abi = (['-DFNTECH3_Q3_ONLY'] if not target else []) if sys.platform == 'win32' else []
    subprocess.run([compiler, *target, *abi, '-std=c++17', '-O1', '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT),
                    str(source), str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
                    str(ROOT / 'code/compat/game_abi_adapter.c'), '-o', str(exe)], check=True, cwd=ROOT)
    return exe


def loader_load(loader, directory, title, data):
    """AAS_LoadAASFile's result, areas, reachabilities and message for the file."""
    path = directory / 'file.aas'
    path.write_bytes(data)
    run = subprocess.run([str(loader), PROFILES[title], str(path)], capture_output=True, text=True, check=True)
    result, areas, reach, message = (run.stdout.rstrip('\n').split(' ', 3) + [''])[:4]
    return dict(result=int(result), areas=int(areas), reachabilities=int(reach), message=message.strip())

# The game folders each title's navigation files live in.
TITLES = {
    'q3': ('baseq3', 'missionpack'),
    'ql': ('baseq3',),
    'rtcw': ('main',),
    'wolfet': ('etmain',),
    'stvef': ('baseef',),
}
HEADER = 124
IDENT = b'EAAS'
LUMPS = ('bboxes', 'vertexes', 'planes', 'edges', 'edgeindex', 'faces', 'faceindex', 'areas', 'areasettings',
         'reachability', 'nodes', 'portals', 'portalindex', 'clusters')
I4 = np.dtype('<i4')
DTYPES = {
    'bboxes': np.dtype([('presencetype', '<i4'), ('flags', '<i4'), ('mins', '<f4', 3), ('maxs', '<f4', 3)]),
    'vertexes': np.dtype([('xyz', '<f4', 3)]),
    'planes': np.dtype([('normal', '<f4', 3), ('dist', '<f4'), ('type', '<i4')]),
    'edges': np.dtype([('v', '<i4', 2)]),
    'edgeindex': I4,
    'faces': np.dtype([('planenum', '<i4'), ('faceflags', '<i4'), ('numedges', '<i4'), ('firstedge', '<i4'),
                       ('frontarea', '<i4'), ('backarea', '<i4')]),
    'faceindex': I4,
    'areas': np.dtype([('areanum', '<i4'), ('numfaces', '<i4'), ('firstface', '<i4'), ('mins', '<f4', 3),
                       ('maxs', '<f4', 3), ('center', '<f4', 3)]),
    'areasettings': np.dtype([('contents', '<i4'), ('areaflags', '<i4'), ('presencetype', '<i4'), ('cluster', '<i4'),
                              ('clusterareanum', '<i4'), ('numreachableareas', '<i4'),
                              ('firstreachablearea', '<i4')]),
    'reachability': np.dtype([('areanum', '<i4'), ('facenum', '<i4'), ('edgenum', '<i4'), ('start', '<f4', 3),
                              ('end', '<f4', 3), ('traveltype', '<i4'), ('traveltime', '<u2'), ('pad', '<u2')]),
    'nodes': np.dtype([('planenum', '<i4'), ('children', '<i4', 2)]),
    'portals': np.dtype([('areanum', '<i4'), ('frontcluster', '<i4'), ('backcluster', '<i4'),
                         ('clusterareanum', '<i4', 2)]),
    'portalindex': I4,
    'clusters': np.dtype([('numareas', '<i4'), ('numreachabilityareas', '<i4'), ('numportals', '<i4'),
                          ('firstportal', '<i4')]),
}
# RTCW's area settings carry the area's ground steepness after Quake III's fields.
RTCW_SETTINGS = np.dtype(DTYPES['areasettings'].descr + [('groundsteepness', '<f4')])
# Checks the loaders make; every other name is informational.
CHECKED = {
    'lump.extent', 'nodes.root', 'planes.pairs', 'areasettings.count', 'node.plane', 'node.child', 'node.tree',
    'area.faceRange', 'faceIndex.face', 'face.plane', 'face.edgeRange', 'face.area', 'edgeIndex.edge',
    'edge.vertex', 'settings.reachRange', 'settings.cluster', 'settings.clusterArea', 'settings.portalArea',
    'reach.area', 'portal.area', 'portal.cluster', 'portal.clusterArea', 'portal.areaCluster',
    'portalIndex.portal', 'portalIndex.member', 'cluster.counts', 'cluster.portalRange', 'cluster.localUnique',
    'settings.clusterless', 'settings.reachTotal', 'cluster.localTotal',
}


def discover(title, installation):
    """Every navigation file the installation carries: [(game dir, where, entry)], later paks last."""
    found = []
    for folder in sorted(p for p in installation.iterdir() if p.is_dir() and p.name.lower() in TITLES[title]):
        for pak in sorted((p for p in folder.iterdir() if p.is_file() and p.suffix.lower() == '.pk3'),
                          key=lambda p: p.name.lower()):
            with zipfile.ZipFile(pak) as archive:
                for entry in archive.namelist():
                    if entry.lower().startswith('maps/') and entry.lower().endswith('.aas'):
                        found.append((folder.name, pak.name, entry))
        maps = folder / 'maps'
        if maps.is_dir():
            for path in sorted(maps.rglob('*')):
                if path.is_file() and path.suffix.lower() == '.aas':
                    found.append((folder.name, 'loose', f'maps/{path.relative_to(maps).as_posix()}'))
    return found


def header(data):
    """(version, checksum, [(offset, length)]) with the header decoded as the loaders decode it."""
    if len(data) < HEADER or data[:4] != IDENT:
        return None
    version = struct.unpack_from('<i', data, 4)[0]
    raw = bytearray(data[:HEADER])
    if version != 4:  # versions 5 and 8 scramble everything after the version
        for i in range(8, HEADER):
            raw[i] ^= ((i - 8) * 119) & 0xff
    checksum = struct.unpack_from('<i', raw, 8)[0]
    lumps = [struct.unpack_from('<ii', raw, 12 + 8 * i) for i in range(len(LUMPS))]
    return version, checksum, lumps


def plain(record):
    """A record as JSON: its fields in order, arrays as lists."""
    if record is None:
        return None
    return [value.tolist() if hasattr(value, 'tolist') else value for value in record.item()]


def outside(first, count, total):
    first, count = first.astype(np.int64), count.astype(np.int64)
    return (first < 0) | (count < 0) | (first > total) | (count > total - first)


def signed_outside(values, total):
    values = values.astype(np.int64)
    return np.abs(values) >= total


def node_tree(children, numAreas):
    """The shape of the tree every descent walks from node 1.

    A child above zero names a node, zero is solid, and below zero names area
    -child. `depth` counts the nodes on the longest path from node 1 to a leaf
    and `descent` the nodes a walk of every branch visits, which equals the
    nodes reached only when none is reached twice. A node is `shared` when two
    of the nodes reached name it; `cycle` names a node leading back to one of
    its ancestors. Areas that several nodes name are normal: each area is a
    convex piece that planes may cut into several leaves. Children outside
    their table are left out: the loaders refuse them before this.
    """
    numNodes = len(children)
    kids = [[int(c) for c in pair if (0 < c < numNodes) or (c <= 0 and -int(c) < numAreas)]
            for pair in children.tolist()]
    if numNodes < 2:
        return dict(nodes=numNodes, reached=0, depth=0, descent=0, sharedNodes=0, unreachedNodes=max(numNodes - 1, 0),
                    areasNamed=0, areasNamedTwice=0, solidLeaves=0, cycle=None, rootNamed=False)
    seen = bytearray(numNodes)
    seen[1] = 1
    stack = [1]
    while stack:
        for child in kids[stack.pop()]:
            if child > 0 and not seen[child]:
                seen[child] = 1
                stack.append(child)
    nodeRefs, areaRefs, solid = Counter(), Counter(), 0
    for node in range(1, numNodes):
        if seen[node]:
            for child in kids[node]:
                if child > 0:
                    nodeRefs[child] += 1
                elif child < 0:
                    areaRefs[-child] += 1
                else:
                    solid += 1
    state, cycle, height, descent = bytearray(numNodes), None, [0] * numNodes, [0] * numNodes
    state[1], path = 1, [[1, 0]]
    while path and cycle is None:
        node, side = path[-1]
        if side == len(kids[node]):
            below = [c for c in kids[node] if c > 0]
            height[node] = 1 + max((height[c] for c in below), default=0)
            descent[node] = 1 + sum(descent[c] for c in below)
            state[node] = 2
            path.pop()
            continue
        path[-1][1] += 1
        child = kids[node][side]
        if child <= 0:
            continue
        if state[child] == 1:
            cycle = dict(node=node, child=child, depth=len(path))
        elif not state[child]:
            state[child] = 1
            path.append([child, 0])
    return dict(nodes=numNodes, reached=sum(seen), depth=None if cycle else height[1],
                descent=None if cycle else descent[1],
                sharedNodes=sum(1 for count in nodeRefs.values() if count > 1),
                unreachedNodes=numNodes - 1 - sum(seen), areasNamed=len(areaRefs),
                areasNamedTwice=sum(1 for count in areaRefs.values() if count > 1), solidLeaves=solid,
                cycle=cycle, rootNamed=bool(nodeRefs[1]))


def audit(data):
    parsed = header(data)
    if parsed is None:
        return None, dict(header=dict(count=1)), {}, None
    version, checksum, lumps = parsed
    found, notes = {}, {}

    def check(name, bad, values):
        count = int(np.count_nonzero(bad))
        if count:
            found[name] = dict(count=count, first=int(np.flatnonzero(bad)[0]),
                               examples=np.asarray(values)[bad][:3].tolist())

    dtypes = dict(DTYPES)
    if version == 8:
        dtypes['areasettings'] = RTCW_SETTINGS
    extent = [i for i, (offset, length) in enumerate(lumps)
              if offset < 0 or length < 0 or offset > len(data) or length > len(data) - offset]
    if extent:
        found['lump.extent'] = dict(count=len(extent), first=extent[0],
                                    examples=[[LUMPS[i], *lumps[i]] for i in extent[:3]])
        return version, found, notes, None
    partial = [LUMPS[i] for i, (_, length) in enumerate(lumps) if length % dtypes[LUMPS[i]].itemsize]
    if partial:
        found['lump.partialRecord (informational)'] = dict(count=len(partial), examples=partial[:3])
    order = [LUMPS[i] for i in range(len(LUMPS))
             if lumps[i][0] != (HEADER if i == 0 else lumps[i - 1][0] + lumps[i - 1][1])]
    if order:
        found['lump.notSequential (informational)'] = dict(count=len(order), examples=order[:3])
    t = {name: np.frombuffer(data, dtype=dtypes[name], count=length // dtypes[name].itemsize, offset=offset)
         for name, (offset, length) in zip(LUMPS, lumps)}
    n = {name: len(t[name]) for name in LUMPS}
    nodes, planes, areas, faces, edges = t['nodes'], t['planes'], t['areas'], t['faces'], t['edges']
    settings, reach, portals, clusters = t['areasettings'], t['reachability'], t['portals'], t['clusters']

    # counts every walk and every paired plane lookup relies on
    if n['nodes'] < 2:
        found['nodes.root'] = dict(count=1, examples=[n['nodes']])
    if n['planes'] % 2:
        found['planes.pairs'] = dict(count=1, examples=[n['planes']])
    if n['areasettings'] != n['areas']:
        found['areasettings.count'] = dict(count=1, examples=[[n['areasettings'], n['areas']]])
    numAreas = min(n['areas'], n['areasettings'])

    # the node tree, from node 1: node 0 is the solid leaf no descent reads
    number = np.arange(n['nodes'])
    real = number > 0
    check('node.plane', real & ((nodes['planenum'] < 0) | (nodes['planenum'] >= n['planes'])), nodes['planenum'])
    children = nodes['children'].astype(np.int64)
    check('node.child', real & (((children > 0) & (children >= n['nodes']))
                                | ((children < 0) & (-children >= numAreas))).any(1), children)
    order = real[:, None] & (children > 0) & (children <= number[:, None])
    check('node.childBeforeParent (informational)', order.any(1), children)
    tree = node_tree(nodes['children'], numAreas)
    if tree['cycle'] or tree['sharedNodes'] or tree['rootNamed']:
        found['node.tree'] = dict(count=1, examples=[dict(cycle=tree['cycle'], sharedNodes=tree['sharedNodes'],
                                                          rootNamed=tree['rootNamed'])])
    notes['node0'] = plain(nodes[0]) if n['nodes'] else None

    # the geometry: areas -> face index -> faces -> edge index -> edges ->
    # vertexes. A signed index cannot name record 0, so face 0 and edge 0 are
    # dummies nothing reaches.
    check('area.faceRange', outside(areas['firstface'], areas['numfaces'], n['faceindex']),
          np.stack([areas['firstface'], areas['numfaces']], 1))
    check('area.number (informational)', areas['areanum'] != np.arange(n['areas']), areas['areanum'])
    check('faceIndex.face', (t['faceindex'] == 0) | signed_outside(t['faceindex'], n['faces']), t['faceindex'])
    real = np.arange(n['faces']) > 0
    check('face.plane', real & ((faces['planenum'] < 0) | (faces['planenum'] >= n['planes'])), faces['planenum'])
    check('face.edgeRange', real & outside(faces['firstedge'], faces['numedges'], n['edgeindex']),
          np.stack([faces['firstedge'], faces['numedges']], 1))
    check('face.area', real & ((faces['frontarea'] < 0) | (faces['frontarea'] >= numAreas) | (faces['backarea'] < 0)
                               | (faces['backarea'] >= numAreas)),
          np.stack([faces['frontarea'], faces['backarea']], 1))
    check('edgeIndex.edge', (t['edgeindex'] == 0) | signed_outside(t['edgeindex'], n['edges']), t['edgeindex'])
    real = np.arange(n['edges']) > 0
    check('edge.vertex', real & ((edges['v'] < 0) | (edges['v'] >= n['vertexes'])).any(1), edges['v'])
    check('edge0.vertex (informational)', ~real & ((edges['v'] < 0) | (edges['v'] >= n['vertexes'])).any(1),
          edges['v'])
    notes['area0'] = plain(areas[0]) if n['areas'] else None
    notes['face0'] = plain(faces[0]) if n['faces'] else None
    notes['edge0'] = plain(edges[0]) if n['edges'] else None

    # routing: area settings, reachabilities, portals and clusters
    s = settings[:numAreas]
    check('settings.reachRange', outside(s['firstreachablearea'], s['numreachableareas'], n['reachability']),
          np.stack([s['firstreachablearea'], s['numreachableareas']], 1))
    cluster = s['cluster'].astype(np.int64)
    badCluster = ((cluster > 0) & (cluster >= n['clusters'])) | ((cluster < 0) & (-cluster >= n['portals']))
    check('settings.cluster', badCluster, cluster)
    local = s['clusterareanum'].astype(np.int64)
    inCluster = (cluster > 0) & ~badCluster
    limit = np.zeros(numAreas, np.int64)
    limit[inCluster] = clusters['numareas'][cluster[inCluster]]
    check('settings.clusterArea', inCluster & ((local < 0) | (local >= limit)), np.stack([cluster, local], 1))
    isPortal = (cluster < 0) & ~badCluster
    back = np.full(numAreas, -1, np.int64)
    back[isPortal] = portals['areanum'][-cluster[isPortal]]
    check('settings.portalArea', isPortal & (back != np.arange(numAreas)), np.stack([cluster, back], 1))
    # area 0 is never a start or a goal, and routing reverses nothing from it
    check('settings.clusterless', (np.arange(numAreas) > 0) & (s['numreachableareas'] > 0) & (cluster == 0),
          np.stack([np.arange(numAreas), s['numreachableareas']], 1))
    check('settings.area0Reachabilities (informational)', (np.arange(numAreas) == 0) & (s['numreachableareas'] > 0),
          s['numreachableareas'])
    check('settings.clusterZero (informational)', (np.arange(numAreas) > 0) & (cluster == 0), np.arange(numAreas))
    notes['settings0'] = plain(s[0]) if numAreas else None
    notes['mostReachabilities'] = int(s['numreachableareas'].max()) if numAreas else 0
    # the reversed links Quake routing makes: each area's first 128
    # reachabilities, from area 1, into a table of one link a reachability
    linked = np.minimum(np.clip(s['numreachableareas'][1:].astype(np.int64), 0, None), 128)
    links = int(linked.sum())
    if links > n['reachability']:
        found['settings.reachTotal'] = dict(count=1, examples=[[links, n['reachability']]])
    incoming = np.zeros(max(numAreas, 1), np.int64)
    for area in range(1, numAreas):
        first, count = int(s['firstreachablearea'][area]), int(s['numreachableareas'][area])
        if 0 <= first and 0 <= count <= n['reachability'] - first:
            targets = reach['areanum'][first:first + min(count, 128)]
            targets = targets[(targets >= 0) & (targets < numAreas)]
            np.add.at(incoming, targets, 1)
    notes['reversedLinks'] = links
    notes['mostIncomingLinks'] = int(incoming.max())
    notes['areasOver128Incoming'] = int(np.count_nonzero(incoming > 128))
    # the travel-time table AAS_CalculateAreaTravelTimes sizes: for every
    # area a row pointer per reachability and a padded row of times per
    # incoming link; 32-bit pointers and longs on x86, 64-bit on x86-64 Linux
    rows = np.clip(s['numreachableareas'].astype(np.int64), 0, None)
    for bits, word in ((32, 4), (64, 8)):
        size = numAreas * word + int((rows * word + rows * (((incoming[:numAreas] + word - 1) // word) * word) * 2).sum())
        notes[f'travelTimeBytes{bits}'] = size
    check('reach.area', (reach['areanum'] < 0) | (reach['areanum'] >= numAreas), reach['areanum'])
    check('reach.areaZero (informational)', (np.arange(n['reachability']) > 0) & (reach['areanum'] == 0),
          np.arange(n['reachability']))
    check('portal.area', (portals['areanum'] < 0) | (portals['areanum'] >= numAreas), portals['areanum'])
    sides = np.stack([portals['frontcluster'], portals['backcluster']], 1).astype(np.int64)
    badSide = (sides < 0) | (sides >= n['clusters'])
    check('portal.cluster', badSide.any(1), sides)
    plocal = portals['clusterareanum'].astype(np.int64)
    limit = np.zeros_like(plocal)
    limit[~badSide] = clusters['numareas'][sides[~badSide]]
    # portal 0 is the dummy every area outside a cluster routes through: its
    # numbers are used, so they must not be negative, but it borders no cluster
    pnumber = np.arange(n['portals'])[:, None]
    check('portal.clusterArea', ((plocal < 0) | ((pnumber > 0) & (plocal >= limit))).any(1), plocal)
    areaOk = (portals['areanum'] >= 0) & (portals['areanum'] < numAreas)
    areaCluster = np.zeros(n['portals'], np.int64)
    areaCluster[areaOk] = settings['cluster'][portals['areanum'][areaOk]]
    check('portal.areaCluster', (np.arange(n['portals']) > 0) & (areaCluster != -np.arange(n['portals'])),
          np.stack([portals['areanum'], areaCluster], 1))
    notes['portal0'] = plain(portals[0]) if n['portals'] else None
    pindex = t['portalindex']
    check('portalIndex.portal', (pindex < 0) | (pindex >= n['portals']), pindex)
    ccounts = np.stack([clusters['numareas'], clusters['numreachabilityareas']], 1)
    check('cluster.counts', (clusters['numareas'] < 0) | (clusters['numreachabilityareas'] < 0)
          | (clusters['numreachabilityareas'] > clusters['numareas']), ccounts)
    check('cluster.moreAreasThanMap (informational)', clusters['numareas'] > n['areas'], clusters['numareas'])
    check('cluster.portalRange', outside(clusters['firstportal'], clusters['numportals'], n['portalindex']),
          np.stack([clusters['firstportal'], clusters['numportals']], 1))
    notes['cluster0'] = plain(clusters[0]) if n['clusters'] else None
    member = 0
    for c in range(n['clusters']):
        first, count = int(clusters['firstportal'][c]), int(clusters['numportals'][c])
        if 0 <= first and 0 <= count <= n['portalindex'] - first:
            for p in pindex[first:first + count]:
                if 0 <= p < n['portals'] and c not in (int(portals['frontcluster'][p]), int(portals['backcluster'][p])):
                    member += 1
    if member:
        found['portalIndex.member'] = dict(count=member)
    # every cluster-local number: one for each area of a cluster, two for a portal
    locals_ = int(np.clip(clusters['numareas'].astype(np.int64), 0, None).sum())
    notes['clusterLocals'] = [locals_, numAreas + 2 * n['portals']]
    if locals_ > numAreas + 2 * n['portals']:
        found['cluster.localTotal'] = dict(count=1, examples=[[locals_, numAreas, n['portals']]])
    # each cluster-local number names one area: an area of the cluster, or a portal on its border
    owners, shared = {}, 0
    for area in range(1, numAreas):
        c, k = int(settings['cluster'][area]), int(settings['clusterareanum'][area])
        slots = []
        if 0 < c < n['clusters']:
            slots = [(c, k)]
        elif c < 0 and -c < n['portals']:
            p = portals[-c]
            slots = [(int(p['frontcluster']), int(p['clusterareanum'][0])),
                     (int(p['backcluster']), int(p['clusterareanum'][1]))]
        for slot in slots:
            if slot in owners and owners[slot] != area:
                shared += 1
            owners.setdefault(slot, area)
    if shared:
        found['cluster.localUnique'] = dict(count=shared)
    notes['clusterLocalsNamed'] = len(owners)
    reachUse = [int(clusters['numreachabilityareas'][c]) for c in range(1, n['clusters'])]
    notes['largestClusterReachabilityAreas'] = max(reachUse, default=0)
    counts = dict(n, version=version)
    return version, found, notes, dict(tree=tree, counts=counts, checksum=checksum)


def summary(rows):
    """Each title's extremes, over the files that parsed."""
    rows = [(key, row) for key, row in rows if row.get('tree')]
    if not rows:
        return {}
    depths = sorted(row['tree']['depth'] for _, row in rows if row['tree']['depth'] is not None)

    def most(value):
        key, row = max(rows, key=lambda kr: value(kr[1]))
        return [value(row), key]

    return dict(
        files=len(rows), served=sum(1 for _, row in rows if row.get('served')),
        deepestTree=most(lambda row: row['tree']['depth'] or 0), medianDepth=depths[len(depths) // 2],
        mostNodes=most(lambda row: row['tree']['nodes']), mostAreas=most(lambda row: row['counts']['areas']),
        mostReachabilities=most(lambda row: row['counts']['reachability']),
        mostReachabilitiesInAnArea=most(lambda row: row['notes']['mostReachabilities']),
        mostLinksIntoAnArea=most(lambda row: row['notes']['mostIncomingLinks']),
        filesWithAnAreaOver128LinksIn=sum(1 for _, row in rows if row['notes']['areasOver128Incoming']),
        largestTravelTimes32=most(lambda row: row['notes']['travelTimeBytes32']),
        largestTravelTimes64=most(lambda row: row['notes']['travelTimeBytes64']),
        withoutVertexes=sum(1 for _, row in rows if not row['counts']['vertexes']),
        nodesReachedTwice=sum(1 for _, row in rows if row['tree']['sharedNodes']),
        nodesNotReached=sum(1 for _, row in rows if row['tree']['unreachedNodes']),
        cycles=sum(1 for _, row in rows if row['tree']['cycle']),
        loaderRefusals=sum(1 for _, row in rows if row['loaderCheck'] != 'valid'),
        loaderLoadFailures=sum(1 for _, row in rows if row['loader']['result']),
    )


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
    files, totals, titles = {}, defaultdict(Counter), {}
    with tempfile.TemporaryDirectory(dir=ROOT / '.tmp') as scratch:
        scratch = Path(scratch)
        shim, loader = build_shim(scratch), build_loader(scratch)
        for title, path in sorted(installations.items()):
            served = {}
            for folder, where, entry in discover(title, path):
                if where == 'loose':
                    data = (path / folder / entry).read_bytes()
                else:
                    with zipfile.ZipFile(path / folder / where) as archive:
                        data = archive.read(entry)
                key = f'{title}:{folder}/{where}:{entry}'
                # later paks, then loose files, win for the same map
                served[(folder.lower(), entry.lower())] = key
                version, found, notes, extra = audit(data)
                fault, words = loader_check(shim, scratch, data)
                loaded = loader_load(loader, scratch, title, data)
                row = dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), version=version, fails=found,
                           notes=notes, loaderCheck=words if fault else 'valid', loader=loaded)
                if extra:
                    row.update(counts=extra['counts'], tree=extra['tree'])
                files[key] = row
                totals[title]['files'] += 1
                totals[title][f'version{version}'] += 1
                for name in found:
                    totals[title][name] += 1
                totals[title]['loaderRefusals'] += bool(fault)
                totals[title]['loaderLoadFailures'] += loaded['result'] != 0
            for key in served.values():
                files[key]['served'] = True
            titles[title] = summary([(key, row) for key, row in files.items() if key.startswith(title + ':')])
            print(title, dict(totals[title]), flush=True)
    failures = sum(1 for row in files.values() for name in row['fails'] if name in CHECKED)
    refusals = sum(1 for row in files.values() if row['loaderCheck'] != 'valid' or row['loader']['result'])
    report = dict(checks=sorted(CHECKED), counts={k: dict(v) for k, v in sorted(totals.items())},
                  titles=dict(sorted(titles.items())), measuredCheckFailures=failures, loaderRefusals=refusals,
                  files=files)
    args.output.write_text(json.dumps(report, indent=1) + '\n', encoding='utf-8')
    print("checks the loaders make that a file fails:", failures, "- files the loaders' own checks refuse:", refusals)
    return 1 if failures or refusals else 0


if __name__ == '__main__':
    raise SystemExit(main())
