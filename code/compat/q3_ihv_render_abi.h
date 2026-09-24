/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_RENDER_ABI_H
#define FNTECH3_Q3_IHV_RENDER_ABI_H

#include <stdint.h>

/* Observed February 1999 Win32 renderer records, independent of retail Q3.
 * Pointer words are native x86 addresses, never renderer qhandles. The
 * portable scene reader decodes their little-endian representations. */
typedef struct {
	uint32_t model;
	float axis[3][3], origin[3];
	int32_t frame;
	float oldorigin[3];
	int32_t oldframe;
	float backlerp;
	int32_t skinNum;
	uint32_t skin, shader, flags;
	int32_t nonNormalizedAxes;
	uint8_t rgba[4];
} fntech3_q3_ihv_render_entity_t;

typedef struct { float origin[3], color[3], radius; } fntech3_q3_ihv_render_light_t;
typedef struct { float origin[3]; uint8_t rgba[4]; } fntech3_q3_ihv_particle_t;
typedef struct { float xyz[3], st[2]; uint8_t rgba[4]; } fntech3_q3_ihv_poly_vertex_t;
typedef struct {
	uint32_t shader;
	int32_t vertexCount;
	uint32_t vertices, next; /* next is renderer-written grouping scratch. */
} fntech3_q3_ihv_poly_t;

typedef struct {
	uint32_t reserved;
	int32_t x, y, width, height;
	float fovX, fovY, origin[3], axis[3][3];
	float overlayGeometry[3], overlayColor[4];
	uint32_t overlayShader;
	int32_t time;
	uint32_t flags, visibleAreas;
	int32_t entityCount;
	uint32_t entities;
	int32_t lightCount;
	uint32_t lights;
	int32_t particleCount;
	uint32_t particles;
	int32_t polyCount;
	uint32_t polys;
} fntech3_q3_ihv_render_scene_t;

#endif
