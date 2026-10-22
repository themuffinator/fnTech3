/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_CSM_AMBIENT_H
#define FNTECH3_TR_CSM_AMBIENT_H

/* Read authored ambient in compiler light units. Q3's LightWorld uses
 * worldspawn ambient * _color; q3map2 also accepts _ambient and defaults a
 * missing colour to white. Ritual and MOH use RGB ambientlight. This is
 * source observation, not incorporated compiler code. No per-map tuning. */
static void R_CSMReadAmbient( const char *entities, int gameId, vec3_t out ) {
	vec3_t color = { 0, 0, 0 }, rgb = { 0, 0, 0 };
	float ambient = 0.0f, alias = 0.0f;
	char key[1024], value[1024];
	const char *cursor, *end;
	qboolean q3map2;
	int i;

	VectorClear( out );
	if ( !entities ) return;
	if ( COM_GAME_ID_IS_RITUAL( gameId ) ) {
		ritualModelLights_t keys;
		Com_Memset( &keys, 0, sizeof( keys ) );
		RitualModelLight_Keys( entities, (int)strlen( entities ), &keys );
		VectorCopy( keys.ambient, rgb );
	} else {
		cursor = entities;
		end = entities + strlen( entities );
		if ( !RitualModelLight_Token( &cursor, end, key, sizeof( key ) ) || strcmp( key, "{" ) ) return;
		while ( RitualModelLight_Token( &cursor, end, key, sizeof( key ) ) && strcmp( key, "}" ) ) {
			if ( !RitualModelLight_Token( &cursor, end, value, sizeof( value ) ) || !strcmp( value, "}" ) ) break;
			if ( !strcmp( key, "ambient" ) ) ambient = (float)atof( value );
			else if ( !strcmp( key, "_ambient" ) ) alias = (float)atof( value );
			else if ( !strcmp( key, "_color" ) ) RitualModelLight_Floats( value, color );
			else if ( !strcmp( key, "ambientlight" ) ) RitualModelLight_Floats( value, rgb );
		}
		if ( gameId != COM_GAME_MOHAA ) {
			q3map2 = R_SkySunEntitiesQ3map2( entities, gameId );
			if ( q3map2 && VectorLengthSquared( color ) == 0.0f ) VectorSet( color, 1, 1, 1 );
			if ( q3map2 && alias != 0.0f ) ambient = alias;
			VectorScale( color, ambient, rgb );
		}
	}
	for ( i = 0; i < 3; ++i ) {
		out[i] = RitualModelLight_Finite( &rgb[i] ) ? Com_Clamp( 0.0f, 65535.0f, rgb[i] ) : 0.0f;
	}
}

#endif
