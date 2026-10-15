/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_JEDI_SPRITE_H
#define FNTECH3_TR_JEDI_SPRITE_H

/*
 * Raven's surface sprites: a shader stage that `surfaceSprites` turns into
 * grass, reeds, rain splashes and the like scattered over the surface's
 * triangles instead of drawing over them. The record keeps the 0x48-byte
 * order every Jedi engine stores: jamp.exe and jasp.exe 1.01 hang it off the
 * stage (stage +0x84, allocated on the first sprite word), jk2mp.exe and
 * jk2sp.exe 1.02a keep it inside the stage at +0xf0. A record whose type is
 * still JEDI_SPRITE_NONE draws nothing and leaves its stage an ordinary one.
 * Record 2026-10-15-jedi-surface-sprites.
 */

typedef enum {
	JEDI_SPRITE_NONE,
	JEDI_SPRITE_VERTICAL,	// `vertical`: upright, facing the viewer
	JEDI_SPRITE_ORIENTED,	// `oriented`: a billboard
	JEDI_SPRITE_EFFECT,		// `effect`: a looping billboard effect
	JEDI_SPRITE_WEATHERFX,	// `effect` with ssFXWeather
	JEDI_SPRITE_FLATTENED	// `flattened`: Academy single player only
} jediSpriteType_t;

typedef enum {
	JEDI_SPRITE_FACING_NORMAL,
	JEDI_SPRITE_FACING_UP,		// ssFaceup
	JEDI_SPRITE_FACING_DOWN,	// ssHangdown
	JEDI_SPRITE_FACING_ANY		// ssAnyangle
} jediSpriteFacing_t;

typedef struct {
	int		type;			// +0x00 jediSpriteType_t
	float	width;			// +0x04
	float	height;			// +0x08
	float	density;		// +0x0c
	float	wind;			// +0x10
	float	windIdle;		// +0x14
	float	fadeDist;		// +0x18
	float	fadeMax;		// +0x1c
	float	fadeScale;		// +0x20
	float	fxAlphaStart;	// +0x24
	float	fxAlphaEnd;		// +0x28
	float	fxDuration;		// +0x2c, milliseconds
	float	vertSkew;		// +0x30
	float	variance[2];	// +0x34
	float	fxGrow[2];		// +0x3c
	int		facing;			// +0x44 jediSpriteFacing_t
} jediSurfaceSprite_t;

/* A shader's sprite stages, each finished as a shader of its own that the
 * back end draws over the generated sprites (tr_jedi_sprite_draw.h). */
typedef struct {
	struct shader_s		*shader;
	jediSurfaceSprite_t	parms;
} jediSpritePass_t;

#endif
