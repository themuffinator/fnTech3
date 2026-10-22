/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_light.c

#include "tr_local.h"
#include "../renderercommon/tr_csm_model_light.h"
#include "../compat/alice_render_abi.h"
#include "../renderercommon/tr_stvef_light.h"

#define	DLIGHT_AT_RADIUS		16
// at the edge of a dlight's influence, this amount of light will be added

#define	DLIGHT_MINIMUM_RADIUS	16
// never calculate a range less than this to prevent huge light numbers


/*
===============
R_TransformDlights

Transforms the origins of an array of dlights.
Used by both the front end (for DlightBmodel) and
the back end (before doing the lighting calculation)
===============
*/
void R_TransformDlights( int count, dlight_t *dl, orientationr_t *or) {
	int		i;
	vec3_t	temp, temp2;

	for ( i = 0 ; i < count ; i++, dl++ ) {
		VectorSubtract( dl->origin, or->origin, temp );
		dl->transformed[0] = DotProduct( temp, or->axis[0] );
		dl->transformed[1] = DotProduct( temp, or->axis[1] );
		dl->transformed[2] = DotProduct( temp, or->axis[2] );
		if ( dl->linear ) {
			VectorSubtract( dl->origin2, or->origin, temp2 );
			dl->transformed2[0] = DotProduct( temp2, or->axis[0] );
			dl->transformed2[1] = DotProduct( temp2, or->axis[1] );
			dl->transformed2[2] = DotProduct( temp2, or->axis[2] );
		}
	}
}


#ifdef USE_LEGACY_DLIGHTS
/*
=============
R_DlightBmodel

Determine which dynamic lights may effect this bmodel
=============
*/
void R_DlightBmodel( bmodel_t *bmodel ) {
	int			i, j;
	const dlight_t	*dl;
	int			mask;
	msurface_t	*surf;

	// transform all the lights
	R_TransformDlights( tr.refdef.num_dlights, tr.refdef.dlights, &tr.or );

	mask = 0;
	for ( i = 0; i < tr.refdef.num_dlights; i++ ) {
		dl = &tr.refdef.dlights[i];

		// see if the point is close enough to the bounds to matter
		for ( j = 0 ; j < 3 ; j++ ) {
			if ( dl->transformed[j] - bmodel->bounds[1][j] > dl->radius ) {
				break;
			}
			if ( bmodel->bounds[0][j] - dl->transformed[j] > dl->radius ) {
				break;
			}
		}
		if ( j < 3 ) {
			continue;
		}

		// we need to check this light
		mask |= 1 << i;
	}

	tr.currentEntity->needDlights = (mask != 0) ? 1 : 0;

	// set the dlight bits in all the surfaces
	for ( i = 0 ; i < bmodel->numSurfaces ; i++ ) {
		surf = bmodel->firstSurface + i;

		if ( *surf->data == SF_FACE ) {
			((srfSurfaceFace_t *)surf->data)->dlightBits = mask;
		} else if ( *surf->data == SF_GRID ) {
			((srfGridMesh_t *)surf->data)->dlightBits = mask;
		} else if ( *surf->data == SF_TRIANGLES ) {
			((srfTriangles_t *)surf->data)->dlightBits = mask;
		}
	}
}
#endif // USE_LEGACY_DLIGHTS


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

extern	cvar_t	*r_ambientScale;
extern	cvar_t	*r_directedScale;
extern	cvar_t	*r_debugLight;

/*
=================
R_SetupEntityLightingGrid

=================
*/
static void R_SetupEntityLightingGrid( trRefEntity_t *ent ) {
	vec3_t	lightOrigin;
	int		pos[3];
	int		i, j;
	byte	*gridData;
	float	frac[3];
	int		gridStep[3];
	vec3_t	direction;
	float	totalFactor;

	if ( ent->e.renderfx & RF_LIGHTING_ORIGIN ) {
		// separate lightOrigins are needed so an object that is
		// sinking into the ground can still be lit, and so
		// multi-part models can be lit identically
		VectorCopy( ent->e.lightingOrigin, lightOrigin );
	} else {
		VectorCopy( ent->e.origin, lightOrigin );
	}

	VectorSubtract( lightOrigin, tr.world->lightGridOrigin, lightOrigin );
	for ( i = 0 ; i < 3 ; i++ ) {
		float	v;

		v = lightOrigin[i]*tr.world->lightGridInverseSize[i];
		pos[i] = floor( v );
		frac[i] = v - pos[i];
		if ( pos[i] < 0 ) {
			pos[i] = 0;
		} else if ( pos[i] > tr.world->lightGridBounds[i] - 1 ) {
			pos[i] = tr.world->lightGridBounds[i] - 1;
		}
	}

	VectorClear( ent->ambientLight );
	VectorClear( ent->directedLight );
	VectorClear( direction );

	assert( tr.world->lightGridData ); // NULL with -nolight maps

	// trilerp the light value
	gridStep[0] = 8;
	gridStep[1] = 8 * tr.world->lightGridBounds[0];
	gridStep[2] = 8 * tr.world->lightGridBounds[0] * tr.world->lightGridBounds[1];
	gridData = tr.world->lightGridData + pos[0] * gridStep[0]
		+ pos[1] * gridStep[1] + pos[2] * gridStep[2];

	totalFactor = 0;
	for ( i = 0 ; i < 8 ; i++ ) {
		float	factor;
		byte	*data;
		int		lat, lng;
		vec3_t	normal;
		factor = 1.0;
		data = gridData;
		for ( j = 0 ; j < 3 ; j++ ) {
			if ( i & (1<<j) ) {
				if ( pos[j] + 1 > tr.world->lightGridBounds[j] - 1 ) {
					break; // ignore values outside lightgrid
				}
				factor *= frac[j];
				data += gridStep[j];
			} else {
				factor *= (1.0f - frac[j]);
			}
		}

		if ( j != 3 ) {
			continue;
		}

		if ( tr.world->ravenBsp.bytes
			? !FN_RavenBspGridCellValid( &tr.world->ravenBsp, (size_t)( data - tr.world->lightGridData ) / 8 )
			: !(data[0]+data[1]+data[2]) ) {
			continue;	// ignore samples in walls
		}
		totalFactor += factor;

		ent->ambientLight[0] += factor * data[0];
		ent->ambientLight[1] += factor * data[1];
		ent->ambientLight[2] += factor * data[2];

		ent->directedLight[0] += factor * data[3];
		ent->directedLight[1] += factor * data[4];
		ent->directedLight[2] += factor * data[5];

		lat = data[7];
		lng = data[6];
		lat *= (FUNCTABLE_SIZE/256);
		lng *= (FUNCTABLE_SIZE/256);

		// decode X as cos( lat ) * sin( long )
		// decode Y as sin( lat ) * sin( long )
		// decode Z as cos( long )

		normal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
		normal[1] = tr.sinTable[lat] * tr.sinTable[lng];
		normal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

		VectorMA( direction, factor, normal, direction );
	}

	if ( totalFactor > 0 && totalFactor < 0.99 ) {
		totalFactor = 1.0f / totalFactor;
		VectorScale( ent->ambientLight, totalFactor, ent->ambientLight );
		VectorScale( ent->directedLight, totalFactor, ent->directedLight );
	}

	VectorScale( ent->ambientLight, r_ambientScale->value, ent->ambientLight );
	VectorScale( ent->directedLight, r_directedScale->value, ent->directedLight );
	FnRTCWWorldGridScale( &tr.rtcwWorld, ent->ambientLight, ent->directedLight );

	VectorNormalize2( direction, ent->lightDir );
}


/*
===============
LogLight
===============
*/
static void LogLight( const trRefEntity_t *ent ) {
	int	max1, max2;

	if ( !(ent->e.renderfx & RF_FIRST_PERSON ) ) {
		return;
	}

	max1 = ent->ambientLight[0];
	if ( ent->ambientLight[1] > max1 ) {
		max1 = ent->ambientLight[1];
	} else if ( ent->ambientLight[2] > max1 ) {
		max1 = ent->ambientLight[2];
	}

	max2 = ent->directedLight[0];
	if ( ent->directedLight[1] > max2 ) {
		max2 = ent->directedLight[1];
	} else if ( ent->directedLight[2] > max2 ) {
		max2 = ent->directedLight[2];
	}

	ri.Printf( PRINT_ALL, "amb:%i  dir:%i\n", max1, max2 );
}


/*
=================
R_SetupEntityLighting

Calculates all the lighting values that will be used
by the Calc_* functions
=================
*/
#include "../renderercommon/tr_q3_ihv_model_light_render.h"

void R_SetupEntityLighting( const trRefdef_t *refdef, trRefEntity_t *ent ) {
	int				i;
	const dlight_t		*dl;
	float			power;
	vec3_t			dir;
	float			d;
	vec3_t			lightDir;
	vec3_t			lightOrigin;
#ifdef USE_PMLIGHT
	vec3_t			shadowLightDir;
#endif

	// lighting calculations
	if ( ent->lightingCalculated ) {
		return;
	}
	ent->lightingCalculated = qtrue;
	if ( ri.gameId == COM_GAME_QUAKE3_IHV && ent->e.q3IHV.active ) {
		/* IHV model lighting has no independent sun contribution. */
		VectorClear( ent->csmSunLight );
		R_SetupQ3IHVEntityLighting( refdef, ent );
		return;
	}

	//
	// trace a sample point down to find ambient light
	//
	if ( ent->e.renderfx & RF_LIGHTING_ORIGIN ) {
		// separate lightOrigins are needed so an object that is
		// sinking into the ground can still be lit, and so
		// multi-part models can be lit identically
		VectorCopy( ent->e.lightingOrigin, lightOrigin );
	} else {
		VectorCopy( ent->e.origin, lightOrigin );
	}

	// if NOWORLDMODEL, only use dynamic lights (menu system, etc)
	if ( !(refdef->rdflags & RDF_NOWORLDMODEL )
		&& tr.world && tr.world->lightGridData ) {
		R_SetupEntityLightingGrid( ent );
	} else {
		ent->ambientLight[0] = ent->ambientLight[1] =
			ent->ambientLight[2] = tr.identityLight * 150;
		ent->directedLight[0] = ent->directedLight[1] =
			ent->directedLight[2] = tr.identityLight * 150;
		VectorCopy( tr.sunDirection, ent->lightDir );
	}

	// bonus items and view weapons have a fixed minimum add
	if ( R_UsesRTCWLighting() && ent->e.rtcw.hilightIntensity != 0.0f ) {
		for ( i = 0; i < 3; ++i ) ent->ambientLight[i] += tr.identityLight * 128 * ent->e.rtcw.hilightIntensity;
	} else if ( ent->e.stvef.fullbright || ent->e.stvef.darken ) {
		// Elite Force brightens or darkens after the grid light, in place of the
		// minimum add, and dynamic lights still apply (stvoyHM.exe 0x479f3c).
		// Retail lets a darkened ambient go negative; it stops at zero here.
		const float change = tr.identityLight * ( ent->e.stvef.fullbright ? 100.0f : -100.0f );
		for ( i = 0; i < 3; ++i ) {
			ent->ambientLight[i] += change;
			if ( ent->ambientLight[i] < 0.0f ) ent->ambientLight[i] = 0.0f;
		}
	} else if ( !R_UsesRTCWLighting() || ( ent->e.renderfx & RF_MINLIGHT ) ) {
		// give everything a minimum light add
		ent->ambientLight[0] += tr.identityLight * 32;
		ent->ambientLight[1] += tr.identityLight * 32;
		ent->ambientLight[2] += tr.identityLight * 32;
	}

	if ( R_UsesRTCWLighting() && refdef->rtcw.snooperView && ent->e.rtcw.entityNum < 64 ) {
		VectorSet( ent->ambientLight, 245, 245, 245 );
	}

	R_CSMCaptureEntitySun( ent );
	//
	// modify the light by dynamic lights
	//
	d = VectorLength( ent->directedLight );
	VectorScale( ent->lightDir, d, lightDir );
#ifdef USE_PMLIGHT
	if ( R_GetDlightMode() == 2 && !( R_UsesRTCWLighting() && refdef->rtcw.snooperView ) ) {
		// only direct lights
		// but we need to deal with shadow light direction
		VectorCopy( lightDir, shadowLightDir );
		if ( r_shadows->integer == 2 ) {
			for ( i = 0 ; i < refdef->num_dlights ; i++ ) {
				dl = &refdef->dlights[i];
				if ( dl->linear ) // no support for linear lights atm
					continue;
				VectorSubtract( dl->origin, lightOrigin, dir );
				d = VectorNormalize( dir );
				power = DLIGHT_AT_RADIUS * ( dl->radius * dl->radius );
				if ( d < DLIGHT_MINIMUM_RADIUS ) {
					d = DLIGHT_MINIMUM_RADIUS;
				}
				d = power / ( d * d );
				VectorMA( shadowLightDir, d, dir, shadowLightDir );
			}
		} // if ( r_shadows->integer == 2 )
	}  // if ( R_GetDlightMode() == 2 )
	else
#endif
	for ( i = 0 ; i < refdef->num_dlights ; i++ ) {
		dl = &refdef->dlights[i];
		if ( dl->rtcwShader ) continue;
		VectorSubtract( dl->origin, lightOrigin, dir );
		d = VectorNormalize( dir );

		power = DLIGHT_AT_RADIUS * ( dl->radius * dl->radius );
		if ( d < DLIGHT_MINIMUM_RADIUS ) {
			d = DLIGHT_MINIMUM_RADIUS;
		}
		d = power / ( d * d );

		VectorMA( ent->directedLight, d, dl->color, ent->directedLight );
		VectorMA( lightDir, d, dir, lightDir );
	}

	// clamp ambient
	for ( i = 0 ; i < 3 ; i++ ) {
		if ( ent->ambientLight[i] > tr.identityLightByte ) {
			ent->ambientLight[i] = tr.identityLightByte;
		}
	}

	if ( r_debugLight->integer ) {
		LogLight( ent );
	}

	// save out the byte packet version
	((byte *)&ent->ambientLightInt)[0] = ri.gameId == COM_GAME_STVEF_SP || ri.gameId == COM_GAME_STVEF_MP
		? FnSTVEF_LightByte( ent->ambientLight[0] ) : myftol( ent->ambientLight[0] ); // -EC-: don't use ri.ftol to avoid precision losses
	((byte *)&ent->ambientLightInt)[1] = ri.gameId == COM_GAME_STVEF_SP || ri.gameId == COM_GAME_STVEF_MP
		? FnSTVEF_LightByte( ent->ambientLight[1] ) : myftol( ent->ambientLight[1] );
	((byte *)&ent->ambientLightInt)[2] = ri.gameId == COM_GAME_STVEF_SP || ri.gameId == COM_GAME_STVEF_MP
		? FnSTVEF_LightByte( ent->ambientLight[2] ) : myftol( ent->ambientLight[2] );
	((byte *)&ent->ambientLightInt)[3] = 0xff;

	// transform the direction to local space
	VectorNormalize( lightDir );
	ent->lightDir[0] = DotProduct( lightDir, ent->e.axis[0] );
	ent->lightDir[1] = DotProduct( lightDir, ent->e.axis[1] );
	ent->lightDir[2] = DotProduct( lightDir, ent->e.axis[2] );

#ifdef USE_PMLIGHT
	if ( r_shadows->integer == 2 && R_GetDlightMode() == 2 ) {
		VectorNormalize( shadowLightDir );
		ent->shadowLightDir[0] = DotProduct( shadowLightDir, ent->e.axis[0] );
		ent->shadowLightDir[1] = DotProduct( shadowLightDir, ent->e.axis[1] );
		ent->shadowLightDir[2] = DotProduct( shadowLightDir, ent->e.axis[2] );
	}
#endif
}


#include "../renderercommon/tr_ritual_model_light_render.h"

/*
=================
R_SetupAliceEntityLighting

American McGee's Alice's entity light, from retail's R_SetupEntityLighting
(alice.exe 0x004ac7e0) where it differs from Quake III's:

- an entity its game module flags RF_EXTRALIGHT or RF_INVISIBLE -- every
  sentient and every weapon -- is lit from the map's compiled lights, never
  from the grid, when the caller gives where in its own frame it is lit and
  half its frame's radius (0x004abc50; R_RitualModelLight,
  code/renderercommon/tr_ritual_model_light.h); its minimum light is part
  of that;
- the light grid is sampled at a point the caller works out -- for a TIKI
  model the middle of its current frame, moved by its model's origin and
  `lightoffset`, turned by its axis and added to its lighting origin
  (0x004aaac0) -- not at the entity's origin;
- an entity flagged full bright passes over the grid for the flat light a
  level without one gets (0x004ac825);
- in a scene without a world an entity starts from no light at all, dynamic
  lights aside (0x004acbfc..0x004acc26), not from that flat light. [I]
  Heavy Metal: F.A.K.K.2's renderer kept the flat light there: its HUD and
  inventory draw its lit weapon models in such scenes and add no light of
  their own (UI_Render3DModel 0x00408210 is passed none);
- only an entity flagged minlight gets the minimum add (0x004acc66); Quake III
  gives it to everything.

Dynamic lights, the clamp and the rest are this renderer's, as for every
other profile; retail's own dynamic-light falloff and its rescaling of an
over-bright directed light are not reproduced.
=================
*/
void R_SetupAliceEntityLighting( const trRefdef_t *refdef, trRefEntity_t *ent, const vec3_t lightOrigin,
	const float *localPoint, float reach ) {
	qboolean		ritual = qfalse;
	int				i;
	const dlight_t		*dl;
	float			power;
	vec3_t			dir;
	float			d;
	vec3_t			lightDir;
#ifdef USE_PMLIGHT
	vec3_t			shadowLightDir;
#endif

	if ( ent->lightingCalculated ) {
		return;
	}
	ent->lightingCalculated = qtrue;

	if ( R_RitualModelLight( refdef, ent, lightOrigin, localPoint, reach ) ) {
		// the map's compiled lights, its added light included
		ritual = qtrue;
	} else if ( !( refdef->rdflags & RDF_NOWORLDMODEL ) && tr.world->lightGridData
		&& !( ent->e.alice.renderfx & FNTECH3_ALICE_RF_FULLBRIGHT ) ) {
		// the grid samples at the entity's lighting origin
		const int renderfx = ent->e.renderfx;
		vec3_t lightingOrigin;
		VectorCopy( ent->e.lightingOrigin, lightingOrigin );
		VectorCopy( lightOrigin, ent->e.lightingOrigin );
		ent->e.renderfx |= RF_LIGHTING_ORIGIN;
		R_SetupEntityLightingGrid( ent );
		ent->e.renderfx = renderfx;
		VectorCopy( lightingOrigin, ent->e.lightingOrigin );
	} else if ( ( refdef->rdflags & RDF_NOWORLDMODEL ) && ri.gameId != COM_GAME_FAKK2 ) {
		VectorClear( ent->ambientLight );
		VectorClear( ent->directedLight );
		VectorClear( ent->lightDir );
	} else {
		ent->ambientLight[0] = ent->ambientLight[1] =
			ent->ambientLight[2] = tr.identityLight * 150;
		ent->directedLight[0] = ent->directedLight[1] =
			ent->directedLight[2] = tr.identityLight * 150;
		VectorCopy( tr.sunDirection, ent->lightDir );
	}

	if ( !ritual && ( ent->e.alice.renderfx & FNTECH3_ALICE_RF_MINLIGHT ) ) {
		ent->ambientLight[0] += tr.identityLight * 32;
		ent->ambientLight[1] += tr.identityLight * 32;
		ent->ambientLight[2] += tr.identityLight * 32;
	}

	R_CSMCaptureEntitySun( ent );
	if ( ent->e.alice.renderfx & FNTECH3_ALICE_RF_FULLBRIGHT ) VectorClear( ent->csmSunLight );
	//
	// modify the light by dynamic lights
	//
	d = VectorLength( ent->directedLight );
	VectorScale( ent->lightDir, d, lightDir );
#ifdef USE_PMLIGHT
	if ( R_GetDlightMode() == 2 ) {
		// only direct lights
		// but we need to deal with shadow light direction
		VectorCopy( lightDir, shadowLightDir );
		if ( r_shadows->integer == 2 ) {
			for ( i = 0 ; i < refdef->num_dlights ; i++ ) {
				dl = &refdef->dlights[i];
				if ( dl->linear ) // no support for linear lights atm
					continue;
				VectorSubtract( dl->origin, lightOrigin, dir );
				d = VectorNormalize( dir );
				power = DLIGHT_AT_RADIUS * ( dl->radius * dl->radius );
				if ( d < DLIGHT_MINIMUM_RADIUS ) {
					d = DLIGHT_MINIMUM_RADIUS;
				}
				d = power / ( d * d );
				VectorMA( shadowLightDir, d, dir, shadowLightDir );
			}
		}
	}
	else
#endif
	for ( i = 0 ; i < refdef->num_dlights ; i++ ) {
		dl = &refdef->dlights[i];
		if ( dl->rtcwShader ) continue;
		VectorSubtract( dl->origin, lightOrigin, dir );
		d = VectorNormalize( dir );

		power = DLIGHT_AT_RADIUS * ( dl->radius * dl->radius );
		if ( d < DLIGHT_MINIMUM_RADIUS ) {
			d = DLIGHT_MINIMUM_RADIUS;
		}
		d = power / ( d * d );

		VectorMA( ent->directedLight, d, dl->color, ent->directedLight );
		VectorMA( lightDir, d, dir, lightDir );
	}

	// clamp ambient
	for ( i = 0 ; i < 3 ; i++ ) {
		if ( ent->ambientLight[i] > tr.identityLightByte ) {
			ent->ambientLight[i] = tr.identityLightByte;
		}
	}

	if ( r_debugLight->integer ) {
		LogLight( ent );
	}

	((byte *)&ent->ambientLightInt)[0] = myftol( ent->ambientLight[0] );
	((byte *)&ent->ambientLightInt)[1] = myftol( ent->ambientLight[1] );
	((byte *)&ent->ambientLightInt)[2] = myftol( ent->ambientLight[2] );
	((byte *)&ent->ambientLightInt)[3] = 0xff;

	// transform the direction to local space
	VectorNormalize( lightDir );
	ent->lightDir[0] = DotProduct( lightDir, ent->e.axis[0] );
	ent->lightDir[1] = DotProduct( lightDir, ent->e.axis[1] );
	ent->lightDir[2] = DotProduct( lightDir, ent->e.axis[2] );

#ifdef USE_PMLIGHT
	if ( r_shadows->integer == 2 && R_GetDlightMode() == 2 ) {
		VectorNormalize( shadowLightDir );
		ent->shadowLightDir[0] = DotProduct( shadowLightDir, ent->e.axis[0] );
		ent->shadowLightDir[1] = DotProduct( shadowLightDir, ent->e.axis[1] );
		ent->shadowLightDir[2] = DotProduct( shadowLightDir, ent->e.axis[2] );
	}
#endif
}


/*
=================
R_LightForPoint
=================
*/
int R_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir )
{
	trRefEntity_t ent;

	if ( !tr.world || tr.world->lightGridData == NULL )
	  return qfalse;

	Com_Memset(&ent, 0, sizeof(ent));
	VectorCopy( point, ent.e.origin );
	R_SetupEntityLightingGrid( &ent );
	VectorCopy(ent.ambientLight, ambientLight);
	VectorCopy(ent.directedLight, directedLight);
	VectorCopy(ent.lightDir, lightDir);

	return qtrue;
}
