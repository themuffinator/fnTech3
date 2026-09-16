/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Independently authored RTCW brush-surface material lookup. */
#ifndef FNTECH3_TR_MODEL_SHADER_H
#define FNTECH3_TR_MODEL_SHADER_H

qhandle_t RE_GetShaderFromModel( qhandle_t handle, int surface, int withLightmap ) {
	const model_t *model = R_GetModelByHandle( handle );
	const bmodel_t *brush;
	shader_t *material;
	/* All four RTCW source implementations ignore this argument. */
	(void)withLightmap;
	if ( !model || model->type != MOD_BRUSH || !( brush = model->bmodel ) ||
		 !brush->firstSurface || brush->numSurfaces <= 0 ) return 0;
	if ( surface < 0 || surface >= brush->numSurfaces ) surface = 0;
	material = brush->firstSurface[surface].shader;
	if ( !material ) return 0;
	if ( material->lightmapIndex > LIGHTMAP_NONE ) {
		const image_t *image;
		qboolean mip = qtrue;
		for ( image = hashTable[generateHashValue( material->name )]; image; image = image->next ) {
			if ( !strcmp( material->name, image->imgName ) ) {
				mip = ( image->flags & IMGFLAG_MIPMAP ) ? qtrue : qfalse;
				break;
			}
		}
		material = R_FindModelShader( material->name, mip );
	}
	return material ? material->index : 0;
}

#endif
