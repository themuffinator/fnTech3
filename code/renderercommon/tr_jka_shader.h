/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_SHADER_H
#define FNTECH3_JKA_SHADER_H
/* Retail jasp0x47b069..7b sets the material's noTC byte at0xcd3013.
 * Its map/clampmap/animmap/sky image requests invert that byte into the
 * compression-permitted argument (0x4798af,0x47993b,0x47a4a0,0x47aa8c).
 * Adapt that data into fnTech3's existing image flags; mip selection,
 * lighting and the renderer's feature pipeline retain their own policies. */
static qboolean R_JKAShaderProfile( void ) {
	return ri.gameId == COM_GAME_JKA_SP || ri.gameId == COM_GAME_JKA_MP ? qtrue : qfalse;
}
/* Jedi Outcast's engines read noTC the same way: jk2mp.exe 0x48c80b and
 * jk2sp.exe 0x47c7a9 set the shader's word (0x1a88e5c, 0x131a02c), and
 * jk2mp.exe's clampmap and animMap requests pass its inverse (0x48cf33,
 * 0x48d9ff). */
static qboolean R_JediShaderProfile( void ) {
	return R_JKAShaderProfile() || ri.gameId == COM_GAME_JK2_SP || ri.gameId == COM_GAME_JK2_MP ? qtrue : qfalse;
}
static qboolean R_JKAShaderNoTCDirective( const char *token, qboolean *noTC ) {
	if ( R_JediShaderProfile() && !Q_stricmp( token, "noTC" ) ) {
		*noTC = qtrue;
		return qtrue;
	}
	return qfalse;
}
static imgFlags_t R_JKAShaderImageFlags( imgFlags_t flags, qboolean noTC ) {
	return R_JediShaderProfile() && noTC ? (imgFlags_t)( flags | IMGFLAG_NO_COMPRESSION ) : flags;
}
/* Raven's general `sun` is q3map_sun under a second name in all four Jedi
 * engines: jamp.exe 1.01 (0x4b4dc4), jasp.exe 1.01 (0x47af0c), jk2mp.exe
 * (0x48c63f) and jk2sp.exe (0x47c667) send both words to one branch
 * (0x4b51a1, 0x47b312, 0x48ccbc, 0x47cab4). It reads exactly six words with
 * COM_ParseExt( text, qfalse ) and atof, a missing one as 0 -- colour,
 * intensity, degrees, elevation -- into tr.sunLight and tr.sunDirection, and
 * leaves the rest of the line to the keyword loop. The renderers take the
 * six values into their own sky sun. Elite Force and the id engines know
 * only q3map_sun (record 2026-10-15-raven-sun-keyword). */
static inline qboolean R_JediSunKeyword( const char *token ) {
	return R_JediShaderProfile() && ( !Q_stricmp( token, "sun" ) || !Q_stricmp( token, "q3map_sun" ) ) ? qtrue : qfalse;
}
static inline void R_JediSunParms( const char **text, float parms[6] ) {
	int i;
	for ( i = 0; i < 6; i++ ) {
		parms[i] = Q_atof( COM_ParseExt( text, qfalse ) );
	}
	/* a read that ran off the text leaves it null; retail's next
	 * COM_ParseExt reads that as "", and the shader fails for want of its
	 * closing brace, which an empty text gives COM_ParseComplex too */
	if ( !*text ) {
		*text = "";
	}
}
/* Raven's lightColor ( r g b ): jk2mp.exe reads it with ParseVector
 * (0x48c697, 0x48c0e0), so a missing bracket or element fails the shader,
 * and FinishShader keeps a sky's as tr.sunAmbient (0x48f629); jamp.exe,
 * jasp.exe and jk2sp.exe skip the line (0x4b4e28, 0x47afcc, 0x47c701).
 * Nothing in the renderers uses the colour. */
static inline qboolean R_JediLightColorKeyword( const char *token ) {
	return R_JediShaderProfile() && !Q_stricmp( token, "lightColor" ) ? qtrue : qfalse;
}
/* All four Jedi engines read animMap, clampAnimMap and oneShotAnimMap in
 * one ParseStage branch (jamp.exe 0x4b426c, jasp.exe 0x47a42e, jk2mp.exe
 * 0x48d97b, jk2sp.exe 0x47d7ab). clampAnimMap loads its frames clamped
 * (GL_CLAMP: jk2mp.exe 0x48d9f2, jk2sp.exe 0x47d822), oneShotAnimMap sets
 * the bundle's one-shot byte (Academy's bundle +0x19; Outcast's +0x51 in
 * jk2mp.exe 0x48d9c7, +0x4d in jk2sp.exe 0x47d7f7), and
 * R_BindAnimatedImage (jamp.exe 0x4ac3d0, jasp.exe 0x472fd0, jk2mp.exe
 * 0x488300, jk2sp.exe 0x478180) then holds a one-shot bundle on its last
 * frame instead of wrapping. Academy's three keep up to 32 frames
 * (0x4b42e4, 0x47a497); Outcast's keep Quake III's 8 and skip the rest
 * without a warning (jk2mp.exe 0x48d9ed, jk2sp.exe 0x47d81d). Elite Force
 * and the id engines know only animMap. */
#define MAX_IMAGE_ANIMATIONS_JKA 32
static inline qboolean R_JediClampAnimMapKeyword( const char *token ) {
	return R_JediShaderProfile() && !Q_stricmp( token, "clampAnimMap" ) ? qtrue : qfalse;
}
static inline qboolean R_JediOneShotAnimMapKeyword( const char *token ) {
	return R_JediShaderProfile() && !Q_stricmp( token, "oneShotAnimMap" ) ? qtrue : qfalse;
}
/* Raven's fourth alphaFunc, GE192: all four Jedi engines' NameToAFunc
 * return state bit 0x80000000 for it (jamp.exe 1.01 0x4b24da, jasp.exe 1.01
 * 0x4785f0, jk2mp.exe 1.02a 0x48dc72, jk2sp.exe 0x47daa2) beside Quake
 * III's three, and GL_State turns that bit into
 * glAlphaFunc( GL_GEQUAL, 0.75f ) (0x48a4c2, 0x454bf2, 0x46d3d8, 0x45c3f8).
 * fnTech3 carries it in the comparison-and-reference alpha test of Alice's
 * `alphatest`: greaterequal (5), reference 192, applied as 192 / 256 = 0.75.
 * Elite Force and the id engines know only the three (record
 * 2026-10-16-jedi-alphafunc-ge192). */
#define GLS_JEDI_ATEST_GE_C0 ( ( 5u << GLS_ALICE_ATEST_FUNC_SHIFT ) | ( 192u << GLS_ALICE_ATEST_REF_SHIFT ) )
static inline qboolean R_JediAlphaFuncGE192( const char *funcname ) {
	return R_JediShaderProfile() && !Q_stricmp( funcname, "GE192" ) ? qtrue : qfalse;
}
#endif
