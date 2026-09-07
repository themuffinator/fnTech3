/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#ifndef FNTECH3_QCOMMON_CM_GAME_CONTRACT_H
#define FNTECH3_QCOMMON_CM_GAME_CONTRACT_H

#include "q_shared.h"
#include "game_profile.h"

#define CM_QL_CYLINDER_SCALE_DEFAULT 1.1f
#define CM_QL_CYLINDER_SCALE_MAX     16.0f

/*
 * FnQuake3 uses the inherited id Tech 3 capsule cylinder radius verbatim.
 * FnQL exposes sv_cylinderScale for Quake Live and defaults it to 1.1.  Keep
 * that choice at the immutable game-profile boundary rather than allowing the
 * cvar itself to select which game's collision rules are active.
 */
static ID_INLINE float CM_VerticalCylinderRadiusForGame(
	comGameId_t gameId,
	float radius,
	float quakeLiveScale )
{
	if ( gameId != COM_GAME_QUAKELIVE ) {
		return radius;
	}

	if ( !( quakeLiveScale >= 0.0f
		&& quakeLiveScale <= CM_QL_CYLINDER_SCALE_MAX ) ) {
		quakeLiveScale = CM_QL_CYLINDER_SCALE_DEFAULT;
	}

	return radius * quakeLiveScale;
}

static ID_INLINE qboolean CM_EntityTokenEqualsAsciiNoCase(
	const char *token,
	int tokenLength,
	const char *literal )
{
	int i;

	for ( i = 0; i < tokenLength; ++i ) {
		char tokenCharacter = token[i];
		const char literalCharacter = literal[i];

		if ( !literalCharacter ) {
			return qfalse;
		}
		if ( tokenCharacter >= 'A' && tokenCharacter <= 'Z' ) {
			tokenCharacter = (char)( tokenCharacter + ( 'a' - 'A' ) );
		}
		if ( tokenCharacter != literalCharacter ) {
			return qfalse;
		}
	}

	return literal[tokenLength] == '\0' ? qtrue : qfalse;
}

/*
 * FnQuake3 deliberately presents QL BSP advertisement entities to Q3 game
 * modules as func_static.  FnQL must preserve the original classname for the
 * retail QL module.  The caller supplies sourceLength + 1 bytes of output;
 * the Q3 replacement is shorter than the token it replaces.
 */
static ID_INLINE int CM_CopyEntityStringForGame(
	comGameId_t gameId,
	const char *source,
	int sourceLength,
	char *destination )
{
	static const char keyName[] = "classname";
	static const char advertisement[] = "advertisement";
	static const char replacement[] = "func_static";
	const char *sourceEnd;
	const char *currentKey;
	int outputLength;
	int currentKeyLength;
	qboolean awaitingValue;

	if ( gameId != COM_GAME_QUAKE3 ) {
		Com_Memcpy( destination, source, sourceLength );
		destination[sourceLength] = '\0';
		return sourceLength;
	}

	sourceEnd = source + sourceLength;
	currentKey = NULL;
	outputLength = 0;
	currentKeyLength = 0;
	awaitingValue = qfalse;

	while ( source < sourceEnd && *source ) {
		const char character = *source++;

		if ( character == '{' || character == '}' ) {
			awaitingValue = qfalse;
			currentKey = NULL;
			currentKeyLength = 0;
			destination[outputLength++] = character;
			continue;
		}

		if ( character != '"' ) {
			destination[outputLength++] = character;
			continue;
		}

		destination[outputLength++] = character;

		{
			const char *tokenStart = source;
			int tokenLength;

			while ( source < sourceEnd && *source && *source != '"' ) {
				++source;
			}
			tokenLength = (int)( source - tokenStart );

			if ( awaitingValue
				&& CM_EntityTokenEqualsAsciiNoCase(
					currentKey, currentKeyLength, keyName )
				&& CM_EntityTokenEqualsAsciiNoCase(
					tokenStart, tokenLength, advertisement ) ) {
				Com_Memcpy( destination + outputLength,
					replacement, sizeof( replacement ) - 1 );
				outputLength += (int)sizeof( replacement ) - 1;
			} else {
				Com_Memcpy( destination + outputLength,
					tokenStart, tokenLength );
				outputLength += tokenLength;
			}

			if ( source < sourceEnd && *source == '"' ) {
				destination[outputLength++] = *source++;
			}

			if ( awaitingValue ) {
				awaitingValue = qfalse;
				currentKey = NULL;
				currentKeyLength = 0;
			} else {
				currentKey = tokenStart;
				currentKeyLength = tokenLength;
				awaitingValue = qtrue;
			}
		}
	}

	destination[outputLength] = '\0';
	return outputLength;
}

#endif
