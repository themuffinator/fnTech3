/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_COMPRESSION_H
#define FNTECH3_STVEF_COMPRESSION_H

/* Included after the renderer's OpenGL extension query helper. */
static void R_InitTextureCompression( void ) {
	if ( ri.gameId == COM_GAME_STVEF_SP || ri.gameId == COM_GAME_STVEF_MP ) {
		const qboolean arb = R_HaveExtension( "GL_ARB_texture_compression" ) &&
			R_HaveExtension( "GL_EXT_texture_compression_s3tc" );
		const qboolean s3 = R_HaveExtension( "GL_S3_s3tc" );

		/* Native SP 1.20 at 0x456cc0 and HM at 0x46a140 test the
		 * enable cvar's float value. Preference 1 selects legacy S3
		 * when present; other values prefer ARB, with either fallback.
		 */
		glConfig.textureCompression = TC_NONE;
		if ( r_ext_compressed_textures->value == 0.0f ) {
			ri.Printf( PRINT_ALL, "...texture compression disabled\n" );
		} else if ( s3 && ( r_ext_preferred_tc_method->integer == TC_S3TC || !arb ) ) {
			glConfig.textureCompression = TC_S3TC;
			ri.Printf( PRINT_ALL, "...using GL_S3_s3tc\n" );
		} else if ( arb ) {
			glConfig.textureCompression = TC_S3TC_ARB;
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_compression_s3tc\n" );
		} else {
			ri.Printf( PRINT_ALL, "...no supported texture compression method\n" );
		}
		return;
	}

	/* Preserve the other profiles' integer enable and ARB-first policy. */
	// GL_EXT_texture_compression_s3tc
	if ( R_HaveExtension( "GL_ARB_texture_compression" ) &&
		 R_HaveExtension( "GL_EXT_texture_compression_s3tc" ) )
	{
		if ( r_ext_compressed_textures->integer ){
			glConfig.textureCompression = TC_S3TC_ARB;
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_compression_s3tc\n" );
		} else {
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_compression_s3tc\n" );
		}
	} else {
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_compression_s3tc not found\n" );
	}

	// GL_S3_s3tc
	if ( glConfig.textureCompression == TC_NONE && r_ext_compressed_textures->integer ) {
		if ( R_HaveExtension( "GL_S3_s3tc" ) ) {
			if ( r_ext_compressed_textures->integer ) {
				glConfig.textureCompression = TC_S3TC;
				ri.Printf( PRINT_ALL, "...using GL_S3_s3tc\n" );
			} else {
				glConfig.textureCompression = TC_NONE;
				ri.Printf( PRINT_ALL, "...ignoring GL_S3_s3tc\n" );
			}
		} else {
			ri.Printf( PRINT_ALL, "...GL_S3_s3tc not found\n" );
		}
	}
}

#endif
