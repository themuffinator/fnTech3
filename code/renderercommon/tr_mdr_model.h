/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MDR_MODEL_H
#define FNTECH3_TR_MDR_MODEL_H

/* Included by each renderer's tr_model.c. Validate the complete immutable
 * file before allocating renderer storage or registering any shaders. */
#include "tr_mdr_format.h"

static void R_MDRCopyName( char *destination, const unsigned char *source, size_t bytes ) {
	memcpy( destination, source, bytes - 1 );
	destination[bytes - 1] = '\0';
}

static qboolean R_LoadMDR( model_t *mod, void *buffer, int filesize, const char *mod_name ) {
	fnMdrView_t view;
	mdrHeader_t *mdr;
	mdrFrame_t *frame;
	mdrLOD_t *lod;
	mdrTag_t *tag;
	uint32_t i, lodOffset;
	if ( filesize < 0 || !FnMdr_Open( buffer, (size_t)filesize,
		SHADER_MAX_VERTEXES, SHADER_MAX_INDEXES / 3, &view ) ||
		mod->dataSize < 0 || view.expandedBytes > (uint32_t)( INT_MAX - mod->dataSize ) ) {
		ri.Printf( PRINT_WARNING, "R_LoadMDR: %s has an invalid RDM5 model structure\n", mod_name );
		return qfalse;
	}
	mdr = (mdrHeader_t *)ri.Hunk_Alloc( (int)view.expandedBytes, h_low );
	memset( mdr, 0, view.expandedBytes );
	mdr->ident = MDR_IDENT; mdr->version = MDR_VERSION;
	R_MDRCopyName( mdr->name, view.data + 8, sizeof( mdr->name ) );
	mdr->numFrames = (int)view.numFrames; mdr->numBones = (int)view.numBones;
	mdr->numLODs = (int)view.numLODs; mdr->numTags = (int)view.numTags;
	mdr->ofsFrames = sizeof( *mdr );
	frame = (mdrFrame_t *)( mdr + 1 );
	for ( i = 0; i < view.numFrames; ++i ) {
		const unsigned char *source = view.data + view.frames + (size_t)i * view.frameStride;
		uint32_t bone;
		int axis;
		for ( axis = 0; axis < 3; ++axis ) {
			frame->bounds[0][axis] = FnMdr_Float( source + axis * 4 );
			frame->bounds[1][axis] = FnMdr_Float( source + 12 + axis * 4 );
			frame->localOrigin[axis] = FnMdr_Float( source + 24 + axis * 4 );
		}
		frame->radius = FnMdr_Float( source + 36 );
		if ( !view.compressed ) R_MDRCopyName( frame->name, source + 40, sizeof( frame->name ) );
		for ( bone = 0; bone < view.numBones; ++bone ) FnMdr_Bone( &view, i, bone, frame->bones[bone].matrix );
		frame = (mdrFrame_t *)&frame->bones[view.numBones];
	}
	lod = (mdrLOD_t *)frame;
	mdr->ofsLODs = (int)( (byte *)lod - (byte *)mdr );
	lodOffset = view.lods;
	for ( i = 0; i < view.numLODs; ++i ) {
		fnMdrLodView_t sourceLod = {0};
		mdrSurface_t *surface = (mdrSurface_t *)( lod + 1 );
		uint32_t s, surfaceOffset;
		FnMdr_Lod( &view, lodOffset, &sourceLod );
		lod->numSurfaces = (int)sourceLod.numSurfaces;
		lod->ofsSurfaces = sizeof( *lod );
		surfaceOffset = sourceLod.surfaces;
		for ( s = 0; s < sourceLod.numSurfaces; ++s ) {
			fnMdrSurfaceView_t sourceSurface = {0};
			mdrVertex_t *vertex = (mdrVertex_t *)( surface + 1 );
			mdrTriangle_t *triangle;
			shader_t *shader;
			uint32_t v, vertexOffset;
			FnMdr_Surface( &sourceLod, surfaceOffset, &sourceSurface );
			surface->ident = SF_MDR;
			R_MDRCopyName( surface->name, sourceSurface.data + 4, sizeof( surface->name ) );
			R_MDRCopyName( surface->shader, sourceSurface.data + 68, sizeof( surface->shader ) );
			Q_strlwr( surface->name );
			surface->ofsHeader = (int)( (byte *)mdr - (byte *)surface );
			surface->numVerts = (int)sourceSurface.numVerts;
			surface->numTriangles = (int)sourceSurface.numTriangles;
			surface->ofsVerts = sizeof( *surface );
			shader = R_FindShader( surface->shader, LIGHTMAP_NONE, qtrue );
			surface->shaderIndex = shader->defaultShader ? 0 : shader->index;
			vertexOffset = sourceSurface.vertices;
			for ( v = 0; v < sourceSurface.numVerts; ++v ) {
				const unsigned char *source = sourceSurface.data + vertexOffset;
				uint32_t weights = 0, next = 0, w;
				int axis;
				const int validNormal = FnMdr_Finite( source, 3 );
				FnMdr_Vertex( &sourceSurface, vertexOffset, &weights, &next );
				for ( axis = 0; axis < 3; ++axis ) vertex->normal[axis] = validNormal ? FnMdr_Float( source + axis * 4 ) : 0.0f;
				for ( axis = 0; axis < 2; ++axis ) vertex->texCoords[axis] = FnMdr_Float( source + 12 + axis * 4 );
				vertex->numWeights = (int)weights;
				for ( w = 0; w < weights; ++w ) {
					const unsigned char *weight = source + FN_MDR_VERTEX_BYTES + (size_t)w * FN_MDR_WEIGHT_BYTES;
					vertex->weights[w].boneIndex = (int)FnMdr_U32( weight );
					vertex->weights[w].boneWeight = FnMdr_Float( weight + 4 );
					for ( axis = 0; axis < 3; ++axis ) vertex->weights[w].offset[axis] = FnMdr_Float( weight + 8 + axis * 4 );
				}
				vertex = (mdrVertex_t *)&vertex->weights[weights];
				vertexOffset = next;
			}
			triangle = (mdrTriangle_t *)vertex;
			surface->ofsTriangles = (int)( (byte *)triangle - (byte *)surface );
			for ( v = 0; v < sourceSurface.numTriangles; ++v ) {
				int index;
				for ( index = 0; index < 3; ++index )
					triangle[v].indexes[index] = (int)FnMdr_U32( sourceSurface.data + sourceSurface.triangles + (size_t)v * 12 + index * 4 );
			}
			surface->ofsEnd = (int)( (byte *)( triangle + sourceSurface.numTriangles ) - (byte *)surface );
			surface = (mdrSurface_t *)( triangle + sourceSurface.numTriangles );
			surfaceOffset += sourceSurface.bytes;
		}
		lod->ofsEnd = (int)( (byte *)surface - (byte *)lod );
		lod = (mdrLOD_t *)surface;
		lodOffset += sourceLod.bytes;
	}
	tag = (mdrTag_t *)lod;
	mdr->ofsTags = (int)( (byte *)tag - (byte *)mdr );
	for ( i = 0; i < view.numTags; ++i ) {
		const unsigned char *source = view.data + view.tags + (size_t)i * FN_MDR_TAG_BYTES;
		tag[i].boneIndex = (int)FnMdr_U32( source );
		R_MDRCopyName( tag[i].name, source + 4, sizeof( tag[i].name ) );
	}
	mdr->ofsEnd = (int)( (byte *)( tag + view.numTags ) - (byte *)mdr );
	mod->type = MOD_MDR; mod->modelData = mdr;
	mod->dataSize += (int)view.expandedBytes;
	mod->numLods = mdr->numLODs;
	/* Both retail modes exclude the final RDM5 LOD (SP 0x451bce,
	 * Holomatch 0x464a8d). Other
	 * profiles retain all exported levels, as their existing loader did. */
	if ( ( ri.gameId == COM_GAME_STVEF_SP || ri.gameId == COM_GAME_STVEF_MP ) && mod->numLods > 1 ) --mod->numLods;
	return qtrue;
}
#endif
