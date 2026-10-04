/* Independently authored RBSP-to-Fn rendering boundary. GPL-2.0-or-later.
 * Shared by GLX, Vulkan and RTX; material/static-light/grid presentation
 * remains owned by those Fn pipelines. The original records are kept in
 * world memory, including every style channel and compressed array index. */
#ifndef FN_RAVEN_BSP_LOAD_H
#define FN_RAVEN_BSP_LOAD_H

static void R_RavenPrepareWorldBuffer( void **buffer, int *length, fnRavenBspView_t *original, const char *name ) {
    fnRavenBspProjection_t projection;
    fnRavenBspError_t error;
    if ( !FN_RavenBspProject( *buffer, *length, ri.Malloc, &projection, &error ) ) {
        ri.Error( ERR_DROP, "Raven RBSP rendering %s: %s (lump %i record %i slot %i style %i)",
            name, error.reason, error.lump, error.record, error.slot, error.style );
        return;
    }
    ri.FS_FreeFile( *buffer );
    *buffer = projection.data; *length = projection.length;
    *original = projection.original;
}

static void R_RavenKeepWorldRecords( const fnRavenBspView_t *original ) {
    byte *copy = ri.Hunk_Alloc( original->length, h_low );
    Com_Memcpy( copy, original->bytes, original->length );
    s_worldData.ravenBsp = *original;
    s_worldData.ravenBsp.bytes = copy;
}

static void R_LoadRavenLightGrid( const lump_t *lump ) {
    world_t *world = &s_worldData;
    int cell;
    if ( !world->ravenBsp.gridRows && !world->ravenBsp.gridCells ) {
        world->lightGridData = NULL; /* Asset explicitly contains no grid. */
        return;
    }
    if ( !world->numBmodels || !world->bmodels || lump->filelen != world->ravenBsp.gridCells * 8
            || !FN_RavenBspGridGeometry( world->bmodels[0].bounds[0], world->bmodels[0].bounds[1],
                world->lightGridSize, world->lightGridOrigin, world->lightGridBounds, world->ravenBsp.gridCells ) ) {
        ri.Error( ERR_DROP, "Raven RBSP rendering %s: light-array length does not match finite world/grid bounds",
            world->name );
        return;
    }
    for ( cell = 0; cell < 3; ++cell ) world->lightGridInverseSize[cell] = 1.0f / world->lightGridSize[cell];
    world->lightGridData = ri.Hunk_Alloc( lump->filelen, h_low );
    Com_Memcpy( world->lightGridData, fileBase + lump->fileofs, lump->filelen );
    for ( cell = 0; cell < world->ravenBsp.gridCells; ++cell ) {
        R_ColorShiftLightingBytes( world->lightGridData + cell * 8, world->lightGridData + cell * 8, qfalse );
        R_ColorShiftLightingBytes( world->lightGridData + cell * 8 + 3, world->lightGridData + cell * 8 + 3, qfalse );
    }
}
#endif
