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
                || FN_RavenBspI32(s + 4) < -1
                || ( type != 4 && FN_RavenBspI32(s + 4) >= view.lengths[12] / 72 ) )
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

/* Light styles. A surface carries up to four lightmap channels and four
 * vertex-colour channels, each with a style; a light-grid row carries four
 * channels too. Retail lights a surface with the sum of its channels, each
 * tinted by its style's colour (OpenJK rd-vanilla, reference observation):
 * - lightmaps: the channels after the first add as their own stages, while
 *   the styles run unbroken below 254 (FinishShader); a channel that is
 *   vertex light (-3) adds the style's colour itself, a white image; the first
 *   channel keeps its own colour;
 * - vertex light (first lightmap channel -3): the sum of each channel's colour
 *   times its style's, shifted down eight bits (ComputeFinalVertexColor);
 * - the light grid: each channel whose style is not 255, times its style's
 *   colour over 255; a first style of 255 is a hole.
 * Fn's renderers draw one channel, so the projection composes the others into
 * it with the colours given. Every style starts white in retail's renderer
 * and stays white until a client game names it (CG_RunLightStyles). */
#define FN_RAVEN_BSP_LIGHT_STYLES 64
#define FN_RAVEN_BSP_LIGHTMAP_SIZE 128

static inline const unsigned char *FN_RavenBspStyleColor( const unsigned char (*colors)[4], unsigned style ) {
    static const unsigned char white[4] = { 255, 255, 255, 255 };
    return colors ? colors[style] : white;
}

/* Lightmap channels after the first that retail draws: while their styles run
 * below 254, and only for a surface whose first channel is a lightmap. */
static inline int FN_RavenBspLightmapStyles( const unsigned char *surface ) {
    int count = 0;
    if ( FN_RavenBspI32( surface + 36 ) < 0 ) return 0;
    while ( count < 3 && surface[29 + count] < 254 ) ++count;
    return count;
}

/* Vertex channels retail sums: a vertex-lit surface's, while below 254. */
static inline int FN_RavenBspVertexStyles( const unsigned char *surface ) {
    int count = 0;
    if ( FN_RavenBspI32( surface + 36 ) != -3 ) return 0;
    while ( count < 4 && surface[32 + count] < 254 ) ++count;
    return count;
}

/* A lightmap channel after the first lies at the first channel's texels moved
 * by a whole number of texels, in every installed map; anything else is
 * refused rather than guessed. */
static inline int FN_RavenBspStyleOffset( const fnRavenBspView_t *view, const unsigned char *s, int slot,
        int offset[2] ) {
    int first = FN_RavenBspI32( s + 12 ), count = FN_RavenBspI32( s + 16 ), v, axis;
    for ( v = first; v < first + count; ++v ) {
        const unsigned char *vertex = view->bytes + view->offsets[10] + v * 80;
        for ( axis = 0; axis < 2; ++axis ) {
            float base, moved;
            double delta;
            memcpy( &base, vertex + 20 + axis * 4, 4 );
            memcpy( &moved, vertex + 20 + slot * 8 + axis * 4, 4 );
            delta = ( (double)moved - (double)base ) * FN_RAVEN_BSP_LIGHTMAP_SIZE;
            if ( !( delta > -4096.0 && delta < 4096.0 ) ) return 0;
            if ( v == first ) offset[axis] = (int)floor( delta + 0.5 );
            if ( fabs( delta - offset[axis] ) > 0.01 ) return 0;
        }
    }
    return 1;
}

static inline int FN_RavenBspCheckStyles( const fnRavenBspView_t *view, fnRavenBspError_t *error ) {
    int i, j, count, offset[2];
    for ( i = 0; i < view->surfaces; ++i ) {
        const unsigned char *s = view->bytes + view->offsets[13] + i * 148;
        count = FN_RavenBspLightmapStyles( s );
        for ( j = 1; j <= count; ++j ) {
            int map = FN_RavenBspI32( s + 36 + j * 4 );
            if ( s[28 + j] >= FN_RAVEN_BSP_LIGHT_STYLES )
                return FN_RavenBspFail( error, "RBSP lightmap style is out of range", 13,i,j,s[28+j] );
            /* retail's FinishShader stops the map on such a channel */
            if ( map < 0 && map != -3 )
                return FN_RavenBspFail( error, "RBSP light style has no lightmap or vertex light", 13,i,j,s[28+j] );
            if ( map >= 0 && !FN_RavenBspStyleOffset( view, s, j, offset ) )
                return FN_RavenBspFail( error, "RBSP lightmap style channel is not its first channel moved", 13,i,j,s[28+j] );
        }
        count = FN_RavenBspVertexStyles( s );
        for ( j = 0; j < count; ++j )
            if ( s[32 + j] >= FN_RAVEN_BSP_LIGHT_STYLES )
                return FN_RavenBspFail( error, "RBSP vertex light style is out of range", 13,i,j,s[32+j] );
    }
    for ( i = 0; i < view->gridRows; ++i ) {
        const unsigned char *g = view->bytes + view->offsets[15] + i * 30;
        if ( g[24] == 255 ) continue; /* Explicit wall/hole marker. */
        for ( j = 0; j < 4; ++j )
            if ( g[24 + j] != 255 && g[24 + j] >= FN_RAVEN_BSP_LIGHT_STYLES )
                return FN_RavenBspFail( error, "RBSP light-grid style is out of range", 15,i,j,g[24+j] );
    }
    return 1;
}

/* Marks the texels a triangle touches, in lightmap texels (conservative: a
 * texel whose square meets the triangle). */
static inline void FN_RavenBspCoverTriangle( unsigned char *mask, const float uv[3][2] ) {
    float lo[2], hi[2];
    int x0, x1, y0, y1, x, y, e, axis;
    if ( ( uv[1][0] - uv[0][0] ) * ( uv[2][1] - uv[0][1] ) == ( uv[2][0] - uv[0][0] ) * ( uv[1][1] - uv[0][1] ) )
        return; /* no area, no texels */
    for ( axis = 0; axis < 2; ++axis ) {
        lo[axis] = hi[axis] = uv[0][axis];
        for ( e = 1; e < 3; ++e ) {
            if ( uv[e][axis] < lo[axis] ) lo[axis] = uv[e][axis];
            if ( uv[e][axis] > hi[axis] ) hi[axis] = uv[e][axis];
        }
    }
    if ( !( lo[0] < FN_RAVEN_BSP_LIGHTMAP_SIZE && hi[0] > 0 && lo[1] < FN_RAVEN_BSP_LIGHTMAP_SIZE && hi[1] > 0 ) ) return;
    x0 = lo[0] < 0 ? 0 : (int)lo[0]; x1 = hi[0] >= FN_RAVEN_BSP_LIGHTMAP_SIZE ? FN_RAVEN_BSP_LIGHTMAP_SIZE - 1 : (int)hi[0];
    y0 = lo[1] < 0 ? 0 : (int)lo[1]; y1 = hi[1] >= FN_RAVEN_BSP_LIGHTMAP_SIZE ? FN_RAVEN_BSP_LIGHTMAP_SIZE - 1 : (int)hi[1];
    for ( y = y0; y <= y1; ++y ) for ( x = x0; x <= x1; ++x ) {
        /* separating axes: each edge's normal; the square's own axes are the box test above */
        int outside = 0;
        for ( e = 0; e < 3 && !outside; ++e ) {
            const float *a = uv[e], *b = uv[(e + 1) % 3], *c = uv[(e + 2) % 3];
            float nx = a[1] - b[1], ny = b[0] - a[0], side = nx * ( c[0] - a[0] ) + ny * ( c[1] - a[1] );
            float best, corner;
            int cx, cy;
            if ( side < 0 ) { nx = -nx; ny = -ny; }
            best = -1e30f;
            for ( cy = 0; cy < 2; ++cy ) for ( cx = 0; cx < 2; ++cx ) {
                corner = nx * ( (float)( x + cx ) - a[0] ) + ny * ( (float)( y + cy ) - a[1] );
                if ( corner > best ) best = corner;
            }
            if ( best < 0 ) outside = 1;
        }
        if ( !outside ) mask[y * FN_RAVEN_BSP_LIGHTMAP_SIZE + x] = 1;
    }
}

/* Marks a surface's first-channel texels: its triangles, or a patch's control
 * grid as triangles. */
static inline void FN_RavenBspCoverSurface( const fnRavenBspView_t *view, const unsigned char *s, unsigned char *mask ) {
    int type = FN_RavenBspI32( s + 8 ), first = FN_RavenBspI32( s + 12 );
    int firstIndex = FN_RavenBspI32( s + 20 ), indexes = FN_RavenBspI32( s + 24 ), i, corner;
    float uv[3][2];
    if ( type == 2 ) {
        int width = FN_RavenBspI32( s + 140 ), height = FN_RavenBspI32( s + 144 ), x, y, tri;
        for ( y = 0; y + 1 < height; ++y ) for ( x = 0; x + 1 < width; ++x ) for ( tri = 0; tri < 2; ++tri ) {
            static const int quad[2][3][2] = { { { 0, 0 }, { 1, 0 }, { 1, 1 } }, { { 0, 0 }, { 1, 1 }, { 0, 1 } } };
            for ( corner = 0; corner < 3; ++corner ) {
                const unsigned char *vertex = view->bytes + view->offsets[10]
                    + ( first + ( y + quad[tri][corner][1] ) * width + x + quad[tri][corner][0] ) * 80;
                memcpy( uv[corner], vertex + 20, 8 );
                uv[corner][0] *= FN_RAVEN_BSP_LIGHTMAP_SIZE; uv[corner][1] *= FN_RAVEN_BSP_LIGHTMAP_SIZE;
            }
            FN_RavenBspCoverTriangle( mask, (const float (*)[2])uv );
        }
        return;
    }
    if ( type != 1 && type != 3 ) return;
    for ( i = 0; i + 2 < indexes; i += 3 ) {
        for ( corner = 0; corner < 3; ++corner ) {
            int index = FN_RavenBspI32( view->bytes + view->offsets[11] + ( firstIndex + i + corner ) * 4 );
            const unsigned char *vertex = view->bytes + view->offsets[10] + ( first + index ) * 80;
            memcpy( uv[corner], vertex + 20, 8 );
            uv[corner][0] *= FN_RAVEN_BSP_LIGHTMAP_SIZE; uv[corner][1] *= FN_RAVEN_BSP_LIGHTMAP_SIZE;
        }
        FN_RavenBspCoverTriangle( mask, (const float (*)[2])uv );
    }
}

/* Two surfaces light the same texels alike when their first channel shares a
 * page and their later channels agree: the same styles from the same places. */
static inline int FN_RavenBspSameStyles( const fnRavenBspView_t *view, const unsigned char *a, const unsigned char *b ) {
    int count = FN_RavenBspLightmapStyles( a ), j, oa[2], ob[2];
    if ( count != FN_RavenBspLightmapStyles( b ) || FN_RavenBspI32( a + 36 ) != FN_RavenBspI32( b + 36 ) ) return 0;
    for ( j = 1; j <= count; ++j ) {
        int map = FN_RavenBspI32( a + 36 + j * 4 );
        if ( a[28 + j] != b[28 + j] || map != FN_RavenBspI32( b + 36 + j * 4 ) ) return 0;
        if ( map >= 0 && ( !FN_RavenBspStyleOffset( view, a, j, oa ) || !FN_RavenBspStyleOffset( view, b, j, ob )
                || oa[0] != ob[0] || oa[1] != ob[1] ) ) return 0;
    }
    return 1;
}

static inline unsigned char FN_RavenBspAddLight( unsigned base, unsigned light, unsigned color ) {
    unsigned sum = base + ( light * color + 127 ) / 255;
    return (unsigned char)( sum > 255 ? 255 : sum );
}

/* Composes every surface's later lightmap channels into its first channel's
 * texels of the projected pages, reading the original pages. Surfaces that
 * light the same texels alike are composed once together. */
static inline void FN_RavenBspComposeLightmaps( const fnRavenBspView_t *view, unsigned char *pages,
        const unsigned char (*colors)[4] ) {
    unsigned char mask[FN_RAVEN_BSP_LIGHTMAP_SIZE * FN_RAVEN_BSP_LIGHTMAP_SIZE];
    const int pageBytes = FN_RAVEN_BSP_LIGHTMAP_SIZE * FN_RAVEN_BSP_LIGHTMAP_SIZE * 3;
    const int numPages = view->lengths[14] / pageBytes;
    int i, j, k, count, texel;
    for ( i = 0; i < view->surfaces; ++i ) {
        const unsigned char *s = view->bytes + view->offsets[13] + i * 148;
        int page = FN_RavenBspI32( s + 36 ), done = 0, offset[2];
        count = FN_RavenBspLightmapStyles( s );
        if ( !count ) continue;
        for ( k = 0; k < i && !done; ++k )
            done = FN_RavenBspSameStyles( view, view->bytes + view->offsets[13] + k * 148, s );
        if ( done || page >= numPages ) continue;
        memset( mask, 0, sizeof( mask ) );
        for ( k = i; k < view->surfaces; ++k ) {
            const unsigned char *other = view->bytes + view->offsets[13] + k * 148;
            if ( k == i || FN_RavenBspSameStyles( view, s, other ) ) FN_RavenBspCoverSurface( view, other, mask );
        }
        for ( j = 1; j <= count; ++j ) {
            const unsigned char *color = FN_RavenBspStyleColor( colors, s[28 + j] );
            int map = FN_RavenBspI32( s + 36 + j * 4 );
            if ( map >= 0 ) FN_RavenBspStyleOffset( view, s, j, offset );
            for ( texel = 0; texel < FN_RAVEN_BSP_LIGHTMAP_SIZE * FN_RAVEN_BSP_LIGHTMAP_SIZE; ++texel ) {
                unsigned char *dest = pages + page * pageBytes + texel * 3;
                int c;
                if ( !mask[texel] ) continue;
                if ( map < 0 ) { /* vertex light: retail's white image */
                    for ( c = 0; c < 3; ++c ) dest[c] = FN_RavenBspAddLight( dest[c], 255, color[c] );
                } else {
                    int x = texel % FN_RAVEN_BSP_LIGHTMAP_SIZE + offset[0], y = texel / FN_RAVEN_BSP_LIGHTMAP_SIZE + offset[1];
                    const unsigned char *src;
                    if ( map >= numPages || x < 0 || y < 0 || x >= FN_RAVEN_BSP_LIGHTMAP_SIZE || y >= FN_RAVEN_BSP_LIGHTMAP_SIZE ) continue;
                    src = view->bytes + view->offsets[14] + map * pageBytes + ( y * FN_RAVEN_BSP_LIGHTMAP_SIZE + x ) * 3;
                    for ( c = 0; c < 3; ++c ) dest[c] = FN_RavenBspAddLight( dest[c], src[c], color[c] );
                }
            }
        }
    }
}

/* A vertex-lit surface's colour: retail's sum over its vertex channels. One
 * whose only channel is the first keeps its colour as the map has it at
 * white, and scales by its style's colour over 255 at others (retail's
 * eight-bit shift would darken it by 1/256 at white). */
static inline void FN_RavenBspComposeVertexColor( const unsigned char *vertex, const unsigned char *s,
        const unsigned char (*colors)[4], unsigned char out[4] ) {
    int count = FN_RavenBspVertexStyles( s ), j, c;
    unsigned sum[3] = { 0, 0, 0 };
    memcpy( out, vertex + 64, 4 );
    if ( count < 1 ) return;
    if ( count == 1 ) {
        const unsigned char *color = FN_RavenBspStyleColor( colors, s[32] );
        for ( c = 0; c < 3; ++c ) out[c] = (unsigned char)( ( (unsigned)vertex[64 + c] * color[c] + 127 ) / 255 );
        return;
    }
    for ( j = 0; j < count; ++j ) {
        const unsigned char *color = FN_RavenBspStyleColor( colors, s[32 + j] );
        for ( c = 0; c < 3; ++c ) sum[c] += (unsigned)vertex[64 + j * 4 + c] * color[c];
    }
    for ( c = 0; c < 3; ++c ) out[c] = (unsigned char)( ( sum[c] >> 8 ) > 255 ? 255 : sum[c] >> 8 );
}

/* A light-grid row's ambient and directed light: each channel whose style is
 * not 255, times its style's colour. */
static inline void FN_RavenBspComposeGrid( const unsigned char *g, const unsigned char (*colors)[4], unsigned char out[6] ) {
    int j, c, part;
    for ( part = 0; part < 2; ++part ) for ( c = 0; c < 3; ++c ) {
        unsigned sum = 0;
        for ( j = 0; j < 4; ++j )
            if ( g[24 + j] != 255 ) sum += (unsigned)g[part * 12 + j * 3 + c] * FN_RavenBspStyleColor( colors, g[24 + j] )[c];
        sum = ( sum + 127 ) / 255;
        out[part * 3 + c] = (unsigned char)( sum > 255 ? 255 : sum );
    }
}

/* Styles that change. Retail's client game sets every style's colour each
 * frame through its renderer's SetLightStyle, which keeps 64 four-byte
 * colours that R_Init starts white (measured: jamp.exe cgame import 220 ->
 * 0x4a3200, table 0x1053650; jasp.exe 97 -> 0x46a730; jk2mp.exe 216 ->
 * 0x480130; jk2sp.exe 87 -> 0x46f900). Fn composes the styles at load; these
 * records name what each style lights, so a renderer recomposes only the
 * texels, vertices and grid cells of a style whose colour changed. What
 * depends on style 0 alone is not recorded: style 0 is the map's normal
 * light, which every Jedi game keeps white. */
typedef struct {
    int page;                   /* the first channel's projected page */
    int surface;                /* one of its surface records: its channels */
    int firstTexel, numTexels;  /* its texels, in fnRavenBspStyles_t.texels */
    uint64_t styles;            /* its later channels' styles */
} fnRavenBspStyleGroup_t;

typedef struct {
    int surface;                /* a vertex-lit surface record */
    uint64_t styles;            /* its vertex channels' styles */
} fnRavenBspStyleSurface_t;

typedef struct {
    int numPages;
    int numGroups;
    fnRavenBspStyleGroup_t *groups;     /* in page order */
    int *pageGroups;                    /* numPages + 1 starts into groups */
    int numTexels;
    unsigned short *texels;             /* texel indices within their page */
    int numSurfaces;
    fnRavenBspStyleSurface_t *surfaces;
    int numCells;
    int *cells;                         /* light-grid cells */
    uint64_t styles;                    /* every style the records depend on */
} fnRavenBspStyles_t;

#define FN_RAVEN_BSP_STYLE_CHANGES( mask ) ( ( mask ) & ~(uint64_t)1 )

static inline uint64_t FN_RavenBspStyleBit( unsigned style ) {
    return style < FN_RAVEN_BSP_LIGHT_STYLES ? (uint64_t)1 << style : 0;
}

static inline uint64_t FN_RavenBspLightmapStyleMask( const unsigned char *s ) {
    int count = FN_RavenBspLightmapStyles( s ), j;
    uint64_t mask = 0;
    for ( j = 1; j <= count; ++j ) mask |= FN_RavenBspStyleBit( s[28 + j] );
    return mask;
}

static inline uint64_t FN_RavenBspVertexStyleMask( const unsigned char *s ) {
    int count = FN_RavenBspVertexStyles( s ), j;
    uint64_t mask = 0;
    for ( j = 0; j < count; ++j ) mask |= FN_RavenBspStyleBit( s[32 + j] );
    return mask;
}

static inline uint64_t FN_RavenBspGridStyleMask( const unsigned char *g ) {
    int j;
    uint64_t mask = 0;
    if ( g[24] == 255 ) return 0;
    for ( j = 0; j < 4; ++j )
        if ( g[24 + j] != 255 ) mask |= FN_RavenBspStyleBit( g[24 + j] );
    return mask;
}

static inline int FN_RavenBspStylePage( const fnRavenBspView_t *view, const unsigned char *s ) {
    const int numPages = view->lengths[14] / ( FN_RAVEN_BSP_LIGHTMAP_SIZE * FN_RAVEN_BSP_LIGHTMAP_SIZE * 3 );
    int page = FN_RavenBspI32( s + 36 );
    return FN_RavenBspLightmapStyles( s ) && page < numPages ? page : -1;
}

static inline void *FN_RavenBspStyleAllocate( void *(*allocate)(int), int count, int size ) {
    void *memory;
    if ( count <= 0 ) return NULL;
    if ( count > INT_MAX / size ) return NULL;
    memory = allocate( count * size );
    if ( memory ) memset( memory, 0, (size_t)count * (size_t)size );
    return memory;
}

/* Records what each changing style lights in a view whose styles
 * FN_RavenBspCheckStyles accepted. The lightmap groups are
 * FN_RavenBspComposeLightmaps's: the surfaces on one page whose later
 * channels agree, with the texels their first channels touch. */
static inline int FN_RavenBspBuildStyles( const fnRavenBspView_t *view, void *(*allocate)(int),
        fnRavenBspStyles_t *out, fnRavenBspError_t *error ) {
    unsigned char mask[FN_RAVEN_BSP_LIGHTMAP_SIZE * FN_RAVEN_BSP_LIGHTMAP_SIZE];
    int *assign = NULL, *leaders = NULL, *next = NULL, *fill = NULL;
    int i, k, g, styled = 0, groups = 0, texels, texel, pass;
    if ( out ) memset( out, 0, sizeof( *out ) );
    if ( !view || !out || !allocate ) return FN_RavenBspFail( error, "RBSP style records have no view or allocator", -1,-1,-1,-1 );
    out->numPages = view->lengths[14] / ( FN_RAVEN_BSP_LIGHTMAP_SIZE * FN_RAVEN_BSP_LIGHTMAP_SIZE * 3 );
    for ( i = 0; i < view->surfaces; ++i ) {
        const unsigned char *s = view->bytes + view->offsets[13] + i * 148;
        if ( FN_RAVEN_BSP_STYLE_CHANGES( FN_RavenBspVertexStyleMask( s ) ) ) ++out->numSurfaces;
        if ( FN_RavenBspStylePage( view, s ) >= 0 ) ++styled;
    }
    for ( i = 0; i < view->gridCells; ++i ) {
        unsigned row = FN_RavenBspU16( view->bytes + view->offsets[17] + i * 2 );
        if ( FN_RAVEN_BSP_STYLE_CHANGES( FN_RavenBspGridStyleMask( view->bytes + view->offsets[15] + row * 30 ) ) )
            ++out->numCells;
    }
    out->surfaces = (fnRavenBspStyleSurface_t *)FN_RavenBspStyleAllocate( allocate, out->numSurfaces, (int)sizeof( *out->surfaces ) );
    out->cells = (int *)FN_RavenBspStyleAllocate( allocate, out->numCells, (int)sizeof( int ) );
    out->pageGroups = (int *)FN_RavenBspStyleAllocate( allocate, out->numPages + 1, (int)sizeof( int ) );
    if ( styled ) {
        assign = (int *)FN_RavenBspStyleAllocate( allocate, view->surfaces, (int)sizeof( int ) );
        next = (int *)FN_RavenBspStyleAllocate( allocate, view->surfaces, (int)sizeof( int ) );
        leaders = (int *)FN_RavenBspStyleAllocate( allocate, styled, (int)sizeof( int ) );
        fill = (int *)FN_RavenBspStyleAllocate( allocate, out->numPages + 1, (int)sizeof( int ) );
        out->groups = (fnRavenBspStyleGroup_t *)FN_RavenBspStyleAllocate( allocate, styled, (int)sizeof( *out->groups ) );
    }
    if ( ( out->numSurfaces && !out->surfaces ) || ( out->numCells && !out->cells ) || !out->pageGroups
            || ( styled && ( !assign || !next || !leaders || !fill || !out->groups ) ) )
        return FN_RavenBspFail( error, "RBSP style record allocation failed", -1,-1,-1,-1 );
    for ( i = k = 0; i < view->surfaces; ++i ) {
        const unsigned char *s = view->bytes + view->offsets[13] + i * 148;
        uint64_t styles = FN_RavenBspVertexStyleMask( s );
        if ( !FN_RAVEN_BSP_STYLE_CHANGES( styles ) ) continue;
        out->surfaces[k].surface = i; out->surfaces[k++].styles = styles;
        out->styles |= styles;
    }
    for ( i = k = 0; i < view->gridCells; ++i ) {
        unsigned row = FN_RavenBspU16( view->bytes + view->offsets[17] + i * 2 );
        uint64_t styles = FN_RavenBspGridStyleMask( view->bytes + view->offsets[15] + row * 30 );
        if ( !FN_RAVEN_BSP_STYLE_CHANGES( styles ) ) continue;
        out->cells[k++] = i;
        out->styles |= styles;
    }
    if ( !styled ) return 1;
    /* Partition the lightmapped surfaces into groups that light alike. */
    for ( i = 0; i < view->surfaces; ++i ) assign[i] = next[i] = -1;
    for ( i = 0; i < view->surfaces; ++i ) {
        const unsigned char *s = view->bytes + view->offsets[13] + i * 148;
        int last = i;
        if ( assign[i] >= 0 || FN_RavenBspStylePage( view, s ) < 0 ) continue;
        assign[i] = groups; leaders[groups++] = i;
        for ( k = i + 1; k < view->surfaces; ++k )
            if ( assign[k] < 0 && FN_RavenBspSameStyles( view, s, view->bytes + view->offsets[13] + k * 148 ) ) {
                assign[k] = assign[i]; next[last] = k; last = k;
            }
    }
    /* In page order, then the order they were found. */
    for ( g = 0; g < groups; ++g )
        ++out->pageGroups[FN_RavenBspStylePage( view, view->bytes + view->offsets[13] + leaders[g] * 148 ) + 1];
    for ( i = 0; i < out->numPages; ++i ) out->pageGroups[i + 1] += out->pageGroups[i];
    for ( g = 0; g < groups; ++g ) {
        const unsigned char *s = view->bytes + view->offsets[13] + leaders[g] * 148;
        int page = FN_RavenBspStylePage( view, s ), slot = out->pageGroups[page] + fill[page]++;
        out->groups[slot].page = page; out->groups[slot].surface = leaders[g];
        out->groups[slot].styles = FN_RavenBspLightmapStyleMask( s );
        out->styles |= out->groups[slot].styles;
    }
    out->numGroups = groups;
    for ( pass = 0; pass < 2; ++pass ) {
        texels = 0;
        for ( g = 0; g < groups; ++g ) {
            fnRavenBspStyleGroup_t *group = &out->groups[g];
            memset( mask, 0, sizeof( mask ) );
            for ( k = group->surface; k >= 0; k = next[k] )
                FN_RavenBspCoverSurface( view, view->bytes + view->offsets[13] + k * 148, mask );
            group->firstTexel = texels;
            for ( texel = 0; texel < FN_RAVEN_BSP_LIGHTMAP_SIZE * FN_RAVEN_BSP_LIGHTMAP_SIZE; ++texel ) {
                if ( !mask[texel] ) continue;
                if ( pass ) out->texels[texels] = (unsigned short)texel;
                ++texels;
            }
            group->numTexels = texels - group->firstTexel;
        }
        if ( !pass ) {
            out->numTexels = texels;
            out->texels = (unsigned short *)FN_RavenBspStyleAllocate( allocate, texels, (int)sizeof( unsigned short ) );
            if ( texels && !out->texels ) return FN_RavenBspFail( error, "RBSP style record allocation failed", -1,-1,-1,-1 );
        }
    }
    return 1;
}

/* Adds a group's later channels into its page of `pages` (the composed
 * pages), at the texels `only` marks, or all of them. */
static inline void FN_RavenBspAddStyleGroup( const fnRavenBspView_t *view, const fnRavenBspStyles_t *styles,
        const fnRavenBspStyleGroup_t *group, const unsigned char (*colors)[4], unsigned char *pages,
        const unsigned char *only ) {
    const int pageBytes = FN_RAVEN_BSP_LIGHTMAP_SIZE * FN_RAVEN_BSP_LIGHTMAP_SIZE * 3;
    const unsigned char *s = view->bytes + view->offsets[13] + group->surface * 148;
    int count = FN_RavenBspLightmapStyles( s ), j, t, c, offset[2] = { 0, 0 };
    for ( j = 1; j <= count; ++j ) {
        const unsigned char *color = FN_RavenBspStyleColor( colors, s[28 + j] );
        int map = FN_RavenBspI32( s + 36 + j * 4 );
        if ( map >= 0 ) FN_RavenBspStyleOffset( view, s, j, offset );
        for ( t = 0; t < group->numTexels; ++t ) {
            int texel = styles->texels[group->firstTexel + t];
            unsigned char *dest = pages + group->page * pageBytes + texel * 3;
            if ( only && !only[texel] ) continue;
            if ( map < 0 ) { /* vertex light: retail's white image */
                for ( c = 0; c < 3; ++c ) dest[c] = FN_RavenBspAddLight( dest[c], 255, color[c] );
            } else {
                int x = texel % FN_RAVEN_BSP_LIGHTMAP_SIZE + offset[0], y = texel / FN_RAVEN_BSP_LIGHTMAP_SIZE + offset[1];
                const unsigned char *src;
                if ( map >= styles->numPages || x < 0 || y < 0 || x >= FN_RAVEN_BSP_LIGHTMAP_SIZE || y >= FN_RAVEN_BSP_LIGHTMAP_SIZE ) continue;
                src = view->bytes + view->offsets[14] + map * pageBytes + ( y * FN_RAVEN_BSP_LIGHTMAP_SIZE + x ) * 3;
                for ( c = 0; c < 3; ++c ) dest[c] = FN_RavenBspAddLight( dest[c], src[c], color[c] );
            }
        }
    }
}

/* Recomposes one page of `pages` where a style in `changed` lights it: those
 * texels start again from the original page, and every group on the page
 * adds its light back there at `colors`. Adding saturates, so the order of
 * the groups does not matter. Returns 1 if the page changed. */
static inline int FN_RavenBspRecomposePage( const fnRavenBspView_t *view, const fnRavenBspStyles_t *styles, int page,
        uint64_t changed, const unsigned char (*colors)[4], unsigned char *pages ) {
    const int pageBytes = FN_RAVEN_BSP_LIGHTMAP_SIZE * FN_RAVEN_BSP_LIGHTMAP_SIZE * 3;
    unsigned char mark[FN_RAVEN_BSP_LIGHTMAP_SIZE * FN_RAVEN_BSP_LIGHTMAP_SIZE];
    int g, t, any = 0;
    if ( !styles->pageGroups || page < 0 || page >= styles->numPages ) return 0;
    memset( mark, 0, sizeof( mark ) );
    for ( g = styles->pageGroups[page]; g < styles->pageGroups[page + 1]; ++g ) {
        const fnRavenBspStyleGroup_t *group = &styles->groups[g];
        if ( !( group->styles & changed ) ) continue;
        for ( t = 0; t < group->numTexels; ++t ) mark[styles->texels[group->firstTexel + t]] = 1;
        any |= group->numTexels > 0;
    }
    if ( !any ) return 0;
    for ( t = 0; t < FN_RAVEN_BSP_LIGHTMAP_SIZE * FN_RAVEN_BSP_LIGHTMAP_SIZE; ++t )
        if ( mark[t] ) memcpy( pages + page * pageBytes + t * 3, view->bytes + view->offsets[14] + page * pageBytes + t * 3, 3 );
    for ( g = styles->pageGroups[page]; g < styles->pageGroups[page + 1]; ++g )
        FN_RavenBspAddStyleGroup( view, styles, &styles->groups[g], colors, pages, mark );
    return 1;
}

/* A vertex-lit patch whose colour follows a changing style. Its control
 * points carry their patch coordinates in the projected lightmap
 * coordinates, which a vertex-lit surface never samples, so each tessellated
 * vertex knows where on the patch it lies and is lit there, channel by
 * channel, as retail's grids carry every channel. */
static inline int FN_RavenBspStyledPatch( const unsigned char *s ) {
    int width = FN_RavenBspI32( s + 140 ), height = FN_RavenBspI32( s + 144 );
    return FN_RavenBspI32( s + 8 ) == 2 && FN_RAVEN_BSP_STYLE_CHANGES( FN_RavenBspVertexStyleMask( s ) )
        && width >= 3 && height >= 3 && ( width & 1 ) && ( height & 1 );
}

/* ... and one whose control points no other surface uses, so carrying the
 * coordinates there changes nothing else (every installed map's). */
static inline int FN_RavenBspStyledPatchAt( const fnRavenBspView_t *view, int surface ) {
    const unsigned char *s = view->bytes + view->offsets[13] + surface * 148;
    int first = FN_RavenBspI32( s + 12 ), count = FN_RavenBspI32( s + 16 ), i;
    if ( !FN_RavenBspStyledPatch( s ) ) return 0;
    for ( i = 0; i < view->surfaces; ++i ) {
        const unsigned char *other = view->bytes + view->offsets[13] + i * 148;
        int otherFirst = FN_RavenBspI32( other + 12 ), otherCount = FN_RavenBspI32( other + 16 );
        if ( i != surface && otherCount > 0 && otherFirst < first + count && first < otherFirst + otherCount ) return 0;
    }
    return 1;
}

/* The nine control points about patch coordinates (u, v) in [0, 1], as
 * vertex numbers from the surface's first, with their biquadratic weights. */
static inline void FN_RavenBspPatchWeights( const unsigned char *s, float u, float v, int index[9], float weight[9] ) {
    int size[2], cell[2], axis, r, c;
    float at[2], t[2], basis[2][3];
    size[0] = FN_RavenBspI32( s + 140 ); size[1] = FN_RavenBspI32( s + 144 );
    at[0] = u; at[1] = v;
    for ( axis = 0; axis < 2; ++axis ) {
        float place = !( at[axis] > 0.0f ) ? 0.0f : at[axis] > 1.0f ? 1.0f : at[axis];
        place *= (float)( size[axis] - 1 );
        cell[axis] = (int)( place * 0.5f );
        if ( cell[axis] > ( size[axis] - 3 ) / 2 ) cell[axis] = ( size[axis] - 3 ) / 2;
        t[axis] = ( place - (float)( cell[axis] * 2 ) ) * 0.5f;
        if ( t[axis] < 0.0f ) t[axis] = 0.0f;
        if ( t[axis] > 1.0f ) t[axis] = 1.0f;
        basis[axis][0] = ( 1.0f - t[axis] ) * ( 1.0f - t[axis] );
        basis[axis][1] = 2.0f * t[axis] * ( 1.0f - t[axis] );
        basis[axis][2] = t[axis] * t[axis];
    }
    for ( r = 0; r < 3; ++r ) for ( c = 0; c < 3; ++c ) {
        index[r * 3 + c] = ( cell[1] * 2 + r ) * size[0] + cell[0] * 2 + c;
        weight[r * 3 + c] = basis[0][c] * basis[1][r];
    }
}

/* A styled patch's colour at patch coordinates (u, v): each channel's colour
 * there, composed as a vertex's. */
static inline void FN_RavenBspPatchColor( const fnRavenBspView_t *view, const unsigned char *s, float u, float v,
        const unsigned char (*colors)[4], unsigned char out[4] ) {
    unsigned char vertex[80];
    int index[9], i, field;
    float weight[9];
    const int first = FN_RavenBspI32( s + 12 );
    FN_RavenBspPatchWeights( s, u, v, index, weight );
    memset( vertex, 0, sizeof( vertex ) );
    for ( field = 64; field < 80; ++field ) {
        float sum = 0.0f;
        for ( i = 0; i < 9; ++i ) sum += weight[i] * (float)view->bytes[view->offsets[10] + ( first + index[i] ) * 80 + field];
        sum += 0.5f;
        vertex[field] = (unsigned char)( sum < 0.0f ? 0 : sum > 255.0f ? 255 : (int)sum );
    }
    FN_RavenBspComposeVertexColor( vertex, s, colors, out );
}

/* A styled patch's own first-channel lightmap coordinates at (u, v), as its
 * tessellated vertices would carry them (non-finite ones count as zero, as
 * in the projection). */
static inline void FN_RavenBspPatchLightmapCoords( const fnRavenBspView_t *view, const unsigned char *s, float u, float v,
        float out[2] ) {
    int index[9], i, axis;
    float weight[9];
    const int first = FN_RavenBspI32( s + 12 );
    FN_RavenBspPatchWeights( s, u, v, index, weight );
    for ( axis = 0; axis < 2; ++axis ) {
        out[axis] = 0.0f;
        for ( i = 0; i < 9; ++i ) {
            const unsigned char *word = view->bytes + view->offsets[10] + ( first + index[i] ) * 80 + 20 + axis * 4;
            float value;
            if ( !FN_RavenBspFinite( word ) ) continue;
            memcpy( &value, word, 4 );
            out[axis] += weight[i] * value;
        }
    }
}

/* A shader record's flag words in the Fn view's Quake III bits. Raven moved
 * every bit: the low five surface bits are a material number and sky is
 * 0x2000, so Quake III's tests would read mp/duel9's snow (material 14,
 * 0xe) as sky. The Raven values are the renderer infoparm tables of all
 * four retail engines (jamp.exe 1.01 0x56cfc8, jasp.exe 1.01 0x4f3440,
 * jk2mp.exe 0x502200, jk2sp.exe 0x4cf5a8), as OpenJK's surfaceflags.h has
 * them; each engine's ParseMesh skips a patch on 0x200000 (jamp.exe
 * 0x48d610). A
 * meaning Quake III lacks (materials, forcefield, forcesight, opaque,
 * ladder, ...) is left out; the original records keep every bit. */
static inline uint32_t FN_RavenBspFlagsToFn( uint32_t raven, int contents ) {
    static const uint32_t surface[][2] = {
        { 0x00002000u, 0x00000004u }, /* sky */        { 0x00004000u, 0x00000002u }, /* slick */
        { 0x00008000u, 0x00001000u }, /* metalsteps */ { 0x00040000u, 0x00000001u }, /* nodamage */
        { 0x00080000u, 0x00000010u }, /* noimpact */   { 0x00100000u, 0x00000020u }, /* nomarks */
        { 0x00200000u, 0x00000080u }, /* nodraw */     { 0x00400000u, 0x00002000u }, /* nosteps */
        { 0x00800000u, 0x00020000u }, /* nodlight */
    };
    static const uint32_t content[][2] = {
        { 0x00000001u, 0x00000001u }, /* solid */      { 0x00000002u, 0x00000008u }, /* lava */
        { 0x00000004u, 0x00000020u }, /* water */      { 0x00000008u, 0x00000040u }, /* fog */
        { 0x00000010u, 0x00010000u }, /* playerclip */ { 0x00000020u, 0x00020000u }, /* monsterclip */
        { 0x00000040u, 0x00400000u }, /* botclip */    { 0x00000400u, 0x40000000u }, /* trigger */
        { 0x00000800u, 0x80000000u }, /* nodrop */     { 0x00020000u, 0x00000010u }, /* slime */
        { 0x08000000u, 0x08000000u }, /* detail */     { 0x80000000u, 0x20000000u }, /* trans */
    };
    const uint32_t (*table)[2] = contents ? content : surface;
    const size_t count = contents ? sizeof( content ) / sizeof( content[0] ) : sizeof( surface ) / sizeof( surface[0] );
    uint32_t fn = 0;
    size_t i;
    for ( i = 0; i < count; ++i )
        if ( raven & table[i][0] ) fn |= table[i][1];
    return fn;
}

static inline int FN_RavenBspAppend( int *length, int count, int *offset ) {
    if ( count < 0 || *length > INT_MAX - 3 ) return 0;
    *offset = (*length + 3) & ~3;
    if ( count > INT_MAX - *offset ) return 0;
    *length = *offset + count;
    return 1;
}

/* The projection with every style at the colours given (NULL: white). */
static inline int FN_RavenBspProjectStyles( const void *data, int length, void *(*allocate)(int),
        const unsigned char (*colors)[4], fnRavenBspProjection_t *out, fnRavenBspError_t *error ) {
    fnRavenBspView_t view;
    fnRavenBspProjection_t result;
    unsigned char *buffer;
    int sizes[17], offsets[17], total = 144, rawOffset, i;
    if ( out ) memset( out, 0, sizeof( *out ) );
    if ( !out || !allocate ) return FN_RavenBspFail(error,"RBSP rendering allocation target is missing",-1,-1,-1,-1);
    if ( !FN_RavenBspRead(data,length,&view,error) || !FN_RavenBspCheckStyles(&view,error) ) return 0;
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
    for ( i = 0; i < view.lengths[1] / 72; ++i ) {
        unsigned char *d = buffer+offsets[1]+i*72;
        FN_RavenBspPut32(d+64,FN_RavenBspFlagsToFn(FN_RavenBspU32(d+64),0));
        FN_RavenBspPut32(d+68,FN_RavenBspFlagsToFn(FN_RavenBspU32(d+68),1));
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
        int vertex, first = FN_RavenBspI32(s+12), count = FN_RavenBspI32(s+16);
        if ( FN_RavenBspVertexStyles(s) < 2 ) continue;
        for ( vertex = first; vertex < first + count; ++vertex )
            FN_RavenBspComposeVertexColor(view.bytes+view.offsets[10]+vertex*80,s,colors,buffer+offsets[10]+vertex*44+40);
    }
    /* A styled vertex-lit patch's control points carry their patch
     * coordinates (FN_RavenBspStyledPatchAt); the renderer puts the patch's own
     * coordinates back once it has read them from its tessellation. */
    for ( i = 0; i < view.surfaces; ++i ) {
        const unsigned char *s = view.bytes+view.offsets[13]+i*148;
        int x, y, width = FN_RavenBspI32(s+140), height = FN_RavenBspI32(s+144), first = FN_RavenBspI32(s+12);
        if ( !FN_RavenBspStyledPatchAt(&view,i) ) continue;
        for ( y = 0; y < height; ++y ) for ( x = 0; x < width; ++x ) {
            float at[2];
            at[0] = (float)x / (float)( width - 1 ); at[1] = (float)y / (float)( height - 1 );
            memcpy(buffer+offsets[10]+(first+y*width+x)*44+20,at,8);
        }
    }
    FN_RavenBspComposeLightmaps(&view,buffer+offsets[14],colors);
    for ( i = 0; i < view.surfaces; ++i ) {
        const unsigned char *s = view.bytes+view.offsets[13]+i*148;
        unsigned char *d = buffer+offsets[13]+i*104;
        memcpy(d,s,28); memcpy(d+28,s+36,4); memcpy(d+32,s+52,4); memcpy(d+36,s+68,4);
        /* Outcast's yavin_temple has 11 flares in fog 0 of a map with no fog;
         * OpenJK's ParseFlare reads the number unchecked. They are drawn unfogged. */
        if ( FN_RavenBspI32(s+8) == 4 && FN_RavenBspI32(s+4) >= view.lengths[12] / 72 )
            FN_RavenBspPut32(d+4,UINT32_C(0xffffffff));
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
        if ( g[24] != 255 ) FN_RavenBspComposeGrid(g,colors,d);
        memcpy(d+6,g+28,2);
    }
    memcpy(buffer+rawOffset,data,(size_t)length);
    memset(&result,0,sizeof(result)); result.data = buffer; result.length = total; result.rawOffset = rawOffset;
    result.original = view; result.original.bytes = buffer+rawOffset;
    *out = result;
    return 1;
}

static inline int FN_RavenBspProject( const void *data, int length, void *(*allocate)(int),
        fnRavenBspProjection_t *out, fnRavenBspError_t *error ) {
    return FN_RavenBspProjectStyles( data, length, allocate, NULL, out, error );
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
