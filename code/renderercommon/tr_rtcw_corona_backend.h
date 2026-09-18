/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_CORONA_BACKEND_H
#define FNTECH3_RTCW_CORONA_BACKEND_H
#include <float.h>
#include <limits.h>

typedef struct {
	int used, frame, scene, portal, visible;
	int64_t fadeTime;
	uint64_t order;
	fnRTCWCorona_t corona;
	int x, y, fog;
	float eyeZ, intensity;
} rtcwCoronaFade_t;

static rtcwCoronaFade_t rtcwCoronaFades[FN_RTCW_MAX_CORONA_FADES];
static uint64_t rtcwCoronaOrder;

static void R_ClearRTCWCoronaFades( void ) {
	Com_Memset( rtcwCoronaFades, 0, sizeof( rtcwCoronaFades ) );
	rtcwCoronaOrder = 0;
}

static int RB_RTCWCoronaFog( const vec3_t origin ) {
	int i, axis;
	if ( !tr.world || !tr.world->fogs ) return 0;
	for ( i = 1; i < tr.world->numfogs; ++i ) {
		for ( axis = 0; axis < 3; ++axis )
			if ( origin[axis] < tr.world->fogs[i].bounds[0][axis] ||
				origin[axis] > tr.world->fogs[i].bounds[1][axis] ) break;
		if ( axis == 3 ) return i;
	}
	return 0;
}

static void RB_QueueRTCWCorona( const fnRTCWCorona_t *corona ) {
	vec4_t eye, clip, normalized, window;
	rtcwCoronaFade_t *fade = NULL, *empty = NULL;
	const int frame = backEnd.viewParms.frameCount;
	double x, y;
	int i;
	R_TransformModelToClip( corona->origin, backEnd.or.modelMatrix,
		backEnd.viewParms.projectionMatrix, eye, clip );
	for ( i = 0; i < 4; ++i ) if ( !FnRTCWCoronaFinite( clip[i] ) ) return;
	for ( i = 0; i < 3; ++i ) if ( clip[i] >= clip[3] || clip[i] <= -clip[3] ) return;
	if ( !FnRTCWCoronaFinite( eye[2] ) || eye[2] >= 0 ) return;
	R_TransformClipToWindow( clip, &backEnd.viewParms, normalized, window );
	if ( !FnRTCWCoronaFinite( window[0] ) || !FnRTCWCoronaFinite( window[1] ) ||
		window[0] < 0 || window[0] >= backEnd.viewParms.viewportWidth ||
		window[1] < 0 || window[1] >= backEnd.viewParms.viewportHeight ) return;
	x = (double)backEnd.viewParms.viewportX + window[0];
	y = (double)backEnd.viewParms.viewportY + window[1];
	if ( x < INT_MIN || x > INT_MAX || y < INT_MIN || y > INT_MAX ) return;
	for ( i = 0; i < FN_RTCW_MAX_CORONA_FADES; ++i ) {
		rtcwCoronaFade_t *candidate = &rtcwCoronaFades[i];
		if ( !candidate->used ) { if ( !empty ) empty = candidate; continue; }
		if ( candidate->corona.id == corona->id && candidate->scene == backEnd.viewParms.frameSceneNum &&
			candidate->portal == (int)backEnd.viewParms.portalView ) { fade = candidate; break; }
	}
	if ( !fade ) {
		if ( !empty ) return;
		fade = empty;
		Com_Memset( fade, 0, sizeof( *fade ) );
		fade->used = 1;
		fade->frame = -1;
		fade->order = ++rtcwCoronaOrder;
	}
	if ( (int64_t)fade->frame != (int64_t)frame - 1 || fade->fadeTime > backEnd.refdef.time ) {
		fade->visible = 0;
		fade->fadeTime = (int64_t)backEnd.refdef.time - 2000;
	}
	fade->frame = frame;
	fade->scene = backEnd.viewParms.frameSceneNum;
	fade->portal = backEnd.viewParms.portalView;
	fade->corona = *corona;
	fade->x = (int)x;
	fade->y = (int)y;
	fade->eyeZ = eye[2];
	fade->fog = RB_RTCWCoronaFog( corona->origin );
	backEnd.pc.c_flareAdds++;
}

static qboolean RB_PrepareRTCWCoronas( void ) {
	int i;
	qboolean draw = qfalse;
	if ( !FnRTCWCoronaProfile( ri.gameId ) || !tr.world ) return qfalse;
	for ( i = 0; i < FN_RTCW_MAX_CORONA_FADES; ++i ) {
		rtcwCoronaFade_t *fade = &rtcwCoronaFades[i];
		if ( (int64_t)fade->frame < (int64_t)backEnd.viewParms.frameCount - 1 ||
			fade->frame > backEnd.viewParms.frameCount ) fade->used = 0;
		fade->intensity = 0;
	}
	if ( r_flares->integer >= 2 ) {
		unsigned index;
		for ( index = 0; index < backEnd.refdef.num_dlights; ++index ) {
			const dlight_t *light = &backEnd.refdef.dlights[index];
			fnRTCWCorona_t corona;
			VectorCopy( light->origin, corona.origin );
			VectorCopy( light->color, corona.color );
			corona.scale = 1; corona.id = (int)index; corona.flags = 1;
			RB_QueueRTCWCorona( &corona );
		}
	}
	if ( r_flares->integer == 1 || r_flares->integer == 3 )
		for ( i = 0; i < backEnd.refdef.numRTCWCoronas; ++i )
			RB_QueueRTCWCorona( &backEnd.refdef.rtcwCoronas[i] );
	for ( i = 0; i < FN_RTCW_MAX_CORONA_FADES; ++i ) {
		rtcwCoronaFade_t *fade = &rtcwCoronaFades[i];
		int visible;
		if ( !fade->used || fade->scene != backEnd.viewParms.frameSceneNum ||
			fade->portal != (int)backEnd.viewParms.portalView ) continue;
		visible = FnRTCWCoronaVisible( ri.gameId != COM_GAME_RTCW_SP, fade->corona.flags );
		if ( visible != fade->visible ) {
			fade->visible = visible;
			fade->fadeTime = (int64_t)backEnd.refdef.time - 1;
		}
		fade->intensity = FnRTCWCoronaFade( (int64_t)backEnd.refdef.time - fade->fadeTime,
			visible, r_flareFade->value );
		backEnd.pc.c_flareTests++;
		if ( fade->intensity > 0 ) draw = qtrue;
		else fade->used = 0;
	}
	return draw;
}

static void RB_DrawRTCWCoronas( void ) {
	int i, component, pass;
	uint64_t before = UINT64_MAX;
	if ( !FnRTCWCoronaProfile( ri.gameId ) ) return;
	// Original active-list order is newest first, including reused fade slots.
	for ( pass = 0; pass < FN_RTCW_MAX_CORONA_FADES; ++pass ) {
		const rtcwCoronaFade_t *fade = NULL;
		color4ub_t color;
		float size;
		for ( i = 0; i < FN_RTCW_MAX_CORONA_FADES; ++i ) {
			const rtcwCoronaFade_t *candidate = &rtcwCoronaFades[i];
			if ( candidate->used && candidate->order < before && candidate->intensity > 0 &&
				candidate->scene == backEnd.viewParms.frameSceneNum &&
				candidate->portal == (int)backEnd.viewParms.portalView &&
				(!fade || candidate->order > fade->order) ) fade = candidate;
		}
		if ( !fade ) break;
		before = fade->order;
		size = backEnd.viewParms.viewportWidth *
			((r_flareSize->value * fade->corona.scale) / 640.0f + 8.0f / -fade->eyeZ);
		if ( !FnRTCWCoronaFinite( size ) || size <= 0 || size > FLT_MAX / 4 ) continue;
		for ( component = 0; component < 3; ++component )
			color.rgba[component] = FnRTCWCoronaByte( fade->corona.color[component] * tr.identityLight );
		color.rgba[3] = FnRTCWCoronaByte( fade->intensity );
		RB_BeginSurface( ri.gameId == COM_GAME_RTCW_SP && (fade->corona.flags & 2) ?
			tr.rtcwSpotFlareShader : tr.flareShader, fade->fog );
		RB_AddQuadStamp2( fade->x - size, fade->y - size, size * 2, size * 2, 0, 0, 1, 1, color );
		RB_EndSurface();
		backEnd.pc.c_flareRenders++;
	}
}

#endif
