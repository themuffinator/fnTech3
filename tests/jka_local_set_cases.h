/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
 *
 * Synthetic Jedi Academy local ambient sets (client-game import 51),
 * independently authored. tests/jka_local_set_tests.py plays them through
 * fnTech3's production player; the retail results in
 * tests/fixtures/jka_local_set_retail.json were played from the same cases
 * by retail jamp.exe 1.01's own AS_AddLocalSet (0x4684f0), whose set lookup
 * was handed each case's set. */
#ifndef FNTECH3_TESTS_JKA_LOCAL_SET_CASES_H
#define FNTECH3_TESTS_JKA_LOCAL_SET_CASES_H

#include <math.h>
#include <string.h>

typedef struct {
	int found;                      /* 0: the name names no set */
	int loop;                       /* the looped wave's handle, or 0 */
	int waves[8];
	int waveCount;
	int timeStart, timeEnd;         /* timeBetweenWaves, in seconds */
	int volumeStart, volumeEnd;     /* volRange */
	int radius, masterVolume;
	float listener[3], origin[3];
	int entity, time, realtime;
	unsigned seed;                  /* the engine generator before the call */
} jkaLocalSetCase_t;

#define JKA_LOCAL_SET_HAND_CASES 54
#define JKA_LOCAL_SET_CASES ( JKA_LOCAL_SET_HAND_CASES + 64 )

static unsigned JKALocalSetNext( unsigned *state ) {
	*state = *state * 1664525u + 1013904223u;
	return *state >> 8;
}

static void JKALocalSetCase( int index, jkaLocalSetCase_t *c ) {
	int i;
	memset( c, 0, sizeof( *c ) );
	c->found = 1;
	c->loop = 7;
	c->timeStart = 10; c->timeEnd = 25;
	c->volumeStart = 255; c->volumeEnd = 255;
	c->radius = 250; c->masterVolume = 255;
	c->listener[0] = 16; c->listener[1] = -32; c->listener[2] = 64;
	memcpy( c->origin, c->listener, sizeof( c->origin ) );
	c->entity = 17;
	c->time = 120000; c->realtime = 90000;
	c->seed = 0x89abcdefu;
	switch ( index ) {
	/* The loop's volume by distance: full within half the radius, falling in
	 * a straight line to nothing at the radius. */
	case 0: break;
	case 1: c->origin[0] += 100; break;
	case 2: c->origin[0] += 125; break;
	case 3: c->listener[0] = 0; c->origin[0] = nextafterf( 125.0f, 0.0f ); break;
	case 4: c->listener[0] = 0; c->origin[0] = nextafterf( 125.0f, 1000.0f ); break;
	case 5: c->origin[1] += 187.5f; break;
	case 6: c->origin[2] -= 249.9f; break;
	case 7: c->origin[0] += 250; break;
	case 8: c->listener[0] = 0; c->origin[0] = nextafterf( 250.0f, 1000.0f ); break;
	case 9: c->origin[0] += 1000; break;
	case 10: c->radius = 0; break;
	case 11: c->radius = 0; c->origin[0] += 5; break;
	case 12: c->radius = 1; c->origin[0] += 0.5f; break;
	case 13: c->radius = 333; c->origin[0] += 100; c->origin[1] += 120; c->origin[2] += 37; break;
	case 14: c->radius = 300; c->masterVolume = 128; c->origin[1] -= 200; break;
	case 15: c->masterVolume = 0; c->origin[0] += 10; break;
	case 16: c->masterVolume = 1; c->origin[0] += 130; break;
	case 17: c->radius = 100000; c->origin[0] += 75000; break;
	case 18: c->listener[0] = -4000.25f; c->listener[1] = -3000.5f; c->listener[2] = -20.125f;
		c->origin[0] = -3880.75f; c->origin[1] = -3121.0f; c->origin[2] = 11.5f; break;
	case 19: c->radius = 1000; c->origin[0] += 600; c->origin[1] += 300; c->origin[2] += 100; break;
	/* A set with no loop, or whose name names no set. */
	case 20: c->loop = 0; c->origin[0] += 60; break;
	case 21: c->found = 0; c->origin[0] += 60; break;
	case 22: c->found = 0; c->realtime = -5; break;
	/* When a subwave plays: the real time less the time the client game hands
	 * over reaches a whole number of seconds drawn on every call. */
	case 23: c->waveCount = 1; c->waves[0] = 31; c->time = 95000; break;
	case 24: c->waveCount = 1; c->waves[0] = 31; c->time = 90000; break;
	case 25: c->waveCount = 1; c->waves[0] = 31; c->time = 80001; break;
	case 26: c->waveCount = 1; c->waves[0] = 31; c->time = 80000; break;
	case 27: c->waveCount = 1; c->waves[0] = 31; c->time = 65000; break;
	case 28: c->waveCount = 1; c->waves[0] = 31; c->time = -2000000; break;
	case 29: c->waveCount = 1; c->waves[0] = 31; c->time = 2147483647; c->realtime = -2147483647 - 1; break;
	case 30: c->waveCount = 1; c->waves[0] = 31; c->time = -2147483647 - 1; c->realtime = 2147483647; break;
	case 31: c->waveCount = 1; c->waves[0] = 31; c->timeStart = 0; c->timeEnd = 0; c->time = 90000; break;
	case 32: c->waveCount = 1; c->waves[0] = 31; c->timeStart = 0; c->timeEnd = 0; c->time = 90001; break;
	case 33: c->waveCount = 1; c->waves[0] = 31; c->timeStart = 5; c->timeEnd = 5; c->time = 85000; break;
	case 34: c->waveCount = 1; c->waves[0] = 31; c->timeStart = 0; c->timeEnd = 2147483; c->time = 0; c->realtime = 2000000000; break;
	case 35: c->waveCount = 1; c->waves[0] = 31; c->timeStart = 25; c->timeEnd = 10; c->time = 60000; break;
	/* The subwave's volume and choice: its range scaled by the loop's volume,
	 * one of the set's subwaves, nulls (handle 0) among them. */
	case 36: c->waveCount = 5; c->waves[0] = 0; c->waves[1] = 41; c->time = 0; c->loop = 0; break;
	case 37: c->waveCount = 8; for ( i = 0; i < 8; ++i ) c->waves[i] = 50 + i; c->time = 0;
		c->volumeStart = 200; c->volumeEnd = 250; break;
	case 38: c->waveCount = 8; for ( i = 0; i < 8; ++i ) c->waves[i] = 50 + i; c->time = 0;
		c->volumeStart = 0; c->volumeEnd = 255; c->origin[0] += 200; break;
	case 39: c->waveCount = 3; c->waves[0] = 61; c->waves[1] = 62; c->waves[2] = 63; c->time = 0;
		c->volumeStart = 0; c->volumeEnd = 0; break;
	case 40: c->waveCount = 3; c->waves[0] = 61; c->waves[1] = 62; c->waves[2] = 63; c->time = 0;
		c->volumeStart = 250; c->volumeEnd = 200; c->origin[1] += 150; break;
	case 41: c->waveCount = 2; c->waves[0] = 61; c->waves[1] = 62; c->time = 0;
		c->volumeStart = 200; c->volumeEnd = 250; c->masterVolume = 0; break;
	case 42: c->waveCount = 2; c->waves[0] = 61; c->waves[1] = 62; c->time = 0; c->origin[0] += 300; break;
	case 43: c->waveCount = 4; for ( i = 0; i < 4; ++i ) c->waves[i] = 70 + i; c->time = 0; c->seed = 0; break;
	case 44: c->waveCount = 4; for ( i = 0; i < 4; ++i ) c->waves[i] = 70 + i; c->time = 0; c->seed = 0xffffffffu; break;
	case 45: c->waveCount = 0; c->time = 0; c->volumeStart = 10; c->volumeEnd = 240; break;
	case 46: c->waveCount = 6; for ( i = 0; i < 6; ++i ) c->waves[i] = 80 + i; c->time = 0; c->entity = 0; break;
	case 47: c->waveCount = 6; for ( i = 0; i < 6; ++i ) c->waves[i] = 80 + i; c->time = 0; c->entity = 1023;
		c->radius = 2000; c->masterVolume = 200; c->origin[2] += 1500; break;
	/* Places whose distance, summed in floats rather than at retail's double
	 * precision, would land one step away and change the volume. */
	case 48: c->listener[0] = 3997.9619140625f; c->listener[1] = 3755.740478515625f; c->listener[2] = 1020.7464599609375f;
		c->origin[0] = 4166.34375f; c->origin[1] = 3610.245849609375f; c->origin[2] = 969.173583984375f; break;
	case 49: c->listener[0] = -2935.543701171875f; c->listener[1] = 3835.97607421875f; c->listener[2] = 1144.7091064453125f;
		c->origin[0] = -2795.954833984375f; c->origin[1] = 3783.830810546875f; c->origin[2] = 1127.5096435546875f; break;
	case 50: c->listener[0] = 2715.72607421875f; c->listener[1] = 2197.342041015625f; c->listener[2] = 1764.6707763671875f;
		c->origin[0] = 2748.864501953125f; c->origin[1] = 2030.6300048828125f; c->origin[2] = 1721.015869140625f; break;
	case 51: c->listener[0] = -1397.6644287109375f; c->listener[1] = 1042.9822998046875f; c->listener[2] = -226.9019317626953f;
		c->origin[0] = -1297.365478515625f; c->origin[1] = 906.5431518554688f; c->origin[2] = -387.428955078125f; break;
	/* A volume range past a byte, which sound.txt cannot give: the subwave
	 * can then be louder than the loop, and nothing clamps it. */
	case 52: c->waveCount = 1; c->waves[0] = 31; c->time = 0; c->origin[0] += 201;
		c->volumeStart = 300; c->volumeEnd = 300; break;
	case 53: c->waveCount = 2; c->waves[0] = 31; c->waves[1] = 32; c->time = 0;
		c->volumeStart = 250; c->volumeEnd = 400; break;
	/* Generated sets, places and clocks. */
	default: {
		static const int radii[] = { 0, 1, 50, 200, 250, 300, 333, 1000 };
		static const int masters[] = { 255, 200, 128, 1, 0 };
		static const int volumes[][2] = { { 255, 255 }, { 200, 250 }, { 0, 255 }, { 240, 250 }, { 10, 10 } };
		static const int intervals[][2] = { { 0, 0 }, { 10, 25 }, { 20, 50 }, { 3, 4 } };
		unsigned state = 0x2b7e1516u + (unsigned)index * 0x9e3779b9u;
		const int n = index - JKA_LOCAL_SET_HAND_CASES;
		c->radius = radii[JKALocalSetNext( &state ) % 8];
		c->masterVolume = masters[JKALocalSetNext( &state ) % 5];
		i = (int)( JKALocalSetNext( &state ) % 5 );
		c->volumeStart = volumes[i][0]; c->volumeEnd = volumes[i][1];
		i = (int)( JKALocalSetNext( &state ) % 4 );
		c->timeStart = intervals[i][0]; c->timeEnd = intervals[i][1];
		c->loop = ( n & 3 ) ? 90 + n : 0;
		c->waveCount = (int)( JKALocalSetNext( &state ) % 9 );
		for ( i = 0; i < c->waveCount; ++i ) c->waves[i] = ( JKALocalSetNext( &state ) % 5 ) ? 100 + n * 8 + i : 0;
		for ( i = 0; i < 3; ++i ) {
			c->listener[i] = (float)( (int)( JKALocalSetNext( &state ) % 8001 ) - 4000 ) * 0.25f;
			c->origin[i] = c->listener[i] + (float)( (int)( JKALocalSetNext( &state ) % 1601 ) - 800 ) * 0.375f;
		}
		c->entity = (int)( JKALocalSetNext( &state ) % 1024 );
		c->realtime = (int)( JKALocalSetNext( &state ) % 600000 );
		c->time = c->realtime - (int)( JKALocalSetNext( &state ) % 60000 ) + 5000;
		c->seed = JKALocalSetNext( &state ) * 2654435761u;
		break;
	}
	}
}

#endif
