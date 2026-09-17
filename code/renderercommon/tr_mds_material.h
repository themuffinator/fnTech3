/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MDS_MATERIAL_H
#define FNTECH3_TR_MDS_MATERIAL_H

#include "tr_mesh_material.h"

static shader_t *R_MdsSurfaceShader( const refEntity_t *entity, const mdsMeshSurface_t *surface ) {
	if ( entity->customShader ) return R_GetShaderByHandle( entity->customShader );
	if ( entity->customSkin > 0 && entity->customSkin < tr.numSkins )
		return R_MeshSkinShader( entity, R_GetSkinByHandle( entity->customSkin ), surface->name );
	return R_GetShaderByHandle( surface->shader );
}

#endif
