/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_MODEL_LIGHT_BSP_H
#define FNTECH3_TR_Q3_IHV_MODEL_LIGHT_BSP_H

/* Called after the validated BSP43 translation and normal world loaders.
 * Keep private, unshifted RGB and original face coordinates: texture atlas
 * packing, overbright controls and renderer geometry must not alter samples.
 * The hunk owns every record until world teardown. */
static void R_LoadQ3IHVModelLighting(const lump_t *surfaceLump,
    const lump_t *vertexLump, const lump_t *lightmapLump) {
    q3IHVModelLightNode_t *nodes;
    q3IHVModelLightFace_t *faces;
    const q3IHVModelLightFace_t **marks;
    const dsurface_t *input;
    const drawVert_t *vertices;
    byte *pixels;
    int i, a, vertexCount, pageCount;
    if (ri.gameId != COM_GAME_QUAKE3_IHV) return;
    if (s_worldData.numnodes <= 0 || s_worldData.numnodes > MAX_MAP_NODES + MAX_MAP_LEAFS
        || !s_worldData.nodes || s_worldData.numsurfaces < 0 || s_worldData.numsurfaces > MAX_MAP_DRAW_SURFS
        || s_worldData.nummarksurfaces < 0 || s_worldData.nummarksurfaces > MAX_MAP_LEAFFACES
        || surfaceLump->filelen < 0 || surfaceLump->filelen % sizeof(dsurface_t)
        || vertexLump->filelen < 0 || lightmapLump->filelen < 0
        || s_worldData.numsurfaces != surfaceLump->filelen / (int)sizeof(dsurface_t)
        || vertexLump->filelen % sizeof(drawVert_t) || lightmapLump->filelen % (128 * 128 * 3)) {
        ri.Error(ERR_DROP, "IHV model lighting: invalid loaded world"); return;
    }
    vertexCount = vertexLump->filelen / sizeof(drawVert_t);
    pageCount = lightmapLump->filelen / (128 * 128 * 3);
    input = (const dsurface_t *)(fileBase + surfaceLump->fileofs);
    vertices = (const drawVert_t *)(fileBase + vertexLump->fileofs);
    nodes = (q3IHVModelLightNode_t *)ri.Hunk_Alloc(s_worldData.numnodes * sizeof(*nodes), h_low);
    faces = (q3IHVModelLightFace_t *)ri.Hunk_Alloc(s_worldData.numsurfaces * sizeof(*faces), h_low);
    marks = (const q3IHVModelLightFace_t **)ri.Hunk_Alloc(s_worldData.nummarksurfaces * sizeof(*marks), h_low);
    pixels = (byte *)ri.Hunk_Alloc(lightmapLump->filelen, h_low);
    if (lightmapLump->filelen) memcpy(pixels, fileBase + lightmapLump->fileofs, lightmapLump->filelen);
    memset(nodes, 0, s_worldData.numnodes * sizeof(*nodes));
    memset(faces, 0, s_worldData.numsurfaces * sizeof(*faces));
    for (i = 0; i < s_worldData.numsurfaces; ++i) {
        const dsurface_t *surface = input + i;
        q3IHVModelLightFace_t *face = faces + i;
        const int lightmap = LittleLong(surface->lightmapNum);
        const int first = LittleLong(surface->firstVert), count = LittleLong(surface->numVerts);
        const int x = LittleLong(surface->lightmapX), y = LittleLong(surface->lightmapY);
        float point[3];
        if (LittleLong(surface->surfaceType) != MST_PLANAR || lightmap < 0 || !pageCount
            || !s_worldData.surfaces[i].shader || !s_worldData.surfaces[i].shader->ihvModelLightmap) continue;
        face->width = LittleLong(surface->lightmapWidth); face->height = LittleLong(surface->lightmapHeight);
        if (first < 0 || first >= vertexCount || count < 3 || count > vertexCount - first
            || lightmap >= pageCount || x < 0 || x > 128 || y < 0 || y > 128
            || face->width < 0 || face->width > 128 - x || face->height < 0 || face->height > 128 - y) {
            ri.Error(ERR_DROP, "IHV model lighting: invalid face extent"); return;
        }
        if (!face->width || !face->height) continue;
        for (a = 0; a < 3; ++a) {
            point[a] = LittleFloat(vertices[first].xyz[a]);
            face->plane[a] = LittleFloat(vertices[first].normal[a]);
            face->origin[a] = LittleFloat(surface->lightmapOrigin[a]);
            if (!Q3IHV_EntityFinite(point + a) || !Q3IHV_EntityFinite(face->plane + a)
                || !Q3IHV_EntityFinite(face->origin + a)) {
                ri.Error(ERR_DROP, "IHV model lighting: nonfinite face"); return;
            }
        }
        face->plane[3] = (float)Q3IHV_LightDot(point, face->plane);
        if (!Q3IHV_EntityFinite(face->plane + 3)) {
            ri.Error(ERR_DROP, "IHV model lighting: excessive plane distance"); return;
        }
        for (a = 0; a < 2; ++a) {
            float vector[3]; int j;
            for (j = 0; j < 3; ++j) vector[j] = LittleFloat(surface->lightmapVecs[a][j]);
            if (!Q3IHV_LightReciprocal(vector, face->reciprocal[a])) {
                ri.Error(ERR_DROP, "IHV model lighting: invalid texture vector"); return;
            }
        }
        face->pixels = pixels + ((lightmap * 128 + y) * 128 + x) * 3;
    }
    for (i = 0; i < s_worldData.nummarksurfaces; ++i)
        marks[i] = faces + (s_worldData.marksurfaces[i] - s_worldData.surfaces);
    for (i = 0; i < s_worldData.numnodes; ++i) {
        const mnode_t *inputNode = s_worldData.nodes + i;
        q3IHVModelLightNode_t *node = nodes + i;
        if (inputNode->contents == -1) {
            memcpy(node->plane, inputNode->plane->normal, 3 * sizeof(float));
            node->plane[3] = inputNode->plane->dist; node->numFaces = -1;
            for (a = 0; a < 2; ++a) node->children[a] = nodes + (inputNode->children[a] - s_worldData.nodes);
        } else {
            node->numFaces = inputNode->nummarksurfaces;
            node->faces = node->numFaces ? marks + (inputNode->firstmarksurface - s_worldData.marksurfaces) : NULL;
        }
    }
    s_worldData.q3IHVModelLight = nodes;
}
#endif
