/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_QCOMMON_ALICE_RUNTIME_H
#define FNTECH3_QCOMMON_ALICE_RUNTIME_H

/* Engine services American McGee's Alice's two modules share.
 *
 * Retail alice.exe hands the game and the client game the same engine
 * function for several imports -- the lip-sync pair, the global alias block,
 * the model-definition queries -- so both modules see one cache and one set
 * of answers. These are that engine side, independent of which module asks.
 *
 * See docs/fntech3/2026-09-23-alice-cgame.md. */

#include "q_shared.h"

/* ---- lip sync ------------------------------------------------------------ */

/* A `.lip` file is text: the spoken length in seconds, a count, and that many
 * mouth amplitudes, one a line. Retail keeps the most recent files in a ring
 * of fixed records whose amplitude array is 512 bytes long (0x0043eee0). */
enum { ALICE_LIP_MAX_AMPLITUDES = 512 };

/* The length of the speech whose sound file is `soundName`, from the `.lip`
 * beside it; -1 when there is none, as retail answers (0x0043f165). */
float Alice_LipLength( const char *soundName );

/* The amplitudes, with their count, or null when there is no lip file. The
 * array stays valid until the ring reuses its record. */
const byte *Alice_LipAmplitudes( const char *soundName, int *count );

void Alice_LipClear( void );

/* ---- the shared block ---------------------------------------------------- */

/* One 60-byte block the engine hands the game, the client game and the
 * renderer alike, from one getter (0x0042c490); the game import build clears
 * its fifteen dwords before the module starts. The client game writes the camera
 * into it and the game its HUD state, which is why there is only one. */
void *Alice_SharedBlock( void );
void Alice_SharedBlockClear( void );

/* ---- noise --------------------------------------------------------------- */

/* The renderer's four-dimensional noise (0x004923c0), over the table retail
 * builds once from `srand( 1001 )` with the Microsoft C runtime's generator
 * (0x00492350). Pure: the same arguments always give the same value. */
float Alice_Noise( float x, float y, float z, float t );

/* ---- debug lines --------------------------------------------------------- */

/* The game module owns the debug line array; the engine owns the two
 * variables its game imports 101 and 102 point at, which the module sets to
 * that array and its count. The client game's R_DebugLine appends through
 * them (0x004766f0). */
void **Alice_DebugLineArray( void );
int *Alice_DebugLineCount( void );
qboolean Alice_AddDebugLine( const vec3_t start, const vec3_t end, float r, float g, float b, float alpha );

#endif
