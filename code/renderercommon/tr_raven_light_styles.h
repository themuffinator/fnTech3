/* Independently authored Raven light styles for the Fn renderers.
 * GPL-2.0-or-later. Shared by GLX, Vulkan and RTX: included by each
 * renderer's tr_bsp.c after tr_raven_bsp_load.h, where the world, its
 * lightmap atlas parameters and R_ProcessLightmap are in scope. */
#ifndef FN_RAVEN_LIGHT_STYLES_H
#define FN_RAVEN_LIGHT_STYLES_H

/* Light styles that change. The client game sets each style's colour every
 * frame (RE_SetRavenLightStyle, from Academy's cgame import 220 and
 * Outcast's 216), and the renderer keeps the 64 colours, white until set, as
 * retail's renderer does (tr_raven_bsp_format.h). A changed colour
 * recomposes, before the next world view, the lightmap texels, vertex
 * colours and grid cells its style lights (FN_RavenBspBuildStyles), and
 * re-uploads the touched lightmap pages. The colours outlive a map, as
 * retail's outlive everything but R_Init: a renderer restart starts them
 * white again. */
struct fnRavenStyleWorld_s {
    fnRavenBspStyles_t records;
    byte *pages;                    /* the composed lightmap pages, raw */
    float (**patchCoords)[2];       /* per record surface: a styled patch's tessellated coordinates */
    int *patchVertices;
};

static byte s_ravenStyleColors[FN_RAVEN_BSP_LIGHT_STYLES][4];
static qboolean s_ravenStyleColorsReady;
static uint64_t s_ravenStylesChanged;
static cvar_t *r_debugStyle;

static void R_RavenStyleColorsReady( void ) {
    if ( !s_ravenStyleColorsReady ) {
        Com_Memset( s_ravenStyleColors, 255, sizeof( s_ravenStyleColors ) );
        s_ravenStyleColorsReady = qtrue;
    }
}

void RE_SetRavenLightStyle( int style, const byte color[4] ) {
    R_RavenStyleColorsReady();
    if ( style < 0 || style >= FN_RAVEN_BSP_LIGHT_STYLES || !color ) {
        ri.Error( ERR_DROP, "RE_SetRavenLightStyle: %d is out of range", style );
        return;
    }
    if ( memcmp( s_ravenStyleColors[style], color, 4 ) ) {
        Com_Memcpy( s_ravenStyleColors[style], color, 4 );
        s_ravenStylesChanged |= FN_RavenBspStyleBit( (unsigned)style );
    }
}

static void *R_RavenStyleAllocate( int size ) {
    return ri.Hunk_Alloc( size, h_low );
}

/* After the world's surfaces, lightmaps and light grid are loaded, and
 * before its static vertex buffer is built: styled vertex-lit surfaces are
 * left out of it, as their colours change. */
static void R_RavenInitLightStyles( const lump_t *lightmaps ) {
    world_t *world = &s_worldData;
    struct fnRavenStyleWorld_s *state;
    const fnRavenBspView_t *view = &world->ravenBsp;
    const int pageBytes = FN_RAVEN_BSP_LIGHTMAP_SIZE * FN_RAVEN_BSP_LIGHTMAP_SIZE * 3;
    fnRavenBspError_t error;
    int k;

    R_RavenStyleColorsReady();
    /* Retail single player's renderer diagnostic (jasp.exe and jk2sp.exe,
     * cheat, "-1"): every style black but the one named, which is white. */
    r_debugStyle = ri.Cvar_Get( "r_debugStyle", "-1", CVAR_CHEAT );
    world->ravenStyles = NULL;
    state = (struct fnRavenStyleWorld_s *)ri.Hunk_Alloc( (int)sizeof( *state ), h_low );
    if ( !FN_RavenBspBuildStyles( view, R_RavenStyleAllocate, &state->records, &error ) ) {
        ri.Error( ERR_DROP, "Raven RBSP rendering %s: %s", world->name, error.reason );
        return;
    }
    if ( !state->records.styles ) {
        return;
    }
    if ( state->records.numGroups && tr.lightmaps && lightmaps->filelen >= state->records.numPages * pageBytes ) {
        state->pages = (byte *)ri.Hunk_Alloc( state->records.numPages * pageBytes, h_low );
        Com_Memcpy( state->pages, fileBase + lightmaps->fileofs, state->records.numPages * pageBytes );
    }
    if ( state->records.numSurfaces ) {
        state->patchCoords = (float (**)[2])ri.Hunk_Alloc( state->records.numSurfaces * (int)sizeof( *state->patchCoords ), h_low );
        state->patchVertices = (int *)ri.Hunk_Alloc( state->records.numSurfaces * (int)sizeof( int ), h_low );
    }
    for ( k = 0; k < state->records.numSurfaces; ++k ) {
        const int surface = state->records.surfaces[k].surface;
        const unsigned char *s = view->bytes + view->offsets[13] + surface * 148;
        msurface_t *surf;
        srfGridMesh_t *grid;
        int i;
        if ( surface >= world->numsurfaces ) {
            continue;
        }
        surf = &world->surfaces[surface];
        surf->ravenStyled = qtrue;
        grid = (srfGridMesh_t *)surf->data;
        if ( grid->surfaceType != SF_GRID || !FN_RavenBspStyledPatchAt( view, surface ) ) {
            continue;
        }
        /* Read where each tessellated vertex lies on the patch, then give it
         * the patch's own lightmap coordinates back. */
        state->patchVertices[k] = grid->width * grid->height;
        state->patchCoords[k] = (float (*)[2])ri.Hunk_Alloc( state->patchVertices[k] * (int)sizeof( *state->patchCoords[k] ), h_low );
        for ( i = 0; i < state->patchVertices[k]; ++i ) {
            state->patchCoords[k][i][0] = grid->verts[i].lightmap[0];
            state->patchCoords[k][i][1] = grid->verts[i].lightmap[1];
            FN_RavenBspPatchLightmapCoords( view, s, grid->verts[i].lightmap[0], grid->verts[i].lightmap[1],
                grid->verts[i].lightmap );
        }
    }
    world->ravenStyles = state;
    /* Compose everything once more at the colours now held, which also
     * lights each styled patch where its vertices lie. */
    s_ravenStylesChanged |= state->records.styles;
}

static void R_RavenUploadLightmapPage( int page, const byte *composed ) {
    static byte image[LIGHTMAP_LEN * LIGHTMAP_LEN * 4];
    image_t *target;
    int x = 0, y = 0, size = LIGHTMAP_SIZE;

    if ( !tr.lightmaps ) {
        return;
    }
    R_ProcessLightmap( image, composed, 0.0f );
    if ( tr.mergeLightmaps ) {
        const int cell = page % tr.lightmapMod;
        if ( page / tr.lightmapMod >= tr.numLightmaps ) {
            return;
        }
        target = tr.lightmaps[page / tr.lightmapMod];
        x = ( cell % lightmapCountX ) * LIGHTMAP_LEN;
        y = ( cell / lightmapCountX ) * LIGHTMAP_LEN;
        size = LIGHTMAP_LEN;
    } else {
        if ( page >= tr.numLightmaps ) {
            return;
        }
        target = tr.lightmaps[page];
    }
#ifdef USE_VULKAN
    vk_upload_image_data( target, x, y, size, size, 1, image, size * size * 4, qtrue );
#else
    R_UploadSubImage( image, x, y, size, size, target );
#endif
}

static void R_RavenRecolorSurface( world_t *world, const struct fnRavenStyleWorld_s *state, int k,
        const unsigned char (*colors)[4] ) {
    const fnRavenBspView_t *view = &world->ravenBsp;
    const int surface = state->records.surfaces[k].surface;
    const unsigned char *s = view->bytes + view->offsets[13] + surface * 148;
    const unsigned char *vertices = view->bytes + view->offsets[10] + FN_RavenBspI32( s + 12 ) * 80;
    const int count = FN_RavenBspI32( s + 16 );
    byte composed[4];
    int i;

    if ( surface >= world->numsurfaces ) {
        return;
    }
    switch ( *world->surfaces[surface].data ) {
    case SF_FACE: {
        srfSurfaceFace_t *face = (srfSurfaceFace_t *)world->surfaces[surface].data;
        for ( i = 0; i < face->numPoints && i < count; ++i ) {
            FN_RavenBspComposeVertexColor( vertices + i * 80, s, colors, composed );
            R_ColorShiftLightingBytes( composed, (byte *)&face->points[i][7], qtrue );
        }
        break;
    }
    case SF_TRIANGLES: {
        srfTriangles_t *tri = (srfTriangles_t *)world->surfaces[surface].data;
        for ( i = 0; i < tri->numVerts && i < count; ++i ) {
            FN_RavenBspComposeVertexColor( vertices + i * 80, s, colors, composed );
            R_ColorShiftLightingBytes( composed, tri->verts[i].color.rgba, qtrue );
        }
        break;
    }
    case SF_GRID: {
        srfGridMesh_t *grid = (srfGridMesh_t *)world->surfaces[surface].data;
        if ( !state->patchCoords[k] || state->patchVertices[k] != grid->width * grid->height ) {
            break;
        }
        for ( i = 0; i < state->patchVertices[k]; ++i ) {
            FN_RavenBspPatchColor( view, s, state->patchCoords[k][i][0], state->patchCoords[k][i][1], colors, composed );
            R_ColorShiftLightingBytes( composed, grid->verts[i].color.rgba, qtrue );
        }
        break;
    }
    default:
        break;
    }
}

/* Before a world view: recompose what the styles changed since the last one
 * light. */
void R_RavenUpdateLightStyles( void ) {
    world_t *world = tr.world;
    const struct fnRavenStyleWorld_s *state;
    const unsigned char (*colors)[4] = (const unsigned char (*)[4])s_ravenStyleColors;
    uint64_t changed;
    int i;

    if ( !world || !world->ravenStyles ) {
        return;
    }
    state = world->ravenStyles;
    if ( r_debugStyle && r_debugStyle->integer >= 0 ) {
        static const byte black[4] = { 0, 0, 0, 255 }, white[4] = { 255, 255, 255, 255 };
        for ( i = 0; i < FN_RAVEN_BSP_LIGHT_STYLES; ++i ) {
            RE_SetRavenLightStyle( i, i == r_debugStyle->integer ? white : black );
        }
    }
    changed = s_ravenStylesChanged & state->records.styles;
    s_ravenStylesChanged = 0;
    if ( !changed ) {
        return;
    }
    if ( state->pages ) {
        for ( i = 0; i < state->records.numPages; ++i ) {
            if ( FN_RavenBspRecomposePage( &world->ravenBsp, &state->records, i, changed, colors, state->pages ) ) {
                R_RavenUploadLightmapPage( i, state->pages + i * FN_RAVEN_BSP_LIGHTMAP_SIZE * FN_RAVEN_BSP_LIGHTMAP_SIZE * 3 );
            }
        }
    }
    for ( i = 0; i < state->records.numSurfaces; ++i ) {
        if ( state->records.surfaces[i].styles & changed ) {
            R_RavenRecolorSurface( world, state, i, colors );
        }
    }
    if ( world->lightGridData ) {
        for ( i = 0; i < state->records.numCells; ++i ) {
            const int cell = state->records.cells[i];
            const unsigned row = FN_RavenBspU16( world->ravenBsp.bytes + world->ravenBsp.offsets[17] + cell * 2 );
            const unsigned char *g = world->ravenBsp.bytes + world->ravenBsp.offsets[15] + row * 30;
            byte composed[8];
            if ( !( FN_RavenBspGridStyleMask( g ) & changed ) ) {
                continue;
            }
            FN_RavenBspComposeGrid( g, colors, composed );
            R_ColorShiftLightingBytes( composed, world->lightGridData + cell * 8, qfalse );
            R_ColorShiftLightingBytes( composed + 3, world->lightGridData + cell * 8 + 3, qfalse );
        }
    }
}
#endif
