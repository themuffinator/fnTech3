/*
Copyright (C) 2026 fnTech3 contributors
GPL version 2 or (at your option) any later version.

Independently authored rendering reader for Raven RBSP 1. Layout facts:
OpenJK 1a6a643427aa347553e9073dac5570b33337c4d9,
codemp/qcommon/qfiles.h. No external implementation text is incorporated.
The original eighteen lumps remain intact beside the canonical geometry view;
the collision-only bsp_rbsp.h translator is deliberately not used here.
*/
#ifndef FN_RAVEN_BSP_FORMAT_H
#define FN_RAVEN_BSP_FORMAT_H

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define FN_RAVEN_BSP_LUMPS 18
#define FN_RAVEN_BSP_HEADER_BYTES 152
#define FN_RAVEN_BSP_MAX_GRID_ROWS 65535
#define FN_RAVEN_BSP_MAX_GRID_CELLS 1048576
#define FN_RAVEN_BSP_MAX_BYTES (256 * 1024 * 1024)

typedef struct {
    const unsigned char *bytes;
    int length;
    int offsets[FN_RAVEN_BSP_LUMPS];
    int lengths[FN_RAVEN_BSP_LUMPS];
    int vertices, surfaces, gridRows, gridCells;
} fnRavenBspView_t;

typedef struct {
    const char *reason;
    int lump, record, slot, style;
} fnRavenBspError_t;

typedef struct {
    void *data;
    int length, rawOffset;
    fnRavenBspView_t original;
} fnRavenBspProjection_t;

static inline uint32_t FN_RavenBspU32( const unsigned char *p ) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline int32_t FN_RavenBspI32( const unsigned char *p ) {
    uint32_t word = FN_RavenBspU32( p );
    int32_t value;
    memcpy( &value, &word, sizeof( value ) );
    return value;
}
static inline unsigned FN_RavenBspU16( const unsigned char *p ) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}
static inline void FN_RavenBspPut32( unsigned char *p, uint32_t value ) {
    int i;
    for ( i = 0; i < 4; ++i ) p[i] = (unsigned char)( value >> (8 * i) );
}
/* Integer encoding checks remain effective under the engine's fast-math flags. */
static inline int FN_RavenBspFinite( const unsigned char *p ) {
    volatile uint32_t encoding = FN_RavenBspU32( p );
    return (encoding & UINT32_C(0x7f800000)) != UINT32_C(0x7f800000);
}
static inline int FN_RavenBspFloatFinite( float value ) {
    uint32_t bits;
    volatile uint32_t encoding;
    memcpy( &bits, &value, sizeof( bits ) );
    encoding = bits;
    return (encoding & UINT32_C(0x7f800000)) != UINT32_C(0x7f800000);
}
static inline int FN_RavenBspFail( fnRavenBspError_t *error, const char *reason,
        int lump, int record, int slot, int style ) {
    if ( error ) {
        error->reason = reason; error->lump = lump; error->record = record;
        error->slot = slot; error->style = style;
    }
    return 0;
}
static inline int FN_RavenBspRange( int first, int count, int maximum ) {
    return first >= 0 && count >= 0 && first <= maximum && count <= maximum - first;
}

static inline int FN_RavenBspRead( const void *data, int length,
        fnRavenBspView_t *out, fnRavenBspError_t *error ) {
    /* Strides are disk records, not any renderer's expanded internal types. */
    const int strides[18] = { 1,72,16,36,48,4,4,40,12,12,80,4,72,148,49152,30,1,2 };
    fnRavenBspView_t view;
    const unsigned char *bytes = (const unsigned char *)data;
    int i, j;
    if ( out ) memset( out, 0, sizeof( *out ) );
    if ( error ) memset( error, 0, sizeof( *error ) );
    if ( !out || !bytes || length < 152 || length > FN_RAVEN_BSP_MAX_BYTES )
        return FN_RavenBspFail( error, "RBSP rendering header/size is invalid", -1,-1,-1,-1 );
    if ( FN_RavenBspU32( bytes ) != UINT32_C(0x50534252) || FN_RavenBspU32( bytes + 4 ) != 1 )
        return FN_RavenBspFail( error, "RBSP rendering identifier/version is invalid", -1,-1,-1,-1 );
    memset( &view, 0, sizeof( view ) );
    view.bytes = bytes; view.length = length;
    for ( i = 0; i < 18; ++i ) {
        int offset = FN_RavenBspI32( bytes + 8 + i * 8 );
        int count = FN_RavenBspI32( bytes + 12 + i * 8 );
        if ( !FN_RavenBspRange( offset, count, length ) || (count && offset < 152) || count % strides[i] )
            return FN_RavenBspFail( error, "RBSP rendering lump bounds/record size is invalid", i,-1,-1,-1 );
        view.offsets[i] = offset; view.lengths[i] = count;
        for ( j = 0; j < i; ++j ) {
            if ( count && view.lengths[j] && offset < view.offsets[j] + view.lengths[j]
                    && view.offsets[j] < offset + count )
                return FN_RavenBspFail( error, "RBSP rendering lumps overlap", i,-1,-1,-1 );
        }
    }
    view.vertices = view.lengths[10] / 80; view.surfaces = view.lengths[13] / 148;
    view.gridRows = view.lengths[15] / 30; view.gridCells = view.lengths[17] / 2;
    if ( view.vertices > 0x80000 || view.surfaces > 0x20000
            || view.gridRows > FN_RAVEN_BSP_MAX_GRID_ROWS || view.gridCells > FN_RAVEN_BSP_MAX_GRID_CELLS
            || !view.lengths[7] || view.lengths[7] / 40 > 0x400 )
        return FN_RavenBspFail( error, "RBSP rendering record counts exceed format bounds", -1,-1,-1,-1 );
    if ( !view.lengths[0] || bytes[view.offsets[0] + view.lengths[0] - 1] != 0 )
        return FN_RavenBspFail( error, "RBSP rendering entity text is not terminated", 0,-1,-1,-1 );
    if ( (!view.gridRows) != (!view.gridCells) )
        return FN_RavenBspFail( error, "RBSP rendering grid pool/array is incomplete", 17,-1,-1,-1 );
    for ( i = 0; i < view.gridCells; ++i )
        if ( FN_RavenBspU16( bytes + view.offsets[17] + i * 2 ) >= (unsigned)view.gridRows )
            return FN_RavenBspFail( error, "RBSP rendering light-array index exceeds its pool", 17,i,-1,-1 );
    for ( i = 0; i < view.lengths[2] / 16; ++i )
        for ( j = 0; j < 4; ++j )
            if ( !FN_RavenBspFinite( bytes + view.offsets[2] + i * 16 + j * 4 ) )
                return FN_RavenBspFail( error, "RBSP rendering plane contains a non-finite scalar", 2,i,j,-1 );
    for ( i = 0; i < view.vertices; ++i ) {
        const unsigned char *v = bytes + view.offsets[10] + i * 80;
        /* Inactive lightmap channels can contain uninitialized NaN encodings
         * in installed Raven maps. Geometry is always consumed; lightmap
         * coordinates are checked below against each surface's active maps. */
        for ( j = 0; j < 16; ++j )
            if ( (j < 5 || j > 12) && !FN_RavenBspFinite( v + j * 4 ) )
                return FN_RavenBspFail( error, "RBSP rendering vertex contains a non-finite scalar", 10,i,j,-1 );
    }
    for ( i = 0; i < view.surfaces; ++i ) {
        const unsigned char *s = bytes + view.offsets[13] + i * 148;
        int first = FN_RavenBspI32( s + 12 ), count = FN_RavenBspI32( s + 16 );
        int firstIndex = FN_RavenBspI32( s + 20 ), indexes = FN_RavenBspI32( s + 24 );
        int type = FN_RavenBspI32( s + 8 );
        if ( type < 1 || type > 4 || !FN_RavenBspRange( first,count,view.vertices )
                || !FN_RavenBspRange( firstIndex,indexes,view.lengths[11] / 4 )
                || !FN_RavenBspRange( FN_RavenBspI32(s),1,view.lengths[1] / 72 )
                || FN_RavenBspI32(s + 4) < -1 || FN_RavenBspI32(s + 4) >= view.lengths[12] / 72 )
            return FN_RavenBspFail( error, "RBSP rendering surface/index range is invalid", 13,i,-1,-1 );
        if ( (type == 1 || type == 3) && (!count || indexes % 3) )
            return FN_RavenBspFail( error, "RBSP rendering triangle count is invalid", 13,i,-1,-1 );
        if ( type == 2 ) {
            int width = FN_RavenBspI32(s+140), height = FN_RavenBspI32(s+144);
            if ( width < 1 || height < 1 || width > 129 || height > 129
                    || width * height > 32 * 32 || width * height > count )
                return FN_RavenBspFail( error, "RBSP rendering patch dimensions exceed its control points", 13,i,-1,-1 );
        }
        for ( j = 0; j < indexes; ++j )
            if ( !FN_RavenBspRange( FN_RavenBspI32(bytes + view.offsets[11] + (firstIndex+j)*4),1,count ) )
                return FN_RavenBspFail( error, "RBSP rendering surface vertex index is invalid", 11,firstIndex+j,-1,-1 );
        for ( j = 0; j < 12; ++j )
            /* The origin/vectors have different meanings for each type:
             * planar normal, patch LOD bounds, or flare position/color/normal.
             * Unused planar origins also contain NaNs in installed maps. */
            if ( ((type == 1 && j >= 9) || (type == 2 && j >= 3 && j < 9)
                    || (type == 4 && (j < 6 || j >= 9))) && !FN_RavenBspFinite( s + 92 + j * 4 ) )
                return FN_RavenBspFail( error, "RBSP rendering surface contains a non-finite scalar", 13,i,j,-1 );
        for ( j = 0; j < 4; ++j ) {
            int map = FN_RavenBspI32( s + 36 + j * 4 );
            if ( map < -4 || (map >= 0 && map >= view.lengths[14] / 49152) )
                return FN_RavenBspFail( error, "RBSP rendering lightmap index is invalid", 13,i,j,-1 );
            if ( map >= 0 && s[28+j] < 254 ) {
                int vertex;
                for ( vertex = first; vertex < first + count; ++vertex ) {
                    const unsigned char *uv = bytes + view.offsets[10] + vertex * 80 + 20 + j * 8;
                    if ( !FN_RavenBspFinite(uv) || !FN_RavenBspFinite(uv+4) )
                        return FN_RavenBspFail( error, "RBSP rendering active lightmap coordinates are non-finite",
                            10,vertex,j,s[28+j] );
                }
            }
        }
    }
    for ( i = 0; i < view.lengths[7] / 40; ++i ) {
        const unsigned char *model = bytes + view.offsets[7] + i * 40;
        for ( j = 0; j < 6; ++j )
            if ( !FN_RavenBspFinite( model + j * 4 ) )
                return FN_RavenBspFail( error, "RBSP rendering model bounds are non-finite", 7,i,j,-1 );
        if ( !FN_RavenBspRange(FN_RavenBspI32(model+24),FN_RavenBspI32(model+28),view.surfaces)
                || !FN_RavenBspRange(FN_RavenBspI32(model+32),FN_RavenBspI32(model+36),view.lengths[8]/12) )
            return FN_RavenBspFail( error, "RBSP rendering model ranges are invalid", 7,i,-1,-1 );
    }
    *out = view;
    return 1;
}

/* Current Fn material records have one lightmap/vertex-light channel. A second
 * active channel, including another normal style, requires an actual material
 * adapter; never silently project it into the first channel. All original
 * channels still remain available in the retained map. */
static inline int FN_RavenBspNormalStyles( const fnRavenBspView_t *view, fnRavenBspError_t *error ) {
    int i, j, group;
    for ( i = 0; i < view->surfaces; ++i ) {
        const unsigned char *s = view->bytes + view->offsets[13] + i * 148;
        for ( group = 0; group < 2; ++group ) for ( j = 0; j < 4; ++j ) {
            unsigned style = s[28 + group * 4 + j];
            if ( (j == 0 && style != 0) || (j != 0 && style < 254) )
                return FN_RavenBspFail( error, group ? "RBSP vertex light styles are not served yet"
                    : "RBSP surface lightmap styles are not served yet", 13,i,j,(int)style );
        }
    }
    for ( i = 0; i < view->gridRows; ++i ) {
        const unsigned char *g = view->bytes + view->offsets[15] + i * 30;
        if ( g[24] == 255 ) continue; /* Explicit wall/hole marker. */
        for ( j = 0; j < 4; ++j )
            if ( (j == 0 && g[24+j] != 0) || (j != 0 && g[24+j] != 255) )
                return FN_RavenBspFail( error, "RBSP light-grid styles are not served yet", 15,i,j,g[24+j] );
    }
    return 1;
}

static inline int FN_RavenBspAppend( int *length, int count, int *offset ) {
    if ( count < 0 || *length > INT_MAX - 3 ) return 0;
    *offset = (*length + 3) & ~3;
    if ( count > INT_MAX - *offset ) return 0;
    *length = *offset + count;
    return 1;
}

static inline int FN_RavenBspProject( const void *data, int length, void *(*allocate)(int),
        fnRavenBspProjection_t *out, fnRavenBspError_t *error ) {
    fnRavenBspView_t view;
    fnRavenBspProjection_t result;
    unsigned char *buffer;
    int sizes[17], offsets[17], total = 144, rawOffset, i;
    if ( out ) memset( out, 0, sizeof( *out ) );
    if ( !out || !allocate ) return FN_RavenBspFail(error,"RBSP rendering allocation target is missing",-1,-1,-1,-1);
    if ( !FN_RavenBspRead(data,length,&view,error) || !FN_RavenBspNormalStyles(&view,error) ) return 0;
    for ( i = 0; i < 17; ++i ) sizes[i] = view.lengths[i];
    sizes[9] = (view.lengths[9] / 12) * 8;
    sizes[10] = view.vertices * 44; sizes[13] = view.surfaces * 104; sizes[15] = view.gridCells * 8;
    for ( i = 0; i < 17; ++i )
        if ( !FN_RavenBspAppend(&total,sizes[i],&offsets[i]) )
            return FN_RavenBspFail(error,"RBSP rendering projection size overflows",i,-1,-1,-1);
    if ( !FN_RavenBspAppend(&total,length,&rawOffset) )
        return FN_RavenBspFail(error,"RBSP retained rendering records exceed address bounds",-1,-1,-1,-1);
    buffer = (unsigned char *)allocate(total);
    if ( !buffer ) return FN_RavenBspFail(error,"RBSP rendering projection allocation failed",-1,-1,-1,-1);
    memset(buffer,0,(size_t)total);
    FN_RavenBspPut32(buffer,UINT32_C(0x50534249)); FN_RavenBspPut32(buffer+4,46);
    for ( i = 0; i < 17; ++i ) {
        FN_RavenBspPut32(buffer+8+i*8,(uint32_t)offsets[i]);
        FN_RavenBspPut32(buffer+12+i*8,(uint32_t)sizes[i]);
        if ( i != 9 && i != 10 && i != 13 && i != 15 && sizes[i] )
            memcpy(buffer+offsets[i],view.bytes+view.offsets[i],(size_t)sizes[i]);
    }
    for ( i = 0; i < view.lengths[9] / 12; ++i )
        memcpy(buffer+offsets[9]+i*8,view.bytes+view.offsets[9]+i*12,8);
    for ( i = 0; i < view.vertices; ++i ) {
        const unsigned char *v = view.bytes+view.offsets[10]+i*80;
        unsigned char *d = buffer+offsets[10]+i*44;
        memcpy(d,v,20);
        /* Active channel-zero coordinates passed the surface checks. Unused
         * non-finite coordinates are sanitized only in the Fn geometry view,
         * so patch interpolation never consumes them. The raw map below
         * retains every original bit, including the inactive channels. */
        if ( FN_RavenBspFinite(v+20) ) memcpy(d+20,v+20,4);
        if ( FN_RavenBspFinite(v+24) ) memcpy(d+24,v+24,4);
        memcpy(d+28,v+52,12); memcpy(d+40,v+64,4);
    }
    for ( i = 0; i < view.surfaces; ++i ) {
        const unsigned char *s = view.bytes+view.offsets[13]+i*148;
        unsigned char *d = buffer+offsets[13]+i*104;
        memcpy(d,s,28); memcpy(d+28,s+36,4); memcpy(d+32,s+52,4); memcpy(d+36,s+68,4);
        memcpy(d+40,s+84,8);
        {
            int scalar;
            for ( scalar = 0; scalar < 12; ++scalar )
                if ( FN_RavenBspFinite(s+92+scalar*4) ) memcpy(d+48+scalar*4,s+92+scalar*4,4);
        }
        memcpy(d+96,s+140,8);
    }
    for ( i = 0; i < view.gridCells; ++i ) {
        unsigned row = FN_RavenBspU16(view.bytes+view.offsets[17]+i*2);
        const unsigned char *g = view.bytes+view.offsets[15]+row*30;
        unsigned char *d = buffer+offsets[15]+i*8;
        if ( g[24] != 255 ) { memcpy(d,g,3); memcpy(d+3,g+12,3); }
        memcpy(d+6,g+28,2);
    }
    memcpy(buffer+rawOffset,data,(size_t)length);
    memset(&result,0,sizeof(result)); result.data = buffer; result.length = total; result.rawOffset = rawOffset;
    result.original = view; result.original.bytes = buffer+rawOffset;
    *out = result;
    return 1;
}

/* Cell order is the original light-array order, including repeated/permuted
 * pool indices. Black ambient is a valid Raven sample: only styles[0]==255 is
 * a hole. Do not borrow Quake III's zero-ambient hole heuristic. */
static inline int FN_RavenBspGridCellValid( const fnRavenBspView_t *view, size_t cell ) {
    unsigned row;
    if ( !view || !view->bytes || cell >= (size_t)view->gridCells ) return 0;
    row = FN_RavenBspU16(view->bytes+view->offsets[17]+cell*2);
    return row < (unsigned)view->gridRows && view->bytes[view->offsets[15]+row*30+24] != 255;
}

static inline int FN_RavenBspGridGeometry( const float mins[3], const float maxs[3], const float spacing[3],
        float origin[3], int bounds[3], int expectedCells ) {
    int axis, cells = 1;
    for ( axis = 0; axis < 3; ++axis ) {
        float lowRatio, highRatio, highOrigin, extent;
        double count;
        if ( !FN_RavenBspFloatFinite(mins[axis]) || !FN_RavenBspFloatFinite(maxs[axis])
                || !FN_RavenBspFloatFinite(spacing[axis]) || spacing[axis] <= 0 || mins[axis] > maxs[axis] ) return 0;
        lowRatio = mins[axis] / spacing[axis]; highRatio = maxs[axis] / spacing[axis];
        if ( !FN_RavenBspFloatFinite(lowRatio) || !FN_RavenBspFloatFinite(highRatio) ) return 0;
        origin[axis] = (float)(spacing[axis] * ceil(lowRatio));
        highOrigin = (float)(spacing[axis] * floor(highRatio));
        if ( !FN_RavenBspFloatFinite(origin[axis]) || !FN_RavenBspFloatFinite(highOrigin) ) return 0;
        extent = (highOrigin-origin[axis]) / spacing[axis];
        if ( !FN_RavenBspFloatFinite(extent) ) return 0;
        count = (double)extent + 1;
        if ( count < 1 || count > FN_RAVEN_BSP_MAX_GRID_CELLS || count != floor(count) ) return 0;
        bounds[axis] = (int)count;
        if ( cells > FN_RAVEN_BSP_MAX_GRID_CELLS / bounds[axis] ) return 0;
        cells *= bounds[axis];
    }
    return cells == expectedCells;
}
#endif
