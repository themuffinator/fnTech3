"""Model light from a Ritual map's compiled lights.

American McGee's Alice and Heavy Metal: F.A.K.K.2 light the models their game
modules flag RF_EXTRALIGHT (0x200) or RF_INVISIBLE (0x8000000) -- every
sentient and every weapon -- from the map's compiled lights, never from the
light grid. code/renderercommon/tr_ritual_model_light.h holds retail's law
for that, read from alice.exe and fakk2.exe 1.02, and fnTech3's fit of its six
directions of light to the ambient and directed light every renderer shades
a model with. See docs/fntech3/2026-09-25-ritual-model-lights.md.

Four things run here:

* the law and the map reader, on maps built in memory, against values worked
  out by hand, and the reader's refusals of malformed lumps;
* each renderer's own R_SetupAliceEntityLighting, sliced from its tr_light.c
  and compiled with the real render glue: a flagged model takes the compiled
  lights, and everything else -- an unflagged model, a full-bright one, a
  view without a world, `r_ritualModelLights 0`, a map without lights --
  keeps the path it had;
* the hunks the three renderers share, compared as text;
* when legitimate installations are present, every retail map's light lumps
  through the reader, and the law at every player start through both the C
  code and an independent Python model, traces aside. Set
  FNTECH3_ALICE_ROOT and FNTECH3_FAKK2_ROOT to point at other copies. Map
  bytes are piped to the host; nothing retail is written to disk.
"""
import math
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import unittest
import zipfile

from compiler_support import engine_fixture_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')
ALICE_ROOT = Path(os.environ.get('FNTECH3_ALICE_ROOT', r'E:\Games\Alice'))
FAKK2_ROOT = Path(os.environ.get('FNTECH3_FAKK2_ROOT', str(ROOT / '.tmp' / 'fakk2-install' / 'game')))


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2] + '\n'


def compile_and_run(test, source, stdin=None, args=()):
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        test.skipTest('C++17 compiler required')
    target = engine_fixture_args(compiler)
    if os.name != 'nt' and not target:
        target = []
    (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='ritual-model-lights-', dir=ROOT / '.tmp') as temp:
        cpp, exe = Path(temp) / 'probe.cpp', Path(temp) / 'probe.exe'
        cpp.write_text(source, encoding='utf-8')
        built = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                               cwd=ROOT, capture_output=True, text=True, timeout=120)
        test.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        run = subprocess.run([str(exe), *args], cwd=ROOT, input=stdin, capture_output=True, timeout=120)
        test.assertEqual(run.returncode, 0, (run.stdout + run.stderr).decode('utf-8', 'replace'))
        return run.stdout.decode('utf-8')


# A FAKK map in memory: the header, then each lump the reader looks at.
MAP_BUILDER = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "code/renderercommon/tr_ritual_model_light.h"

struct Light {
	float origin[3], color[3], intensity; int leaf, trace, spot; float dir[3], rbd, cap;
};
static void Put( std::vector<unsigned char> &out, const void *data, size_t size ) {
	const unsigned char *p = (const unsigned char *)data;
	out.insert( out.end(), p, p + size );
}
static std::vector<unsigned char> Map( int version, const std::vector<Light> &lights,
	const std::vector<int> &lists, const std::vector<int> &areas, const std::string &entities, bool vis,
	int lightBytes = -1 ) {
	const int record = version == 42 ? 60 : 56;
	std::vector<unsigned char> lumps[20];
	for ( const Light &l : lights ) {
		std::vector<unsigned char> &o = lumps[17];
		Put( o, l.origin, 12 ); Put( o, l.color, 12 ); Put( o, &l.intensity, 4 );
		Put( o, &l.leaf, 4 ); Put( o, &l.trace, 4 ); Put( o, &l.spot, 4 ); Put( o, l.dir, 12 ); Put( o, &l.rbd, 4 );
		if ( record == 60 ) Put( o, &l.cap, 4 );
	}
	if ( lightBytes >= 0 ) lumps[17].resize( (size_t)lightBytes );
	for ( int n : lists ) Put( lumps[18], &n, 4 );
	for ( int area : areas ) {
		int leaf[12] = { 0, area };
		Put( lumps[8], leaf, sizeof( leaf ) );
	}
	Put( lumps[14], entities.c_str(), entities.size() + 1 );
	if ( vis ) { int v[2] = { 1, 1 }; Put( lumps[15], v, 8 ); }
	std::vector<unsigned char> file;
	Put( file, "FAKK", 4 ); Put( file, &version, 4 ); int checksum = 0; Put( file, &checksum, 4 );
	int offset = 12 + 8 * 20;
	for ( int i = 0; i < 20; i++ ) {
		int len = (int)lumps[i].size();
		Put( file, &offset, 4 ); Put( file, &len, 4 );
		offset += len;
	}
	for ( int i = 0; i < 20; i++ ) Put( file, lumps[i].data(), lumps[i].size() );
	return file;
}
static void *Alloc( int size ) { return malloc( (size_t)size ); }
static ritualModelLights_t *Parse( const std::vector<unsigned char> &file, char *error = nullptr ) {
	char local[128];
	return RitualModelLights_Parse( file.data(), (int)file.size(), Alloc, error ? error : local, 128 );
}
static bool Near( float a, float b, float tolerance = 1e-3f ) { return std::fabs( a - b ) <= tolerance; }
'''

LAW_CHECKS = MAP_BUILDER + r'''
/* A world of two leaves split at x = 0 -- leaf 0 behind, leaf 1 in front --
 * with traces and sky under the test's control. */
struct World { int solid; int clear; int sky; int leafOverride; };
static int LeafAt( void *c, const float p[3] ) {
	const World *w = (const World *)c;
	if ( w->leafOverride >= 0 ) return w->leafOverride;
	return p[0] > 0 ? 1 : 0;
}
static int Clear( void *c, const float *, const float * ) { return ( (const World *)c )->clear; }
static int Sky( void *c, const float *, const float * ) { return ( (const World *)c )->sky; }

static ritualModelLightEntity_t Entity( float x, float y, float z, int fx = RITUAL_ML_RF_EXTRALIGHT ) {
	ritualModelLightEntity_t e;
	memset( &e, 0, sizeof( e ) );
	e.origin[0] = x; e.origin[1] = y; e.origin[2] = z - 32;
	for ( int k = 0; k < 3; k++ ) e.axis[k][k] = 1;
	e.point[0] = x; e.point[1] = y; e.point[2] = z;
	e.localPoint[2] = 32;
	e.reach = 16;
	e.renderfx = fx;
	return e;
}
static int Cube( const ritualModelLights_t *map, World &w, const ritualModelLightEntity_t &e, float cube[6][3],
	ritualModelLightStats_t *stats = nullptr, const unsigned char *areamask = nullptr, float shift = 1.0f ) {
	ritualModelLightWorld_t world;
	ritualModelLightStats_t local;
	world.context = &w; world.leafAt = LeafAt; world.clear = Clear; world.sky = Sky;
	world.areamask = areamask; world.areamaskBytes = areamask ? 32 : 0;
	return RitualModelLight_Cube( map, &world, &e, shift, cube, stats ? stats : &local );
}
static Light Point( float x, float y, float z, float intensity, int leaf = 1 ) {
	Light l;
	memset( &l, 0, sizeof( l ) );
	l.origin[0] = x; l.origin[1] = y; l.origin[2] = z;
	l.color[0] = 1; l.color[1] = 0.5f; l.color[2] = 0.25f;
	l.intensity = intensity; l.leaf = leaf;
	return l;
}
static const std::vector<int> AREAS = { 0, 1 };
static const std::string WORLDSPAWN = "{\n\"classname\" \"worldspawn\"\n}\n";

int main() {
	char error[128];
	/* ---- the reader ---- */
	{
		std::vector<Light> lights = { Point( 1, 2, 3, 300, 0 ), Point( 4, 5, 6, -50, 1 ), Point( 7, 8, 9, 20, 5 ) };
		lights[0].cap = 150; lights[0].trace = 1;
		lights[2].spot = 1; lights[2].dir[2] = -1; lights[2].rbd = -8;
		const std::string ents = "{ \"classname\" \"worldspawn\" \"ambientlight\" \"10 20 30\" // note\n"
			"\"suncolor\" \"1 2 3\" \"sundirection\" \"-90 0 0\" }\n{ \"ambientlight\" \"4 5 6\" }\n";
		ritualModelLights_t *map = Parse( Map( 42, lights, { 0, 2, -1, 1, -1 }, AREAS, ents, true ), error );
		assert( map && !error[0] );
		assert( map->version == 42 && map->numLights == 3 && map->numLeafs == 2 && map->numListed == 3 );
		assert( map->power == 30000.0f && map->leafLists == 1 && map->sun == 1 );
		/* the last `ambientlight` in the text wins, as retail's scan gives it */
		assert( map->ambient[0] == 4 && map->ambient[1] == 5 && map->ambient[2] == 6 );
		assert( map->sunColor[0] == 1 && map->sunColor[2] == 3 );
		assert( Near( map->sunDirection[0], 0 ) && Near( map->sunDirection[2], 1 ) );
		const ritualModelLight_t *l = RITUAL_ML_AT( map, ritualModelLight_t, ofsLights );
		assert( l[0].cap == 150 && l[0].needsTrace && !l[0].spot && l[0].area == 0 && l[0].intensity == 300 );
		assert( l[1].area == 1 && l[1].intensity == -50 );
		/* a light naming no leaf has no area, and so is never masked */
		assert( l[2].area == -1 && l[2].spot && l[2].spotRadiusByDistance == -8 && l[2].spotDirection[2] == -1 );
		const int *first = RITUAL_ML_AT( map, int, ofsLeafFirst ), *count = RITUAL_ML_AT( map, int, ofsLeafCount );
		const int *listed = RITUAL_ML_AT( map, int, ofsListed );
		assert( count[0] == 2 && listed[first[0]] == 0 && listed[first[0] + 1] == 2 );
		assert( count[1] == 1 && listed[first[1]] == 1 );
		free( map );

		/* F.A.K.K.2's own maps: 56-byte records, no cap, 7500 */
		map = Parse( Map( 12, { Point( 0, 0, 0, 10 ) }, { 0, -1, -1 }, AREAS, WORLDSPAWN, true ), error );
		assert( map && map->version == 12 && map->power == 7500.0f && map->numLights == 1 );
		assert( RITUAL_ML_AT( map, ritualModelLight_t, ofsLights )[0].cap == 0 && !map->sun );
		free( map );
		/* one terminator a leaf lists nothing; no visibility lists nothing */
		map = Parse( Map( 42, { Point( 0, 0, 0, 10 ) }, { -1, -1 }, AREAS, WORLDSPAWN, false ), error );
		assert( map && map->numListed == 0 && map->leafLists == 0 );
		free( map );
		map = Parse( Map( 42, {}, {}, AREAS, WORLDSPAWN, true ), error );
		assert( map && map->numLights == 0 && map->numListed == 0 );
		free( map );
	}
	/* ---- refusals ---- */
	{
		const Light one = Point( 0, 0, 0, 10 );
		struct Case { std::vector<unsigned char> file; const char *reason; };
		std::vector<Light> tooMany( 1533, one );
		std::vector<Case> cases = {
			{ Map( 42, { one }, { 0, -1 }, AREAS, WORLDSPAWN, true, 61 ), "whole number of records" },
			{ Map( 12, { one }, { 0, -1 }, AREAS, WORLDSPAWN, true, 60 ), "whole number of records" },
			{ Map( 42, tooMany, { -1, -1 }, AREAS, WORLDSPAWN, true ), "more than 1532" },
			{ Map( 42, { one }, { 1, -1, -1 }, AREAS, WORLDSPAWN, true ), "names no light" },
			{ Map( 42, { one }, { -2, -1, -1 }, AREAS, WORLDSPAWN, true ), "names no light" },
			{ Map( 42, { one }, { 0, -1, 0 }, AREAS, WORLDSPAWN, true ), "runs past" },
			{ Map( 42, { one }, { -1, -1, -1, -1 }, AREAS, WORLDSPAWN, true ), "outnumber" },
		};
		{
			std::vector<unsigned char> f = Map( 42, { one }, { 0, -1 }, AREAS, WORLDSPAWN, true );
			int bad = (int)f.size() + 4;
			memcpy( f.data() + 12 + 8 * 17, &bad, 4 );
			cases.push_back( { f, "outside the file" } );
			f = Map( 42, { one }, { 0, -1 }, AREAS, WORLDSPAWN, true );
			int odd = 47;
			memcpy( f.data() + 12 + 8 * 8 + 4, &odd, 4 );
			cases.push_back( { f, "leaf lump" } );
			f = Map( 42, { one }, { 0, -1 }, AREAS, WORLDSPAWN, true );
			int five = 5;
			memcpy( f.data() + 12 + 8 * 18 + 4, &five, 4 );
			cases.push_back( { f, "numbers" } );
		}
		for ( const Case &c : cases ) {
			error[0] = 0;
			ritualModelLights_t *map = Parse( c.file, error );
			if ( map || !strstr( error, c.reason ) ) fprintf( stderr, "refusal '%s' gave '%s'\n", c.reason, error );
			assert( !map && strstr( error, c.reason ) );
		}
		/* other formats are not this reader's, and say nothing */
		std::vector<unsigned char> f = Map( 42, { one }, { 0, -1 }, AREAS, WORLDSPAWN, true );
		memcpy( f.data(), "IBSP", 4 );
		assert( !Parse( f, error ) && !error[0] );
		f = Map( 42, { one }, { 0, -1 }, AREAS, WORLDSPAWN, true );
		int version = 46;
		memcpy( f.data() + 4, &version, 4 );
		assert( !Parse( f, error ) && !error[0] );
		f.resize( 100 );
		assert( !Parse( f, error ) && !error[0] );
	}
	/* ---- the law ---- */
	World w = { 0, 1, 0, -1 };
	float cube[6][3];
	ritualModelLightStats_t stats;
	{
		/* a point light 100 units along +X of the lighting point:
		 * power 30 x 30000 / 100^2 = 90; the +X side takes all of it, the
		 * sides the reach's spill, 16 / sqrt(100^2 + 16^2) of it */
		ritualModelLights_t *map = Parse( Map( 42, { Point( 120, 0, 32, 30 ) }, { -1, 0, -1 }, AREAS, WORLDSPAWN, true ) );
		const ritualModelLightEntity_t e = Entity( 20, 0, 32 );
		const float spill = 90.0f * 16.0f / std::sqrt( 100.0f * 100.0f + 16.0f * 16.0f );
		assert( Cube( map, w, e, cube, &stats ) == RITUAL_ML_LIT );
		assert( stats.leaves == 1 && stats.candidates == 1 && stats.lit == 1 );
		assert( Near( cube[0][0], 90 ) && Near( cube[0][1], 45 ) && Near( cube[0][2], 22.5f ) );
		assert( cube[1][0] == 0 && cube[1][2] == 0 );
		for ( int i = 2; i < 6; i++ ) assert( Near( cube[i][0], spill ) && Near( cube[i][2], spill * 0.25f ) );
		free( map );
		/* power below 1 is dropped */
		map = Parse( Map( 42, { Point( 120, 0, 32, 0.3f ) }, { -1, 0, -1 }, AREAS, WORLDSPAWN, true ) );
		assert( Cube( map, w, e, cube, &stats ) == RITUAL_ML_LIT && stats.lit == 0 && cube[0][0] == 0 );
		free( map );
		/* Alice's cap clamps the power; F.A.K.K.2 has none, and its constant is a quarter */
		std::vector<Light> capped = { Point( 120, 0, 32, 300 ) };
		capped[0].cap = 40;
		map = Parse( Map( 42, capped, { -1, 0, -1 }, AREAS, WORLDSPAWN, true ) );
		Cube( map, w, e, cube );
		assert( Near( cube[0][0], 40 ) && Near( cube[0][1], 20 ) );
		free( map );
		map = Parse( Map( 12, { Point( 120, 0, 32, 30 ) }, { -1, 0, -1 }, AREAS, WORLDSPAWN, true ) );
		Cube( map, w, e, cube );
		assert( Near( cube[0][0], 22.5f ) );
		free( map );
		/* a positive light skips an invisible entity; a negative one lights only it, by its size */
		map = Parse( Map( 42, { Point( 120, 0, 32, 30 ), Point( 120, 0, 32, -30 ) }, { -1, 0, 1, -1 }, AREAS,
			WORLDSPAWN, true ) );
		Cube( map, w, e, cube, &stats );
		assert( stats.lit == 1 && Near( cube[0][0], 90 ) );
		Cube( map, w, Entity( 20, 0, 32, RITUAL_ML_RF_INVISIBLE ), cube, &stats );
		assert( stats.lit == 1 && Near( cube[0][0], 90 ) );
		free( map );
		/* a light that needs a trace is dropped when it is blocked */
		std::vector<Light> traced = { Point( 120, 0, 32, 30 ) };
		traced[0].trace = 1;
		map = Parse( Map( 42, traced, { -1, 0, -1 }, AREAS, WORLDSPAWN, true ) );
		w.clear = 0;
		Cube( map, w, e, cube, &stats );
		assert( stats.lit == 0 && cube[0][0] == 0 );
		w.clear = 1;
		Cube( map, w, e, cube, &stats );
		assert( stats.lit == 1 );
		free( map );
		/* the view's area mask hides a listed light's area; with no visibility nothing is masked */
		unsigned char mask[32] = { 0x02 };
		map = Parse( Map( 42, { Point( 120, 0, 32, 30 ) }, { -1, 0, -1 }, AREAS, WORLDSPAWN, true ) );
		Cube( map, w, e, cube, &stats, mask );
		assert( stats.candidates == 0 && cube[0][0] == 0 );
		free( map );
		map = Parse( Map( 42, { Point( 120, 0, 32, 30 ) }, { -1, -1 }, AREAS, WORLDSPAWN, false ) );
		Cube( map, w, e, cube, &stats, mask );
		assert( stats.candidates == 1 && stats.lit == 1 );
		free( map );
		/* a light listed by two gathered leaves counts once: this point
		 * straddles x = 0, so its samples find both leaves */
		map = Parse( Map( 42, { Point( 120, 0, 32, 30 ) }, { 0, -1, 0, -1 }, AREAS, WORLDSPAWN, true ) );
		Cube( map, w, Entity( 0.5f, 0, 32 ), cube, &stats );
		assert( stats.leaves == 2 && stats.candidates == 1 && stats.lit == 1 );
		free( map );
	}
	{
		/* the added light, the ambient key, and the identity light */
		ritualModelLights_t *map = Parse( Map( 42, { Point( 120, 0, 32, 0.3f ) }, { -1, 0, -1 }, AREAS,
			"{ \"ambientlight\" \"3 4 5\" }", true ) );
		Cube( map, w, Entity( 20, 0, 32, RITUAL_ML_RF_EXTRALIGHT | RITUAL_ML_RF_MINLIGHT ), cube );
		for ( int i = 0; i < 6; i++ ) assert( cube[i][0] == 35 && cube[i][1] == 36 && cube[i][2] == 37 );
		ritualModelLightEntity_t offset = Entity( 20, 0, 32, RITUAL_ML_RF_EXTRALIGHT | RITUAL_ML_RF_LIGHTOFFSET
			| RITUAL_ML_RF_MINLIGHT );
		offset.lightOffset[0] = 100; offset.lightOffset[1] = 10; offset.lightOffset[2] = 1;
		Cube( map, w, offset, cube );
		assert( cube[3][0] == 103 && cube[3][1] == 14 && cube[3][2] == 6 );
		/* no leaf with an area around the entity: the identity light, plus what it adds */
		w.leafOverride = 5;
		assert( Cube( map, w, offset, cube, &stats ) == RITUAL_ML_IDENTITY && stats.leaves == 0 );
		assert( cube[0][0] == 355 && cube[5][2] == 256 );
		w.leafOverride = -1;
		free( map );
		/* no lights at all: the identity light */
		map = Parse( Map( 42, {}, {}, AREAS, WORLDSPAWN, true ) );
		assert( Cube( map, w, Entity( 20, 0, 32 ), cube ) == RITUAL_ML_IDENTITY && cube[2][1] == 255 );
		free( map );
	}
	{
		/* the sun: its colour by the cosine on the sides facing it, when it is seen */
		ritualModelLights_t *map = Parse( Map( 42, { Point( 120, 0, 32, 0.3f ) }, { -1, 0, -1 }, AREAS,
			"{ \"suncolor\" \"100 50 25\" \"sundirection\" \"-90 0 0\" }", true ) );
		w.sky = 1;
		Cube( map, w, Entity( 20, 0, 32 ), cube, &stats );
		assert( stats.sun == 1 && Near( cube[4][0], 100 ) && Near( cube[4][2], 25 ) && Near( cube[0][0], 0 ) );
		w.sky = 0;
		Cube( map, w, Entity( 20, 0, 32 ), cube, &stats );
		assert( stats.sun == 0 && cube[4][0] == 0 );
		free( map );
	}
	{
		/* a spot 100 units above, pointing down, with a radius of half its
		 * distance: full on the axis, fading over its last 32 units, nothing
		 * outside its cone or behind it */
		std::vector<Light> spot = { Point( 0, 0, 132, 30 ) };
		spot[0].spot = 1; spot[0].dir[2] = -1; spot[0].rbd = 0.5f;
		ritualModelLights_t *map = Parse( Map( 42, spot, { 0, -1, 0, -1 }, AREAS, WORLDSPAWN, true ) );
		Cube( map, w, Entity( 0, 0, 32 ), cube, &stats );
		assert( stats.lit == 1 && Near( cube[4][0], 90 ) && cube[5][0] == 0 && cube[0][0] == 0 );
		Cube( map, w, Entity( 40, 0, 32 ), cube, &stats );
		assert( Near( cube[4][0], 0.3125f * 30.0f * 30000.0f / 11600.0f, 1e-2f ) );
		Cube( map, w, Entity( 60, 0, 32 ), cube, &stats );
		assert( stats.lit == 0 && cube[4][0] == 0 );
		Cube( map, w, Entity( 0, 0, 232 ), cube, &stats );
		assert( stats.lit == 0 );
		free( map );
		/* a negative radius is a disc: the entity is lit from the disc's
		 * point nearest it, under a cone of 1.25 */
		spot[0].rbd = -20;
		map = Parse( Map( 42, spot, { 0, -1, 0, -1 }, AREAS, WORLDSPAWN, true ) );
		Cube( map, w, Entity( 100, 0, 32 ), cube, &stats );
		assert( stats.lit == 1 && Near( cube[4][0], 30.0f * 30000.0f / 16400.0f, 1e-2f ) );
		free( map );
	}
	{
		/* each side's brightest channel stops at 255, hue kept */
		ritualModelLights_t *map = Parse( Map( 42, { Point( 70, 0, 32, 300 ) }, { -1, 0, -1 }, AREAS, WORLDSPAWN, true ) );
		Cube( map, w, Entity( 20, 0, 32 ), cube );
		assert( Near( cube[0][0], 255 ) && Near( cube[0][1], 127.5f ) && Near( cube[0][2], 63.75f ) );
		free( map );
	}
	{
		/* shifted as the map's lightmaps are, before the clamp: F.A.K.K.2's
		 * two map bits make 90 a 360, kept to 255 with its hue */
		ritualModelLights_t *map = Parse( Map( 12, { Point( 120, 0, 32, 120 ) }, { -1, 0, -1 }, AREAS, WORLDSPAWN, true ) );
		Cube( map, w, Entity( 20, 0, 32 ), cube, nullptr, nullptr, 4.0f );
		assert( Near( cube[0][0], 255 ) && Near( cube[0][1], 127.5f ) && Near( cube[0][2], 63.75f ) );
		Cube( map, w, Entity( 20, 0, 32 ), cube, nullptr, nullptr, 0.5f );
		assert( Near( cube[0][0], 45 ) && Near( cube[0][1], 22.5f ) );
		free( map );
		/* the sun's colour is shifted as it is read, and again with the rest */
		map = Parse( Map( 12, { Point( 120, 0, 32, 0.1f ) }, { -1, 0, -1 }, AREAS,
			"{ \"suncolor\" \"10 5 2.5\" \"sundirection\" \"-90 0 0\" \"ambientlight\" \"1 2 3\" }", true ) );
		w.sky = 1;
		Cube( map, w, Entity( 20, 0, 32 ), cube, nullptr, nullptr, 4.0f );
		w.sky = 0;
		assert( Near( cube[4][0], 164 ) && Near( cube[4][1], 88 ) && Near( cube[4][2], 52 ) );
		assert( Near( cube[0][0], 4 ) && Near( cube[5][2], 12 ) );
		free( map );
	}
	/* ---- the fit ---- */
	{
		float ambient[3], directed[3], direction[3];
		float one[6][3] = {};
		one[0][0] = 90; one[0][1] = 45; one[0][2] = 22.5f;
		RitualModelLight_Fold( one, ambient, directed, direction );
		assert( Near( direction[0], 1 ) && Near( ambient[0], 0, 1e-3f ) && Near( directed[0], 90, 1e-2f ) );
		assert( Near( directed[2], 22.5f, 1e-2f ) );
		float down[6][3] = {};
		down[5][1] = 60;
		RitualModelLight_Fold( down, ambient, directed, direction );
		assert( Near( direction[2], -1 ) && Near( directed[1], 60, 1e-2f ) && Near( ambient[1], 0, 1e-3f ) );
		/* two opposite equal sides cancel: an even ambient of the mean */
		float pair[6][3] = {};
		pair[0][0] = pair[1][0] = 80;
		RitualModelLight_Fold( pair, ambient, directed, direction );
		assert( Near( direction[2], 1 ) && Near( directed[0], 0 ) && Near( ambient[0], 40, 1e-3f ) );
		/* whatever the six, the mean over all directions is kept */
		unsigned seed = 7;
		for ( int trial = 0; trial < 2000; trial++ ) {
			float c[6][3], sum = 0;
			for ( int i = 0; i < 6; i++ ) {
				for ( int k = 0; k < 3; k++ ) {
					seed = seed * 1103515245u + 12345u;
					c[i][k] = (float)( ( seed >> 8 ) % 256 ) * ( ( trial & 3 ) == 0 && i > 1 ? 0.0f : 1.0f );
				}
				sum += c[i][0];
			}
			RitualModelLight_Fold( c, ambient, directed, direction );
			assert( ambient[0] >= 0 && directed[0] >= 0 );
			assert( Near( ambient[0] + 0.25f * directed[0], 0.25f * sum, 0.05f ) );
			assert( Near( direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2], 1 ) );
		}
	}
	puts( "ok" );
	return 0;
}
'''


def renderer_fixture(renderer):
    source = (ROOT / 'code' / renderer / 'tr_light.c').read_text()
    fixture = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include "code/compat/alice_render_abi.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#define USE_PMLIGHT
#define DLIGHT_AT_RADIUS 16
#define DLIGHT_MINIMUM_RADIUS 16
enum { RF_MINLIGHT = 1, RF_LIGHTING_ORIGIN = 128, RDF_NOWORLDMODEL = 1 };
struct dlight_t { vec3_t origin, color; float radius; bool linear; void *rtcwShader; float ritualRadius; vec3_t ritualColor; };
struct trRefdef_t { int rdflags, time, num_dlights; dlight_t *dlights; byte areamask[32]; };
struct trRefEntity_t {
	struct {
		int renderfx; vec3_t origin, lightingOrigin, axis[3];
		struct { int renderfx; } alice;
		color4ub_t shader;
	} e;
	qboolean lightingCalculated; vec3_t ambientLight, directedLight, lightDir, shadowLightDir, csmSunLight; int ambientLightInt;
};
struct mnode_t { int contents; cplane_t *plane; mnode_t *children[2]; };
#include "code/renderercommon/tr_ritual_model_light.h"
struct model_t { char name[64]; };
static struct {
	const void *lightGridData; mnode_t *nodes; int numDecisionNodes; const ritualModelLights_t *ritualModelLights;
} world;
static model_t tiki = { "models/test.tik" };
static struct {
	decltype( world ) *world; float identityLight; int identityLightByte; vec3_t sunDirection; model_t *currentModel;
	int overbrightBits;
	struct { qboolean valid, unitColour; vec3_t directionToSun, color, light; } worldSun;
} tr = { &world, 1, 255, { 0, 0, 1 }, &tiki, 0, {} };
static int traces, prints;
static void Trace( trace_t *t, const vec3_t, const vec3_t, const vec3_t, const vec3_t, clipHandle_t, int, qboolean ) {
	++traces; memset( t, 0, sizeof( *t ) ); t->fraction = 1.0f;
}
static void QDECL Print( printParm_t, const char *, ... ) { ++prints; }
static struct {
	comGameId_t gameId;
	void ( *CM_BoxTrace )( trace_t *, const vec3_t, const vec3_t, const vec3_t, const vec3_t, clipHandle_t, int, qboolean );
	void ( QDECL *Printf )( printParm_t, const char *, ... );
} ri = { COM_GAME_ALICE, Trace, Print };
static cvar_t mode{}, shadow{}, debug{}, compiled{}, compiledDebug{}, mapBits{}, csm{};
static cvar_t *r_dlightMode = &mode, *r_shadows = &shadow, *r_debugLight = &debug, *r_mapOverBrightBits = &mapBits;
static cvar_t *r_csmShadows = &csm;
cvar_t *r_ritualModelLights = &compiled, *r_ritualModelLightsDebug = &compiledDebug;
static int R_GetDlightMode( void ) { return mode.integer; }
static int gridCalls, logCalls;
static void LogLight( const trRefEntity_t * ) { ++logCalls; }
static void R_SetupEntityLightingGrid( trRefEntity_t *entity ) {
	++gridCalls; VectorSet( entity->ambientLight, 10, 20, 30 ); VectorSet( entity->directedLight, 40, 50, 60 );
	VectorSet( entity->lightDir, 0, 0, 1 );
}
float VectorNormalize( vec3_t v ) {
	const float length = VectorLength( v );
	if ( length ) for ( int axis = 0; axis < 3; ++axis ) v[axis] /= length;
	return length;
}
float VectorNormalize2( const vec3_t in, vec3_t out ) {
	VectorCopy( in, out );
	return VectorNormalize( out );
}
float Com_Clamp( float low, float high, float value ) {
	return value < low ? low : value > high ? high : value;
}
#define myftol( x ) ( (int)( x ) )
#include "code/renderercommon/tr_csm_model_light.h"
#include "code/renderercommon/tr_ritual_model_light_render.h"
'''
    fixture += function(source, 'void R_SetupAliceEntityLighting(')
    return fixture


RENDERER_CHECKS = r'''
#include <vector>
#include <string>
static std::vector<unsigned char> Map( const std::vector<float> &light ) {
	/* two leaves, both with area 0, each listing the one light */
	std::vector<unsigned char> lumps[20];
	auto put = []( std::vector<unsigned char> &o, const void *d, size_t n ) {
		o.insert( o.end(), (const unsigned char *)d, (const unsigned char *)d + n );
	};
	put( lumps[17], light.data(), light.size() * 4 );
	const int lists[4] = { 0, -1, 0, -1 };
	put( lumps[18], lists, sizeof( lists ) );
	int leaf[12] = {};
	put( lumps[8], leaf, sizeof( leaf ) ); put( lumps[8], leaf, sizeof( leaf ) );
	const char ents[] = "{ \"classname\" \"worldspawn\" }";
	put( lumps[14], ents, sizeof( ents ) );
	put( lumps[15], lists, 8 );
	std::vector<unsigned char> file;
	put( file, "FAKK", 4 ); int version = 42, zero = 0; put( file, &version, 4 ); put( file, &zero, 4 );
	int offset = 12 + 160;
	for ( auto &l : lumps ) { int n = (int)l.size(); put( file, &offset, 4 ); put( file, &n, 4 ); offset += n; }
	for ( auto &l : lumps ) put( file, l.data(), l.size() );
	return file;
}
static void *Alloc( int size ) { return malloc( (size_t)size ); }
static bool Near( float a, float b, float t = 1e-3f ) { return std::fabs( a - b ) <= t; }

int main() {
	/* a light 100 units along +X of the lighting point, intensity 30: 90 on that side */
	const std::vector<float> record = { 100, 0, 32, 1, 0.5f, 0.25f, 30, 0, 0, 0, 0, 0, 0, 0, 0 };
	const std::vector<unsigned char> file = Map( record );
	char error[128];
	ritualModelLights_t *map = RitualModelLights_Parse( file.data(), (int)file.size(), Alloc, error, 128 );
	assert( map );
	cplane_t plane = {};
	plane.normal[0] = 1;
	mnode_t nodes[3] = {};
	nodes[0].contents = -1; nodes[0].plane = &plane; nodes[0].children[0] = &nodes[2]; nodes[0].children[1] = &nodes[1];
	world.nodes = nodes; world.numDecisionNodes = 1; world.ritualModelLights = map;
	static const byte grid[8] = { 1, 1, 1, 1, 1, 1, 0, 0 };
	world.lightGridData = grid;
	compiled.integer = 1;
	csm.integer = 1;
	tr.worldSun.valid = qtrue;
	VectorSet( tr.worldSun.directionToSun, 1, 0, 0 );
	VectorSet( tr.worldSun.light, 30, 20, 10 );
	trRefdef_t view = {};
	const float localPoint[3] = { 0, 0, 32 };
	vec3_t point = { 0, 0, 32 };
	auto Fresh = [&]( int fx ) {
		trRefEntity_t e = {};
		for ( int k = 0; k < 3; k++ ) e.e.axis[k][k] = 1;
		e.e.alice.renderfx = fx;
		return e;
	};

	/* a flagged model takes the compiled lights, fitted, and not the grid */
	trRefEntity_t e = Fresh( FNTECH3_ALICE_RF_SHADOW | 0x200 );
	R_SetupAliceEntityLighting( &view, &e, point, localPoint, 0 );
	assert( gridCalls == 0 && Near( e.directedLight[0], 90, 0.05f ) && Near( e.directedLight[1], 45, 0.05f ) );
	assert( Near( e.ambientLight[0], 0, 0.05f ) && Near( e.lightDir[0], 1 ) );
	/* The sun budget is captured without changing the compiled lighting. */
	assert( Near( e.csmSunLight[0], 30 ) && Near( e.csmSunLight[1], 20 ) && Near( e.csmSunLight[2], 10 ) );
	assert( traces == 0 );
	/* turned a quarter to the left, the light is on its right: the model's
	 * own frame still carries the direction back */
	e = Fresh( 0x200 );
	e.e.axis[0][0] = 0; e.e.axis[0][1] = 1; e.e.axis[1][0] = -1; e.e.axis[1][1] = 0;
	R_SetupAliceEntityLighting( &view, &e, point, localPoint, 0 );
	assert( gridCalls == 0 && Near( e.lightDir[1], -1 ) && Near( e.directedLight[0], 90, 0.05f ) );
	/* shifted as the lightmaps are: a bit down at one overbright bit, before
	 * a doubled output, and two bits up at F.A.K.K.2's map bits, where 360
	 * is kept to 255, hue and all */
	tr.overbrightBits = 1; tr.identityLight = 0.5f; tr.identityLightByte = 127;
	e = Fresh( 0x200 );
	R_SetupAliceEntityLighting( &view, &e, point, localPoint, 0 );
	assert( Near( e.directedLight[0], 45, 0.05f ) && Near( e.directedLight[1], 22.5f, 0.05f ) );
	tr.overbrightBits = 0; tr.identityLight = 1; tr.identityLightByte = 255;
	mapBits.integer = 2;
	e = Fresh( 0x200 );
	R_SetupAliceEntityLighting( &view, &e, point, localPoint, 0 );
	assert( Near( e.directedLight[0], 255, 0.05f ) && Near( e.directedLight[1], 127.5f, 0.05f ) );
	mapBits.integer = 0;
	/* minimum light is part of the compiled light, not added again */
	e = Fresh( 0x200 | FNTECH3_ALICE_RF_MINLIGHT );
	R_SetupAliceEntityLighting( &view, &e, point, localPoint, 0 );
	float cube[6][3] = {};
	cube[0][0] = 90; cube[0][1] = 45; cube[0][2] = 22.5f;
	for ( auto &side : cube ) for ( float &c : side ) c += 32;
	float ambient[3], directed[3], direction[3];
	RitualModelLight_Fold( cube, ambient, directed, direction );
	assert( Near( e.ambientLight[0], ambient[0], 0.05f ) && Near( e.directedLight[0], directed[0], 0.05f ) );
	/* an invisible model is lit by negative lights only: this one stays dark */
	e = Fresh( 0x8000000 );
	R_SetupAliceEntityLighting( &view, &e, point, localPoint, 0 );
	assert( gridCalls == 0 && e.ambientLight[0] == 0 && e.directedLight[0] == 0 );
	/* the debug print changes nothing, and compares the grid */
	compiledDebug.integer = 1; view.time = 5000;
	e = Fresh( 0x200 );
	R_SetupAliceEntityLighting( &view, &e, point, localPoint, 0 );
	assert( prints == 4 && gridCalls == 1 && Near( e.directedLight[0], 90, 0.05f ) );
	compiledDebug.integer = 0; gridCalls = 0;

	/* everything else keeps the grid, as before */
	e = Fresh( FNTECH3_ALICE_RF_SHADOW );
	R_SetupAliceEntityLighting( &view, &e, point, localPoint, 0 );
	assert( gridCalls == 1 && e.ambientLight[0] == 10 && e.directedLight[2] == 60 );
	compiled.integer = 0;
	e = Fresh( 0x200 );
	R_SetupAliceEntityLighting( &view, &e, point, localPoint, 0 );
	assert( gridCalls == 2 );
	compiled.integer = 1;
	e = Fresh( 0x200 );
	R_SetupAliceEntityLighting( &view, &e, point, nullptr, 0 );
	assert( gridCalls == 3 );
	world.ritualModelLights = nullptr;
	e = Fresh( 0x200 );
	R_SetupAliceEntityLighting( &view, &e, point, localPoint, 0 );
	assert( gridCalls == 4 );
	world.ritualModelLights = map;
	/* a full-bright model keeps the flat light, and so does a view with no world */
	VectorCopy( tr.sunDirection, tr.worldSun.directionToSun );
	e = Fresh( 0x200 | FNTECH3_ALICE_RF_FULLBRIGHT );
	R_SetupAliceEntityLighting( &view, &e, point, localPoint, 0 );
	assert( gridCalls == 4 && e.ambientLight[0] == 150 && e.directedLight[0] == 150 );
	assert( e.csmSunLight[0] == 0 && e.csmSunLight[1] == 0 && e.csmSunLight[2] == 0 );
	view.rdflags = RDF_NOWORLDMODEL;
	e = Fresh( 0x200 );
	R_SetupAliceEntityLighting( &view, &e, point, localPoint, 0 );
	assert( gridCalls == 4 && e.ambientLight[0] == 0 && e.directedLight[0] == 0 );
	/* and the lighting is worked out once a frame */
	view.rdflags = 0;
	e = Fresh( 0x200 );
	R_SetupAliceEntityLighting( &view, &e, point, localPoint, 0 );
	const trRefEntity_t saved = e;
	world.ritualModelLights = nullptr;
	R_SetupAliceEntityLighting( &view, &e, point, localPoint, 0 );
	assert( !memcmp( &saved, &e, sizeof( e ) ) && gridCalls == 4 );

	/* A light its model holds: retail's rule. F.A.K.K.2's dung pile light,
	 * 96 across and a quarter green, ten units from where the model is lit,
	 * inside a frame 40 across: 7500 * 96 / 100 = 7200 times its colour, past
	 * 255, so the directed light comes down to 255 green and the ambient with
	 * it, in either light mode, without turning the light's direction. */
	tr.worldSun.valid = qfalse;
	ri.gameId = COM_GAME_FAKK2;
	dlight_t light = {};
	VectorSet( light.origin, 10, 0, 32 ); VectorSet( light.ritualColor, 0, 0.25f, 0 );
	/* the renderer's own colour, desaturated, is not the one retail weighs */
	VectorSet( light.color, 0.5f, 0.2f, 0.5f );
	light.radius = 48; light.ritualRadius = 96;
	view.num_dlights = 1; view.dlights = &light;
	const float down = 255.0f / 1850.0f;
	for ( int lightMode = 0; lightMode <= 2; lightMode += 2 ) {
		mode.integer = lightMode;
		e = Fresh( FNTECH3_ALICE_RF_SHADOW );
		R_SetupAliceEntityLighting( &view, &e, point, localPoint, 20 );
		assert( Near( e.directedLight[1], 255, 0.05f ) && Near( e.directedLight[0], 40 * down, 0.01f ) );
		assert( Near( e.directedLight[2], 60 * down, 0.01f ) && Near( e.ambientLight[2], 30 * down, 0.01f ) );
		assert( Near( e.lightDir[2], 1 ) && ( ( e.ambientLightInt >> 16 ) & 0xff ) == (int)( 30 * down ) );
	}
	/* a dim one is added without the rescale; Alice's rule is four times it */
	VectorSet( light.ritualColor, 0, 0.001f, 0 );
	e = Fresh( FNTECH3_ALICE_RF_SHADOW );
	R_SetupAliceEntityLighting( &view, &e, point, localPoint, 20 );
	assert( Near( e.directedLight[1], 57.2f, 0.01f ) && e.ambientLight[1] == 20 && e.directedLight[0] == 40 );
	ri.gameId = COM_GAME_ALICE;
	e = Fresh( FNTECH3_ALICE_RF_SHADOW );
	R_SetupAliceEntityLighting( &view, &e, point, localPoint, 20 );
	assert( Near( e.directedLight[1], 78.8f, 0.01f ) );
	/* with an overbright bit the light is halved and so is the ceiling */
	ri.gameId = COM_GAME_FAKK2;
	tr.identityLight = 0.5f; tr.identityLightByte = 127;
	e = Fresh( FNTECH3_ALICE_RF_SHADOW );
	R_SetupAliceEntityLighting( &view, &e, point, localPoint, 20 );
	assert( Near( e.directedLight[1], 53.6f, 0.01f ) );
	VectorSet( light.ritualColor, 0, 0.25f, 0 );
	e = Fresh( FNTECH3_ALICE_RF_SHADOW );
	R_SetupAliceEntityLighting( &view, &e, point, localPoint, 20 );
	assert( Near( e.directedLight[1], 127, 0.05f ) );
	tr.identityLight = 1; tr.identityLightByte = 255;
	/* a light at the lighting point itself is weighed at a unit */
	VectorCopy( point, light.origin ); VectorSet( light.ritualColor, 1e-6f, 0, 0 );
	e = Fresh( FNTECH3_ALICE_RF_SHADOW );
	R_SetupAliceEntityLighting( &view, &e, point, localPoint, 20 );
	assert( Near( e.directedLight[0], 40.72f, 0.01f ) );
	/* farther than its radius, a light the model holds adds nothing, and the
	 * renderer's own vertex light leaves it out too */
	VectorSet( light.origin, 10, 0, 32 ); VectorSet( light.ritualColor, 0, 0.25f, 0 ); light.ritualRadius = 5;
	for ( int lightMode = 0; lightMode <= 2; lightMode += 2 ) {
		mode.integer = lightMode;
		e = Fresh( FNTECH3_ALICE_RF_SHADOW );
		R_SetupAliceEntityLighting( &view, &e, point, localPoint, 20 );
		assert( e.directedLight[1] == 50 && e.ambientLight[1] == 20 );
	}
	/* Outside the frame, with no frame, or a light no client game gave (a
	 * map's static light): the renderer's own lights, as before -- the
	 * vertex light turns toward it, the per-pixel one adds nothing here. */
	light.ritualRadius = 96; VectorSet( light.color, 0, 0.25f, 0 );
	for ( int k = 0; k < 3; k++ ) {
		const float reach = k == 0 ? 4.0f : k == 1 ? 0.0f : 20.0f;
		light.ritualRadius = k == 2 ? 0.0f : 96.0f;
		mode.integer = 0;
		e = Fresh( FNTECH3_ALICE_RF_SHADOW );
		R_SetupAliceEntityLighting( &view, &e, point, localPoint, reach );
		assert( Near( e.directedLight[1], 50 + 16 * 48 * 48 / 256.0f * 0.25f, 0.05f ) && e.lightDir[0] > 0.1f );
		assert( e.ambientLight[1] == 20 );
		mode.integer = 2;
		e = Fresh( FNTECH3_ALICE_RF_SHADOW );
		R_SetupAliceEntityLighting( &view, &e, point, localPoint, reach );
		assert( e.directedLight[1] == 50 && Near( e.lightDir[2], 1 ) );
	}
	mode.integer = 0; view.num_dlights = 0; view.dlights = nullptr; ri.gameId = COM_GAME_ALICE;
	puts( "ok" );
	return 0;
}
'''


# ---- an independent model of the law, for the retail maps ----------------

FACES = [(1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1)]
SAMPLES = [(0, 0, 0), (0, 0, 1), (1, 0, 0), (0, 1, 0), (-1, 0, 0), (0, -1, 0), (1, 1, 0), (-1, -1, 0), (0, 0, -1)]


def lumps(data):
    return [struct.unpack_from('<ii', data, 12 + 8 * i) for i in range(20)]


def lump(data, table, i):
    offset, length = table[i]
    return data[offset:offset + length]


class PyMap:
    def __init__(self, data):
        self.version = struct.unpack_from('<i', data, 4)[0]
        table = lumps(data)
        record = 60 if self.version == 42 else 56
        self.power = 30000.0 if self.version == 42 else 7500.0
        planes = lump(data, table, 1)
        self.planes = [struct.unpack_from('<4f', planes, i) for i in range(0, len(planes), 16)]
        nodes = lump(data, table, 9)
        self.nodes = [struct.unpack_from('<3i', nodes, i) for i in range(0, len(nodes), 36)]
        leafs = lump(data, table, 8)
        self.areas = [struct.unpack_from('<i', leafs, i + 4)[0] for i in range(0, len(leafs), 48)]
        self.vis = table[15][1] > 0
        raw = lump(data, table, 17)
        self.lights = []
        for i in range(0, len(raw), record):
            origin = struct.unpack_from('<3f', raw, i)
            color = struct.unpack_from('<3f', raw, i + 12)
            intensity, leaf, trace, spot = struct.unpack_from('<fiii', raw, i + 24)
            direction = struct.unpack_from('<3f', raw, i + 40)
            rbd = struct.unpack_from('<f', raw, i + 52)[0]
            cap = struct.unpack_from('<f', raw, i + 56)[0] if record == 60 else 0.0
            area = self.areas[leaf] if 0 <= leaf < len(self.areas) else -1
            self.lights.append((origin, color, intensity, trace, spot, direction, rbd, cap, area))
        ints = [n for (n,) in struct.iter_unpack('<i', lump(data, table, 18))]
        self.lists = [[] for _ in self.areas]
        if len(ints) != len(self.areas):
            pos = 0
            for leaf in range(len(self.areas)):
                if pos >= len(ints):
                    break
                while ints[pos] != -1:
                    self.lists[leaf].append(ints[pos])
                    pos += 1
                pos += 1
        text = lump(data, table, 14).split(b'\0')[0].decode('latin-1')
        tokens = re.findall(r'"([^"]*)"|(\S+)', text)
        tokens = [a or b for a, b in tokens]
        self.ambient = [0.0, 0.0, 0.0]
        for i, token in enumerate(tokens[:-1]):
            if token == 'ambientlight':
                values = tokens[i + 1].split()[:3]
                for k, v in enumerate(values):
                    self.ambient[k] = float(v)
        self.entities = text

    def leaf_at(self, p):
        n = 0
        while n >= 0:
            plane_num, front, back = self.nodes[n]
            a, b, c, dist = self.planes[plane_num]
            n = front if a * p[0] + b * p[1] + c * p[2] - dist > 0 else back
        return -n - 1

    def cube(self, point, reach):
        leaves = []
        for s in SAMPLES:
            leaf = self.leaf_at([point[k] + reach * s[k] for k in range(3)])
            if self.areas[leaf] != -1 and leaf not in leaves and len(leaves) < 4:
                leaves.append(leaf)
        if not self.lights or not leaves:
            return 'identity', [[255.0] * 3 for _ in FACES]
        cube = [list(self.ambient) for _ in FACES]
        candidates = []
        for leaf in (leaves if self.vis else [None]):
            for n in (self.lists[leaf] if self.vis else range(len(self.lights))):
                if n not in candidates:
                    candidates.append(n)
        for n in candidates:
            origin, color, intensity, trace, spot, direction, rbd, cap, area = self.lights[n]
            if intensity <= 0:
                continue
            if spot:
                source = list(origin)
                cone = rbd
                if cone < 0:
                    t = sum((origin[k] - point[k]) * direction[k] for k in range(3))
                    offset = [point[k] + t * direction[k] - origin[k] for k in range(3)]
                    length2 = sum(x * x for x in offset)
                    if length2 > cone * cone:
                        offset = [x * -cone / math.sqrt(length2) for x in offset]
                    source = [origin[k] + offset[k] for k in range(3)]
                    cone = 1.25
                v = [source[k] - point[k] for k in range(3)]
                d2 = max(1.0, sum(x * x for x in v))
                power = intensity * self.power / d2
                if power < 1:
                    continue
                along = -sum(direction[k] * v[k] for k in range(3))
                if along < -reach or along < 0:
                    continue
                q = [v[k] + along * direction[k] for k in range(3)]
                off = math.sqrt(sum(x * x for x in q))
                radius = along * cone
                if off >= radius:
                    continue
                fade = 1.0 if off <= radius - 32 else (radius - off) / 32
                for i, face in enumerate(FACES):
                    facing = -sum(direction[k] * face[k] for k in range(3))
                    if facing >= 0 and fade * facing * power > 0:
                        cube[i] = [cube[i][c] + fade * facing * power * color[c] for c in range(3)]
                continue
            v = [origin[k] - point[k] for k in range(3)]
            d2 = max(1.0, sum(x * x for x in v))
            power = abs(intensity) * self.power / d2
            if power < 1:
                continue
            if cap and power > cap:
                power = cap
            for i, face in enumerate(FACES):
                t = [v[k] + reach * face[k] for k in range(3)]
                length = math.sqrt(sum(x * x for x in t))
                w = sum(t[k] * face[k] for k in range(3)) / length if length else -1
                if w >= 0:
                    cube[i] = [cube[i][c] + w * power * color[c] for c in range(3)]
        shift = 1.0 if self.version == 42 else 4.0
        for i in range(6):
            cube[i] = [c * shift for c in cube[i]]
            peak = max(cube[i])
            if peak > 255:
                cube[i] = [c * 255 / peak for c in cube[i]]
        return 'lit', cube


def starts(text):
    out = []
    for block in re.findall(r'\{[^{}]*\}', text):
        pairs = dict(re.findall(r'"([^"]+)"\s+"([^"]*)"', block))
        if pairs.get('classname', '').startswith('info_player_start') and 'origin' in pairs:
            out.append([float(x) for x in pairs['origin'].split()[:3]])
    return out


RETAIL_HOST = MAP_BUILDER + r'''
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
/* reads a map on stdin and points on the command line, x y z reach at a
 * time, and prints the reader's counts and the law with traces clear and no
 * sky, as the Python model assumes */
struct Nodes { std::vector<float> planes; std::vector<int> nodes; };
static int LeafAt( void *c, const float p[3] ) {
	const Nodes *n = (const Nodes *)c;
	int i = 0;
	while ( i >= 0 ) {
		const float *pl = &n->planes[4 * n->nodes[9 * i]];
		i = pl[0] * p[0] + pl[1] * p[1] + pl[2] * p[2] - pl[3] > 0 ? n->nodes[9 * i + 1] : n->nodes[9 * i + 2];
	}
	return -i - 1;
}
static int Clear( void *, const float *, const float * ) { return 1; }
static int NoSky( void *, const float *, const float * ) { return 0; }
int main( int argc, char **argv ) {
#ifdef _WIN32
	_setmode( _fileno( stdin ), _O_BINARY );
#endif
	std::vector<unsigned char> file;
	unsigned char chunk[65536];
	for ( size_t got; ( got = fread( chunk, 1, sizeof( chunk ), stdin ) ) > 0; ) file.insert( file.end(), chunk, chunk + got );
	char error[128];
	ritualModelLights_t *map = RitualModelLights_Parse( file.data(), (int)file.size(), Alloc, error, 128 );
	if ( !map ) { printf( "refused %s\n", error ); return 0; }
	printf( "map %d %d %d %d %d %g %g %g %d\n", map->version, map->numLights, map->numLeafs, map->numListed,
		map->leafLists, map->ambient[0], map->ambient[1], map->ambient[2], map->sun );
	Nodes n;
	int offset, length;
	memcpy( &offset, file.data() + 12 + 8, 4 ); memcpy( &length, file.data() + 12 + 8 + 4, 4 );
	n.planes.resize( length / 4 ); memcpy( n.planes.data(), file.data() + offset, length );
	memcpy( &offset, file.data() + 12 + 72, 4 ); memcpy( &length, file.data() + 12 + 72 + 4, 4 );
	n.nodes.resize( length / 4 ); memcpy( n.nodes.data(), file.data() + offset, length );
	ritualModelLightWorld_t world = { &n, LeafAt, Clear, NoSky, nullptr, 0 };
	for ( int i = 1; i + 3 < argc; i += 4 ) {
		ritualModelLightEntity_t e;
		memset( &e, 0, sizeof( e ) );
		for ( int k = 0; k < 3; k++ ) { e.axis[k][k] = 1; e.point[k] = e.origin[k] = (float)atof( argv[i + k] ); }
		e.reach = (float)atof( argv[i + 3] );
		e.renderfx = RITUAL_ML_RF_EXTRALIGHT;
		float cube[6][3];
		ritualModelLightStats_t stats;
		/* each game's shift at its defaults: none, and two map bits */
		const int result = RitualModelLight_Cube( map, &world, &e, map->version == 42 ? 1.0f : 4.0f, cube, &stats );
		printf( "cube %s", result == RITUAL_ML_LIT ? "lit" : "identity" );
		for ( auto &side : cube ) printf( " %.4f %.4f %.4f", side[0], side[1], side[2] );
		printf( "\n" );
	}
	return 0;
}
'''


def retail_maps(pak, version):
    if not pak.is_file():
        return []
    with zipfile.ZipFile(pak) as z:
        return [(n, z.read(n)) for n in sorted(z.namelist())
                if n.lower().startswith('maps/') and n.lower().endswith('.bsp')
                and z.read(n)[:4] == b'FAKK' and struct.unpack_from('<i', z.read(n), 4)[0] == version]


class RitualModelLightTests(unittest.TestCase):
    def test_law_reader_and_fit(self):
        self.assertEqual(compile_and_run(self, LAW_CHECKS).strip(), 'ok')

    def test_renderers_light_flagged_models_from_compiled_lights(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                self.assertEqual(compile_and_run(self, renderer_fixture(renderer) + RENDERER_CHECKS).strip(), 'ok')

    def test_renderers_share_the_hunks(self):
        light = [(ROOT / 'code' / r / 'tr_light.c').read_text() for r in RENDERERS]
        alice = [(ROOT / 'code' / r / 'tr_alice.c').read_text() for r in RENDERERS]
        bsp = [(ROOT / 'code' / r / 'tr_bsp.c').read_text() for r in RENDERERS]
        init = [(ROOT / 'code' / r / 'tr_init.c').read_text() for r in RENDERERS]
        local = [(ROOT / 'code' / r / 'tr_local.h').read_text() for r in RENDERERS]
        for text in light:
            self.assertEqual(function(text, 'void R_SetupAliceEntityLighting('),
                             function(light[0], 'void R_SetupAliceEntityLighting('))
            self.assertIn('#include "../renderercommon/tr_ritual_model_light_render.h"', text)
        for text in alice:
            call = re.search(r'if \( R_AliceTikiLightOrigin\( ent, mod, lightOrigin, lightPoint \) \) \{.*?\n\t\t\}\n', text, re.S)
            self.assertIsNotNone(call)
            self.assertIn('RA_TikiLodSphere', call[0])
            self.assertIn('lightPoint, 0.5f * radius', call[0])
            self.assertIn('lightOrigin, NULL, 0.0f', text)
        for text in bsp:
            fakk = text.index('if ( bspFormat == COM_GAME_BSP_FAKK ) {')
            read = text.index('R_RitualModelLightsRead( buffer.b, size, name );')
            free_file = text.index('ri.FS_FreeFile( buffer.v );\n\t\tbuffer.v = fakkTranslated.data;')
            self.assertLess(fakk, read)
            self.assertLess(read, free_file)
            keep = text.index('R_RitualModelLightsKeep();')
            self.assertLess(text.index('R_LoadLightGrid( &header->lumps[LUMP_LIGHTGRID] );'), keep)
            self.assertLess(keep, text.index('tr.world = &s_worldData;'))
            self.assertIn('if ( bspFormat == COM_GAME_BSP_FAKK ) {\n\t\tR_RitualModelLightsKeep();', text)
        for text in init:
            block = re.search(r'if \( RITUAL_ML_PROFILE\( ri\.gameId \) \) \{\n\t\tr_ritualModelLights = .*?\n\t\}\n', text, re.S)
            self.assertIsNotNone(block)
            self.assertIn('"r_ritualModelLights", "1", CVAR_ARCHIVE_ND', block[0])
            self.assertIn('"r_ritualModelLightsDebug", "0", CVAR_CHEAT', block[0])
        for text in local:
            self.assertIn('const ritualModelLights_t *ritualModelLights;', text)
            self.assertIn('const float *localPoint, float reach );', text)
        # the only caller is the Ritual profiles' TIKI path
        for r in RENDERERS:
            for path in (ROOT / 'code' / r).glob('*.c'):
                if path.name in ('tr_light.c', 'tr_alice.c'):
                    continue
                self.assertNotIn('R_SetupAliceEntityLighting(', path.read_text(errors='replace'), path)

    def test_retail_maps_through_the_reader_and_the_law(self):
        # (pak, version, maps, compiled lights, player starts)
        sources = [(ALICE_ROOT / 'base' / 'pak0.pk3', 42, 36, 10184, 39),
                   (FAKK2_ROOT / 'fakk' / 'pak0.pk3', 12, 30, 10267, 36)]
        present = [s for s in sources if s[0].is_file()]
        if not present:
            self.skipTest('no legitimate Alice or F.A.K.K.2 installation')
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='ritual-model-lights-', dir=ROOT / '.tmp') as temp:
            cpp, exe = Path(temp) / 'host.cpp', Path(temp) / 'host.exe'
            cpp.write_text(RETAIL_HOST, encoding='utf-8')
            built = subprocess.run([compiler, *engine_fixture_args(compiler), '-std=c++17', '-O2', '-I', str(ROOT),
                                    str(cpp), '-o', str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            for pak, version, expected, lights, players in present:
                maps = retail_maps(pak, version)
                self.assertEqual(len(maps), expected, pak)
                records = compared = 0
                for name, data in maps:
                    with self.subTest(map=name):
                        model = PyMap(data)
                        points = starts(model.entities)
                        args = []
                        for p in points:
                            args += [f'{p[0] + 0.5}', f'{p[1] + 0.5}', f'{p[2] + 32}', '20']
                        run = subprocess.run([str(exe), *args], input=data, capture_output=True, timeout=120)
                        self.assertEqual(run.returncode, 0, run.stderr)
                        lines = run.stdout.decode().splitlines()
                        head = lines[0].split()
                        self.assertEqual(head[0], 'map', lines[0])
                        listed = sum(len(x) for x in model.lists)
                        self.assertEqual([int(x) for x in head[1:6]],
                                         [version, len(model.lights), len(model.areas), listed, int(model.vis)])
                        self.assertEqual([float(x) for x in head[6:9]], model.ambient)
                        records += len(model.lights)
                        self.assertEqual(len(lines), 1 + len(points))
                        compared += len(points)
                        for p, line in zip(points, lines[1:]):
                            fields = line.split()
                            kind, cube = model.cube([p[0] + 0.5, p[1] + 0.5, p[2] + 32], 20)
                            self.assertEqual(fields[1], kind, (p, line))
                            values = [float(x) for x in fields[2:]]
                            for i in range(6):
                                for c in range(3):
                                    self.assertAlmostEqual(values[3 * i + c], cube[i][c], delta=0.05 + 1e-3 * cube[i][c],
                                                           msg=(name, p, i, c))
                # The compiled lights the sidecar records counted, and every
                # player start compared.
                self.assertEqual(records, lights)
                self.assertEqual(compared, players)


if __name__ == '__main__':
    unittest.main()
