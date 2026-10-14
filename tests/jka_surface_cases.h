/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
 *
 * Synthetic Jedi Academy saber glow (5), line (8) and oriented line (9)
 * entities and views, independently authored. tests/jka_surface_fixture.c
 * draws them through fnTech3's renderers; the retail vectors in
 * tests/fixtures/jka_surface_retail.json were drawn from the same cases by
 * retail jamp.exe 1.01's own routines (RB_SurfaceEntity 0x4bdcd0). */
#ifndef FNTECH3_TESTS_JKA_SURFACE_CASES_H
#define FNTECH3_TESTS_JKA_SURFACE_CASES_H

typedef struct {
	int type;
	float origin[3], oldorigin[3], axis[3][3];
	float radius, saberLength, lineWidth, lineSTScale;
	unsigned char rgba[4];
	float viewOrigin[3], viewAxis[3][3];
	int mirror;
} jkaSurfaceCase_t;

#define JKA_SURFACE_CASES 22

/* The view's forward, left and up axes from yaw and pitch, in degrees. */
static void JKASurfaceViewAxes( float yaw, float pitch, float axis[3][3] ) {
	const double y = yaw * 3.14159265358979323846 / 180.0, p = pitch * 3.14159265358979323846 / 180.0;
	axis[0][0] = (float)( cos( p ) * cos( y ) ); axis[0][1] = (float)( cos( p ) * sin( y ) ); axis[0][2] = (float)-sin( p );
	axis[1][0] = (float)-sin( y ); axis[1][1] = (float)cos( y ); axis[1][2] = 0.0f;
	axis[2][0] = (float)( sin( p ) * cos( y ) ); axis[2][1] = (float)( sin( p ) * sin( y ) ); axis[2][2] = (float)cos( p );
}

static void JKASurfaceCase( int index, jkaSurfaceCase_t *c ) {
	int i;
	memset( c, 0, sizeof( *c ) );
	c->origin[0] = 10; c->origin[1] = 20; c->origin[2] = 30;
	c->oldorigin[0] = 50; c->oldorigin[1] = 25; c->oldorigin[2] = 35;
	c->axis[0][0] = 0.6f; c->axis[0][2] = 0.8f;
	c->axis[1][1] = 1.0f;
	c->axis[2][0] = -0.8f; c->axis[2][2] = 0.6f;
	c->radius = 2.85f;
	for ( i = 0; i < 4; ++i ) c->rgba[i] = 255;
	c->viewOrigin[0] = -100; c->viewOrigin[1] = 0; c->viewOrigin[2] = 40;
	JKASurfaceViewAxes( 0, 0, c->viewAxis );
	switch ( index ) {
	/* The saber glow: the blade as the client game draws it, then lengths,
	 * radii, axes, views and colours that move each rule. */
	case 0: c->type = 5; c->saberLength = 40; break;
	case 1: c->type = 5; c->saberLength = 40; c->mirror = 1; break;
	case 2: c->type = 5; c->saberLength = 33.3f; c->radius = 0.1f; break;
	case 3: c->type = 5; c->saberLength = 0.5f; break;
	case 4: c->type = 5; c->saberLength = 0; break;
	case 5: c->type = 5; c->saberLength = -3; break;
	case 6: c->type = 5; c->saberLength = 40; JKASurfaceViewAxes( 30, 10, c->viewAxis ); break;
	case 7: c->type = 5; c->saberLength = 12; c->axis[0][0] = 2; c->axis[0][2] = 0; break;
	case 8: c->type = 5; c->saberLength = 10; c->radius = 0; break;
	case 9: c->type = 5; c->saberLength = 40; c->rgba[0] = 201; c->rgba[1] = 113; c->rgba[2] = 57; c->rgba[3] = 117; break;
	/* The line: across the plane through the eye and both ends. */
	case 10: c->type = 8; c->radius = 1.5f; break;
	case 11: c->type = 8; c->radius = 1.5f; c->mirror = 1; break;
	case 12: c->type = 8; c->radius = 1.5f; JKASurfaceViewAxes( -45, 20, c->viewAxis );
		c->viewOrigin[0] = 30; c->viewOrigin[1] = -80; c->viewOrigin[2] = 90; break;
	/* An eye on the line's own line, and a line of no length: no width. */
	case 13: c->type = 8; c->radius = 1.5f; c->viewOrigin[0] = -30; c->viewOrigin[1] = 15; c->viewOrigin[2] = 25; break;
	case 14: c->type = 8; c->radius = 1.5f; memcpy( c->oldorigin, c->origin, sizeof( c->origin ) ); break;
	case 15: c->type = 8; c->radius = 0; c->rgba[0] = 201; c->rgba[1] = 113; c->rgba[2] = 57; c->rgba[3] = 117; break;
	/* The oriented line: across the entity's own second axis. */
	case 16: c->type = 9; c->lineWidth = 4; c->lineSTScale = 2.5f; break;
	case 17: c->type = 9; c->lineWidth = 6.25f; c->lineSTScale = 0.75f; c->axis[1][1] = 3; c->axis[1][2] = 4; break;
	case 18: c->type = 9; c->lineWidth = 4; c->lineSTScale = 2.5f; c->axis[1][1] = 0; break;
	case 19: c->type = 9; c->lineWidth = -2; c->lineSTScale = -1; break;
	case 20: c->type = 9; c->lineWidth = 4; c->lineSTScale = 2.5f; c->mirror = 1; break;
	default: c->type = 9; c->lineWidth = 3; c->lineSTScale = 1; c->radius = 0;
		c->rgba[0] = 201; c->rgba[1] = 113; c->rgba[2] = 57; c->rgba[3] = 117; break;
	}
}

/* The random numbers both renderers are handed for a hilt's radius. */
static unsigned jkaSurfaceRandomIndex;
static int JKASurfaceRandom( void ) {
	return (int)( ( ( ++jkaSurfaceRandomIndex ) * 1777u + 123u ) & 32767u );
}

#endif
