/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_STAGELESS_H
#define FNTECH3_TR_Q3_IHV_STAGELESS_H

/* A shader script with no stage that is neither sky nor fog. Q3 1.32 ignores
 * the script and gives the name the default shader. The original 4.01 IHV
 * renderer's ParseShader (0x43a3c0) returns no shader for it either
 * (0x43ad34..0x43ad54), but its R_FindShader (0x439fd0, through 0x43a350)
 * then treats the name as unscripted: it loads <name>.tga, .bmp, .wal or .pcx
 * (0x43a1a9..0x43a297) and builds the implicit shader on what the script set
 * (0x43a2d8..0x43a337). Only a missing image gives the default shader
 * (0x43a299). The build's textures/sfx/blackness is such a script, drawn
 * black from textures/sfx/blackness.tga. Reset for every shader looked up;
 * included by a renderer's shader parser after its parse globals. */
static qboolean s_q3ihvStagelessImage;

/* Called where a stageless script is ignored; true when this profile looks
 * the name up as an image instead. */
static qboolean R_Q3IHVStagelessImage( void ) {
	s_q3ihvStagelessImage = ri.gameId == COM_GAME_QUAKE3_IHV ? qtrue : qfalse;
	return s_q3ihvStagelessImage;
}

#endif
