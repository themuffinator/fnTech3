/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) the OpenMoHAA team
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3. It is adapted from OpenMoHAA's
code/qcommon/msg.cpp (https://github.com/openmoh/openmohaa), which carries
Quake III Arena's message code with OpenMoHAA's protocol-6 tables and
packings and is distributed under the GNU General Public License, version 2
or (at your option) any later version. It remains under that licence here.

fnTech3 is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

fnTech3 is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE. See the GNU General Public License for more details.
===========================================================================
*/

/* Medal of Honor: Allied Assault's protocol-6 message records; see
 * code/compat/mohaa_message_codec.hpp.
 *
 * fnTech3's changes from the source: the tables name the members of the
 * 1.00 module records rather than OpenMoHAA's merged ones, so the
 * networked flags word is `pm_flags`; an eight-bit packed angle is read
 * back scaled by its value (the source returns the scale alone); and the
 * later protocols' paths are gone. */

#include "../compat/mohaa_message_codec.hpp"
#include "q_shared.h"
#include "qcommon.h"

#include <cmath>
#include <cstddef>
#include <cstring>

extern "C" void EulerToQuat( float ang[3], float q[4] );

namespace fntech3::mohaa::wire {

namespace {

enum class FieldType {
	regular,
	angle,
	animTime,
	animWeight,
	scale,
	alpha,
	coord,
	velocity
};

struct NetField {
	std::size_t offset;
	int size;
	int bits;
	FieldType type;
};

constexpr int FLOAT_INT_BITS = 13;
constexpr int FLOAT_INT_BIAS = 1 << ( FLOAT_INT_BITS - 1 );

#define ESF( member, bits, type ) \
	{ offsetof( EntityState, member ), (int)sizeof( EntityState::member ), bits, FieldType::type }
#define ESFA( member, index, bits, type ) \
	{ offsetof( EntityState, member ) + sizeof( EntityState::member[0] ) * ( index ), \
		(int)sizeof( EntityState::member[0] ), bits, FieldType::type }
#define ESFF( index, part, bits, type ) \
	{ offsetof( EntityState, frameInfo ) + sizeof( FrameInfo ) * ( index ) + offsetof( FrameInfo, part ), \
		(int)sizeof( FrameInfo::part ), bits, FieldType::type }
#define ESFB( index, axis ) \
	{ offsetof( EntityState, bone_angles ) + sizeof( float ) * ( ( index ) * 3 + ( axis ) ), \
		(int)sizeof( float ), -13, FieldType::angle }

/* entityStateFields_ver_6, in its order. */
const NetField kEntityFields[] = {
	ESFA( netorigin, 0, 0, coord ),
	ESFA( netorigin, 1, 0, coord ),
	ESFA( netangles, 1, 12, angle ),
	ESFF( 0, time, 0, animTime ),
	ESFF( 1, time, 0, animTime ),
	ESFB( 0, 0 ),
	ESFB( 3, 0 ),
	ESFB( 1, 0 ),
	ESFB( 2, 0 ),
	ESFA( netorigin, 2, 0, coord ),
	ESFF( 0, weight, 0, animWeight ),
	ESFF( 1, weight, 0, animWeight ),
	ESFF( 2, time, 0, animTime ),
	ESFF( 3, time, 0, animTime ),
	ESFF( 0, index, 12, regular ),
	ESFF( 1, index, 12, regular ),
	ESF( actionWeight, 0, animWeight ),
	ESFF( 2, weight, 0, animWeight ),
	ESFF( 3, weight, 0, animWeight ),
	ESFF( 2, index, 12, regular ),
	ESFF( 3, index, 12, regular ),
	ESF( eType, 8, regular ),
	ESF( modelindex, 16, regular ),
	ESF( parent, 16, regular ),
	ESF( constantLight, 32, regular ),
	ESF( renderfx, 32, regular ),
	ESFA( bone_tag, 0, -8, regular ),
	ESFA( bone_tag, 1, -8, regular ),
	ESFA( bone_tag, 2, -8, regular ),
	ESFA( bone_tag, 3, -8, regular ),
	ESFA( bone_tag, 4, -8, regular ),
	ESF( scale, 0, scale ),
	ESF( alpha, 0, alpha ),
	ESF( usageIndex, 16, regular ),
	ESF( eFlags, 16, regular ),
	ESF( solid, 32, regular ),
	ESFA( netangles, 2, 12, angle ),
	ESFA( netangles, 0, 12, angle ),
	ESF( tag_num, 10, regular ),
	ESFB( 1, 2 ),
	ESF( attach_use_angles, 1, regular ),
	ESFA( origin2, 1, 0, coord ),
	ESFA( origin2, 0, 0, coord ),
	ESFA( origin2, 2, 0, coord ),
	ESFB( 0, 2 ),
	ESFB( 2, 2 ),
	ESFB( 3, 2 ),
	ESFA( surfaces, 0, 8, regular ),
	ESFA( surfaces, 1, 8, regular ),
	ESFA( surfaces, 2, 8, regular ),
	ESFA( surfaces, 3, 8, regular ),
	ESFB( 0, 1 ),
	ESFA( surfaces, 4, 8, regular ),
	ESFA( surfaces, 5, 8, regular ),
	ESF( loopSound, 16, regular ),
	ESF( loopSoundVolume, 0, regular ),
	ESF( loopSoundMinDist, 0, regular ),
	ESF( loopSoundMaxDist, 0, regular ),
	ESF( loopSoundPitch, 0, regular ),
	ESF( loopSoundFlags, 8, regular ),
	ESFA( attach_offset, 0, 0, regular ),
	ESFA( attach_offset, 1, 0, regular ),
	ESFA( attach_offset, 2, 0, regular ),
	ESF( beam_entnum, 16, regular ),
	ESF( skinNum, 16, regular ),
	ESF( wasframe, 10, regular ),
	ESFF( 4, index, 12, regular ),
	ESFF( 5, index, 12, regular ),
	ESFF( 6, index, 12, regular ),
	ESFF( 7, index, 12, regular ),
	ESFF( 8, index, 12, regular ),
	ESFF( 9, index, 12, regular ),
	ESFF( 10, index, 12, regular ),
	ESFF( 11, index, 12, regular ),
	ESFF( 12, index, 12, regular ),
	ESFF( 13, index, 12, regular ),
	ESFF( 14, index, 12, regular ),
	ESFF( 15, index, 12, regular ),
	ESFF( 4, time, 0, animTime ),
	ESFF( 5, time, 0, animTime ),
	ESFF( 6, time, 0, animTime ),
	ESFF( 7, time, 0, animTime ),
	ESFF( 8, time, 0, animTime ),
	ESFF( 9, time, 0, animTime ),
	ESFF( 10, time, 0, animTime ),
	ESFF( 11, time, 0, animTime ),
	ESFF( 12, time, 0, animTime ),
	ESFF( 13, time, 0, animTime ),
	ESFF( 14, time, 0, animTime ),
	ESFF( 15, time, 0, animTime ),
	ESFF( 4, weight, 0, animWeight ),
	ESFF( 5, weight, 0, animWeight ),
	ESFF( 6, weight, 0, animWeight ),
	ESFF( 7, weight, 0, animWeight ),
	ESFF( 8, weight, 0, animWeight ),
	ESFF( 9, weight, 0, animWeight ),
	ESFF( 10, weight, 0, animWeight ),
	ESFF( 11, weight, 0, animWeight ),
	ESFF( 12, weight, 0, animWeight ),
	ESFF( 13, weight, 0, animWeight ),
	ESFF( 14, weight, 0, animWeight ),
	ESFF( 15, weight, 0, animWeight ),
	ESFB( 1, 1 ),
	ESFB( 2, 1 ),
	ESFB( 3, 1 ),
	ESFB( 4, 0 ),
	ESFB( 4, 1 ),
	ESFB( 4, 2 ),
	ESF( clientNum, 8, regular ),
	ESF( groundEntityNum, kEntityNumBits, regular ),
	ESFA( shader_data, 0, 0, regular ),
	ESFA( shader_data, 1, 0, regular ),
	ESF( shader_time, 0, regular ),
	ESFA( eyeVector, 0, 0, regular ),
	ESFA( eyeVector, 1, 0, regular ),
	ESFA( eyeVector, 2, 0, regular ),
	ESFA( surfaces, 6, 8, regular ),
	ESFA( surfaces, 7, 8, regular ),
	ESFA( surfaces, 8, 8, regular ),
	ESFA( surfaces, 9, 8, regular ),
	ESFA( surfaces, 10, 8, regular ),
	ESFA( surfaces, 11, 8, regular ),
	ESFA( surfaces, 12, 8, regular ),
	ESFA( surfaces, 13, 8, regular ),
	ESFA( surfaces, 14, 8, regular ),
	ESFA( surfaces, 15, 8, regular ),
	ESFA( surfaces, 16, 8, regular ),
	ESFA( surfaces, 17, 8, regular ),
	ESFA( surfaces, 18, 8, regular ),
	ESFA( surfaces, 19, 8, regular ),
	ESFA( surfaces, 20, 8, regular ),
	ESFA( surfaces, 21, 8, regular ),
	ESFA( surfaces, 22, 8, regular ),
	ESFA( surfaces, 23, 8, regular ),
	ESFA( surfaces, 24, 8, regular ),
	ESFA( surfaces, 25, 8, regular ),
	ESFA( surfaces, 26, 8, regular ),
	ESFA( surfaces, 27, 8, regular ),
	ESFA( surfaces, 28, 8, regular ),
	ESFA( surfaces, 29, 8, regular ),
	ESFA( surfaces, 30, 8, regular ),
	ESFA( surfaces, 31, 8, regular ),
};
constexpr int kNumEntityFields = (int)( sizeof( kEntityFields ) / sizeof( kEntityFields[0] ) );
static_assert( kNumEntityFields == 142, "protocol 6 sends 142 entity fields" );

#undef ESF
#undef ESFA
#undef ESFF
#undef ESFB

#define PSF( member, bits, type ) \
	{ offsetof( PlayerState, member ), (int)sizeof( PlayerState::member ), bits, FieldType::type }
#define PSFA( member, index, bits, type ) \
	{ offsetof( PlayerState, member ) + sizeof( PlayerState::member[0] ) * ( index ), \
		(int)sizeof( PlayerState::member[0] ), bits, FieldType::type }

/* playerStateFields_ver_6, in its order. */
const NetField kPlayerFields[] = {
	PSF( commandTime, 32, regular ),
	PSFA( origin, 0, 0, coord ),
	PSFA( origin, 1, 0, coord ),
	PSFA( viewangles, 1, 0, regular ),
	PSFA( velocity, 1, 0, velocity ),
	PSFA( velocity, 0, 0, velocity ),
	PSFA( viewangles, 0, 0, regular ),
	PSF( pm_time, -16, regular ),
	PSFA( origin, 2, 0, coord ),
	PSFA( velocity, 2, 0, velocity ),
	PSF( iViewModelAnimChanged, 2, regular ),
	PSFA( damage_angles, 0, -13, angle ),
	PSFA( damage_angles, 1, -13, angle ),
	PSFA( damage_angles, 2, -13, angle ),
	PSF( speed, 16, regular ),
	PSFA( delta_angles, 1, 16, regular ),
	PSF( viewheight, -8, regular ),
	PSF( groundEntityNum, kEntityNumBits, regular ),
	PSFA( delta_angles, 0, 16, regular ),
	PSF( iNetViewModelAnim, 4, regular ),
	PSF( fov, 0, regular ),
	PSF( current_music_mood, 8, regular ),
	PSF( gravity, 16, regular ),
	PSF( fallback_music_mood, 8, regular ),
	PSF( music_volume, 0, regular ),
	PSF( pm_flags, 16, regular ),
	PSF( clientNum, 8, regular ),
	PSF( fLeanAngle, 0, regular ),
	PSFA( blend, 3, 0, regular ),
	PSFA( blend, 0, 0, regular ),
	PSF( pm_type, 8, regular ),
	PSF( feetfalling, 8, regular ),
	PSFA( camera_angles, 0, 16, angle ),
	PSFA( camera_angles, 1, 16, angle ),
	PSFA( camera_angles, 2, 16, angle ),
	PSFA( camera_origin, 0, 0, coord ),
	PSFA( camera_origin, 1, 0, coord ),
	PSFA( camera_origin, 2, 0, coord ),
	PSFA( camera_posofs, 0, 0, coord ),
	PSFA( camera_posofs, 2, 0, coord ),
	PSF( camera_time, 0, regular ),
	PSF( bobCycle, 8, regular ),
	PSFA( delta_angles, 2, 16, regular ),
	PSFA( viewangles, 2, 0, regular ),
	PSF( music_volume_fade_time, 0, regular ),
	PSF( reverb_type, 6, regular ),
	PSF( reverb_level, 0, regular ),
	PSFA( blend, 1, 0, regular ),
	PSFA( blend, 2, 0, regular ),
	PSFA( camera_offset, 0, 0, regular ),
	PSFA( camera_offset, 1, 0, regular ),
	PSFA( camera_offset, 2, 0, regular ),
	PSFA( camera_posofs, 1, 0, coord ),
	PSF( camera_flags, 16, regular ),
};
constexpr int kNumPlayerFields = (int)( sizeof( kPlayerFields ) / sizeof( kPlayerFields[0] ) );
static_assert( kNumPlayerFields == 54, "protocol 6 sends 54 player fields" );

#undef PSF
#undef PSFA

/* A field's bytes, as the little-endian integer the wire carries. */
int LoadField( const void *field, int size ) {
	int value = 0;
	std::memcpy( &value, field, (std::size_t)size );
	if ( size == 1 ) {
		value = *static_cast<const unsigned char *>( field );
	} else if ( size == 2 ) {
		value = *static_cast<const short *>( field );
	}
	return value;
}

void StoreField( void *field, int size, int value ) {
	std::memcpy( field, &value, (std::size_t)size );
}

float LoadFloat( const void *field ) {
	float value;
	std::memcpy( &value, field, sizeof( value ) );
	return value;
}

void StoreFloat( void *field, float value ) {
	std::memcpy( field, &value, sizeof( value ) );
}

int FloatBits( float value ) {
	int bits;
	std::memcpy( &bits, &value, sizeof( bits ) );
	return bits;
}

float BitsFloat( int bits ) {
	float value;
	std::memcpy( &value, &bits, sizeof( value ) );
	return value;
}

/* ---- regular fields ---------------------------------------------------- */

/* An entity's regular field: a zero flag, then an integer of the field's
 * width, or for a float a small-integer or full-float form. */
void WriteRegular( msg_t *msg, const NetField &field, const void *toF ) {
	if ( field.bits == 0 ) {
		const float fullFloat = LoadFloat( toF );
		const int trunc = (int)fullFloat;

		if ( fullFloat == 0.0f ) {
			MSG_WriteBits( msg, 0, 1 );
		} else {
			MSG_WriteBits( msg, 1, 1 );
			if ( trunc == fullFloat && trunc + FLOAT_INT_BIAS >= 0 && trunc + FLOAT_INT_BIAS < ( 1 << FLOAT_INT_BITS ) ) {
				// send as small integer
				MSG_WriteBits( msg, 0, 1 );
				MSG_WriteBits( msg, trunc + FLOAT_INT_BIAS, -FLOAT_INT_BITS );
			} else {
				// send as full floating point value
				MSG_WriteBits( msg, 1, 1 );
				MSG_WriteBits( msg, FloatBits( fullFloat ), 32 );
			}
		}
	} else {
		const int value = LoadField( toF, field.size );
		if ( !value ) {
			MSG_WriteBits( msg, 0, 1 );
		} else {
			MSG_WriteBits( msg, 1, 1 );
			MSG_WriteBits( msg, value, field.bits );
		}
	}
}

void ReadRegular( msg_t *msg, const NetField &field, void *toF ) {
	if ( field.bits == 0 ) {
		if ( !MSG_ReadBitsExternal( msg, 1 ) ) {
			StoreFloat( toF, 0.0f );
		} else if ( !MSG_ReadBitsExternal( msg, 1 ) ) {
			StoreFloat( toF, (float)( MSG_ReadBitsExternal( msg, FLOAT_INT_BITS ) - FLOAT_INT_BIAS ) );
		} else {
			StoreFloat( toF, BitsFloat( MSG_ReadBitsExternal( msg, 32 ) ) );
		}
	} else if ( MSG_ReadBitsExternal( msg, 1 ) ) {
		StoreField( toF, field.size, MSG_ReadBitsExternal( msg, field.bits ) );
	} else {
		std::memset( toF, 0, (std::size_t)field.size );
	}
}

/* A player's regular field has no zero flag. */
void WriteRegularSimple( msg_t *msg, const NetField &field, const void *toF ) {
	if ( field.bits == 0 ) {
		const float fullFloat = LoadFloat( toF );
		const int trunc = (int)fullFloat;

		if ( trunc == fullFloat && trunc + FLOAT_INT_BIAS >= 0 && trunc + FLOAT_INT_BIAS < ( 1 << FLOAT_INT_BITS ) ) {
			MSG_WriteBits( msg, 0, 1 );
			MSG_WriteBits( msg, trunc + FLOAT_INT_BIAS, FLOAT_INT_BITS );
		} else {
			MSG_WriteBits( msg, 1, 1 );
			MSG_WriteBits( msg, FloatBits( fullFloat ), 32 );
		}
	} else {
		MSG_WriteBits( msg, LoadField( toF, field.size ), field.bits );
	}
}

void ReadRegularSimple( msg_t *msg, const NetField &field, void *toF ) {
	if ( field.bits == 0 ) {
		if ( MSG_ReadBitsExternal( msg, 1 ) == 0 ) {
			StoreFloat( toF, (float)( MSG_ReadBitsExternal( msg, FLOAT_INT_BITS ) - FLOAT_INT_BIAS ) );
		} else {
			StoreField( toF, 4, MSG_ReadBitsExternal( msg, 32 ) );
		}
	} else {
		StoreField( toF, field.size, MSG_ReadBitsExternal( msg, field.bits ) );
	}
}

/* ---- packed fields ------------------------------------------------------ */

void WritePackedAngle( msg_t *msg, float value, int bits ) {
	float tmp = value;

	if ( bits < 0 ) {
		if ( tmp < 0.0f ) {
			MSG_WriteBits( msg, 1, 1 );
			tmp = -tmp;
		} else {
			MSG_WriteBits( msg, 0, 1 );
		}
		bits = ~bits;
	}

	if ( bits == 12 ) {
		tmp = tmp * 4096.0f / 360.0f;
		MSG_WriteBits( msg, ( (int)tmp ) & 4095, 12 );
	} else if ( bits == 8 ) {
		tmp = tmp * 256.0f / 360.0f;
		MSG_WriteBits( msg, ( (int)tmp ) & 255, 8 );
	} else if ( bits == 16 ) {
		tmp = tmp * 65536.0f / 360.0f;
		MSG_WriteBits( msg, ( (int)tmp ) & 65535, 16 );
	} else {
		tmp = tmp * (float)( 1 << bits ) / 360.0f;
		MSG_WriteBits( msg, ( (int)tmp ) & ( ( 1 << bits ) - 1 ), bits );
	}
}

float ReadPackedAngle( msg_t *msg, int bits ) {
	float sign = 1.0f;
	if ( bits < 0 ) {
		if ( MSG_ReadBitsExternal( msg, 1 ) ) {
			sign = -1.0f;
		}
		bits = ~bits;
	}

	const int result = MSG_ReadBitsExternal( msg, bits );
	switch ( bits ) {
	case 8:
		return sign * result * 360.0f / 256.0f;
	case 12:
		return sign * result * 360.0f / 4096.0f;
	case 16:
		return sign * result * 360.0f / 65536.0f;
	default:
		return sign * 360.0f / (float)( 1 << bits ) * result;
	}
}

void WritePackedAnimTime( msg_t *msg, float value ) {
	int packed = (int)( value * 100.0f );
	if ( packed < 0 ) {
		packed = 0;
	} else if ( packed >= ( 1 << 15 ) ) {
		packed = 1 << 15;
	}
	MSG_WriteBits( msg, packed, 15 );
}

float ReadPackedAnimTime( msg_t *msg ) {
	return MSG_ReadBitsExternal( msg, 15 ) / 100.0f;
}

void WritePackedUnit( msg_t *msg, float value ) {
	int packed = (int)( ( value * 255.0f ) + 0.5f );
	if ( packed < 0 ) {
		packed = 0;
	} else if ( packed > 255 ) {
		packed = 255;
	}
	MSG_WriteBits( msg, packed, 8 );
}

float ReadPackedUnit( msg_t *msg ) {
	const float tmp = MSG_ReadBitsExternal( msg, 8 ) / 255.0f;
	return tmp < 0.0f ? 0.0f : tmp > 1.0f ? 1.0f : tmp;
}

void WritePackedScale( msg_t *msg, float value ) {
	int packed = (int)( value * 100.0f );
	if ( packed < 0 ) {
		packed = 0;
	} else if ( packed > 1023 ) {
		packed = 1023;
	}
	MSG_WriteBits( msg, packed, 10 );
}

float ReadPackedScale( msg_t *msg ) {
	return MSG_ReadBitsExternal( msg, 10 ) / 100.0f;
}

/* A coordinate: eighteen bits of sixteenths and a sign bit. */
void WritePackedCoord( msg_t *msg, float value ) {
	int packed = (int)( value * 16.0f );
	if ( value < 0 ) {
		packed = ( ( -packed ) & 262143 ) | 262144;
	} else {
		packed = packed & 262143;
	}
	MSG_WriteBits( msg, packed, 19 );
}

float ReadPackedCoord( msg_t *msg ) {
	float sign = 1.0f;
	int value = MSG_ReadBitsExternal( msg, 19 );
	if ( value & 262144 ) {
		sign = -1.0f;
	}
	value &= ~262144;
	return sign * value / 16.0f;
}

/* A velocity: sixteen bits of eighths and a sign bit. */
void WritePackedVelocity( msg_t *msg, float value ) {
	int packed = (int)( value * 8.0f );
	if ( value < 0 ) {
		packed = ( ( -packed ) & 65535 ) | 65536;
	} else {
		packed = packed & 65535;
	}
	MSG_WriteBits( msg, packed, 17 );
}

float ReadPackedVelocity( msg_t *msg ) {
	float sign = 1.0f;
	int value = MSG_ReadBitsExternal( msg, 17 );
	if ( value & 65536 ) {
		sign = -1.0f;
	}
	value &= ~65536;
	return sign * value / 8.0f;
}

void WriteField( msg_t *msg, const NetField &field, const void *toF, bool simple ) {
	switch ( field.type ) {
	case FieldType::regular:
		if ( simple ) {
			WriteRegularSimple( msg, field, toF );
		} else {
			WriteRegular( msg, field, toF );
		}
		break;
	case FieldType::angle:
		WritePackedAngle( msg, LoadFloat( toF ), field.bits );
		break;
	case FieldType::animTime:
		WritePackedAnimTime( msg, LoadFloat( toF ) );
		break;
	case FieldType::animWeight:
	case FieldType::alpha:
		WritePackedUnit( msg, LoadFloat( toF ) );
		break;
	case FieldType::scale:
		WritePackedScale( msg, LoadFloat( toF ) );
		break;
	case FieldType::coord:
		WritePackedCoord( msg, LoadFloat( toF ) );
		break;
	case FieldType::velocity:
		WritePackedVelocity( msg, LoadFloat( toF ) );
		break;
	}
}

void ReadField( msg_t *msg, const NetField &field, void *toF, bool simple ) {
	switch ( field.type ) {
	case FieldType::regular:
		if ( simple ) {
			ReadRegularSimple( msg, field, toF );
		} else {
			ReadRegular( msg, field, toF );
		}
		break;
	case FieldType::angle:
		StoreFloat( toF, ReadPackedAngle( msg, field.bits ) );
		break;
	case FieldType::animTime:
		StoreFloat( toF, ReadPackedAnimTime( msg ) );
		break;
	case FieldType::animWeight:
	case FieldType::alpha:
		StoreFloat( toF, ReadPackedUnit( msg ) );
		break;
	case FieldType::scale:
		StoreFloat( toF, ReadPackedScale( msg ) );
		break;
	case FieldType::coord:
		StoreFloat( toF, ReadPackedCoord( msg ) );
		break;
	case FieldType::velocity:
		StoreFloat( toF, ReadPackedVelocity( msg ) );
		break;
	}
}

/* 1.00 compares the field's bytes, not its packed value. */
bool FieldChanged( const NetField &field, const void *fromF, const void *toF ) {
	return std::memcmp( fromF, toF, (std::size_t)field.size ) != 0;
}

void WriteDelta( msg_t *msg, int oldV, int newV, int bits ) {
	if ( oldV == newV ) {
		MSG_WriteBits( msg, 0, 1 );
		return;
	}
	MSG_WriteBits( msg, 1, 1 );
	MSG_WriteBits( msg, newV, bits );
}

int ReadDelta( msg_t *msg, int oldV, int bits ) {
	return MSG_ReadBitsExternal( msg, 1 ) ? MSG_ReadBitsExternal( msg, bits ) : oldV;
}

void WriteDeltaFloat( msg_t *msg, float oldV, float newV ) {
	if ( oldV == newV ) {
		MSG_WriteBits( msg, 0, 1 );
		return;
	}
	MSG_WriteBits( msg, 1, 1 );
	MSG_WriteBits( msg, FloatBits( newV ), 32 );
}

float ReadDeltaFloat( msg_t *msg, float oldV ) {
	return MSG_ReadBitsExternal( msg, 1 ) ? BitsFloat( MSG_ReadBitsExternal( msg, 32 ) ) : oldV;
}

const unsigned int kBitMask[33] = {
	0x00000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000F, 0x0000001F, 0x0000003F, 0x0000007F,
	0x000000FF, 0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF, 0x00001FFF, 0x00003FFF, 0x00007FFF,
	0x0000FFFF, 0x0001FFFF, 0x0003FFFF, 0x0007FFFF, 0x000FFFFF, 0x001FFFFF, 0x003FFFFF, 0x007FFFFF,
	0x00FFFFFF, 0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF, 0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF,
	0xFFFFFFFF,
};

void WriteDeltaKey( msg_t *msg, int key, int oldV, int newV, int bits ) {
	if ( oldV == newV ) {
		MSG_WriteBits( msg, 0, 1 );
		return;
	}
	MSG_WriteBits( msg, 1, 1 );
	MSG_WriteBits( msg, newV ^ key, bits );
}

int ReadDeltaKey( msg_t *msg, int key, int oldV, int bits ) {
	if ( MSG_ReadBitsExternal( msg, 1 ) ) {
		return MSG_ReadBitsExternal( msg, bits ) ^ (int)( (unsigned int)key & kBitMask[bits] );
	}
	return oldV;
}

} // namespace

void NullEntityState( EntityState &state ) {
	std::memset( &state, 0, sizeof( state ) );
	state.alpha = 1.0f;
	state.scale = 1.0f;
	state.parent = FNTECH3_MOHAA_ENTITYNUM_NONE;
	state.constantLight = 0xffffff;
	state.renderfx = 16;
	for ( int i = 0; i < FNTECH3_MOHAA_NUM_BONE_CONTROLLERS; i++ ) {
		state.bone_tag[i] = -1;
	}
}

void WriteEntityNum( msg_t *msg, int number ) {
	MSG_WriteBits( msg, number % FNTECH3_MOHAA_MAX_GENTITIES, kEntityNumBits );
}

int ReadEntityNum( msg_t *msg ) {
	return MSG_ReadBitsExternal( msg, kEntityNumBits ) % FNTECH3_MOHAA_MAX_GENTITIES;
}

void WriteDeltaEntity( msg_t *msg, const EntityState *from, const EntityState *to, bool force ) {
	// a null `to` is a delta remove message
	if ( !to ) {
		if ( !from ) {
			return;
		}
		WriteEntityNum( msg, from->number );
		MSG_WriteBits( msg, 1, 1 );
		return;
	}

	if ( to->number < 0 || to->number >= FNTECH3_MOHAA_MAX_GENTITIES ) {
		Com_Error( ERR_FATAL, "MSG_WriteDeltaEntity: Bad entity number: %i", to->number );
	}

	bool changed[kNumEntityFields];
	int lc = 0;
	for ( int i = 0; i < kNumEntityFields; i++ ) {
		const NetField &field = kEntityFields[i];
		const byte *fromF = reinterpret_cast<const byte *>( from ) + field.offset;
		const byte *toF = reinterpret_cast<const byte *>( to ) + field.offset;
		changed[i] = FieldChanged( field, fromF, toF );
		if ( changed[i] ) {
			lc = i + 1;
		}
	}

	if ( lc == 0 ) {
		// nothing at all changed
		if ( !force ) {
			return;
		}
		WriteEntityNum( msg, to->number );
		MSG_WriteBits( msg, 0, 1 ); // not removed
		MSG_WriteBits( msg, 0, 1 ); // no delta
		return;
	}

	WriteEntityNum( msg, to->number );
	MSG_WriteBits( msg, 0, 1 ); // not removed
	MSG_WriteBits( msg, 1, 1 ); // we have a delta
	MSG_WriteByte( msg, lc );   // # of changes

	for ( int i = 0; i < lc; i++ ) {
		if ( !changed[i] ) {
			MSG_WriteBits( msg, 0, 1 );
			continue;
		}
		MSG_WriteBits( msg, 1, 1 );
		const NetField &field = kEntityFields[i];
		WriteField( msg, field, reinterpret_cast<const byte *>( to ) + field.offset, false );
	}
}

void ReadDeltaEntity( msg_t *msg, const EntityState *from, EntityState *to, int number ) {
	if ( number < 0 || number >= FNTECH3_MOHAA_MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "Bad delta entity number: %i", number );
		return;
	}

	// check for a remove
	if ( MSG_ReadBitsExternal( msg, 1 ) == 1 ) {
		std::memset( to, 0, sizeof( *to ) );
		to->number = kEntityNumEnd;
		return;
	}

	// check for no delta
	if ( MSG_ReadBitsExternal( msg, 1 ) == 0 ) {
		*to = *from;
		to->number = number;
		return;
	}

	const int lc = MSG_ReadByte( msg );
	if ( lc < 0 || lc > kNumEntityFields ) {
		Com_Error( ERR_DROP, "MSG_ReadDeltaEntity: number of changed fields (%i) is higher than total field count (%i)",
			lc, kNumEntityFields );
		return;
	}

	if ( to != from ) {
		*to = *from;
	}
	to->number = number;

	for ( int i = 0; i < lc; i++ ) {
		const NetField &field = kEntityFields[i];
		byte *toF = reinterpret_cast<byte *>( to ) + field.offset;
		if ( MSG_ReadBitsExternal( msg, 1 ) ) {
			ReadField( msg, field, toF, false );
		}
	}

	VectorCopy( to->netorigin, to->origin );
	VectorCopy( to->netangles, to->angles );
	EulerToQuat( to->angles, to->quat );

	// the bone controllers
	for ( int i = 0; i < FNTECH3_MOHAA_NUM_BONE_CONTROLLERS; i++ ) {
		if ( to->bone_tag[i] >= 0 ) {
			EulerToQuat( to->bone_angles[i], to->bone_quat[i] );
		}
	}
}

void WriteDeltaPlayerstate( msg_t *msg, const PlayerState *from, const PlayerState *to ) {
	PlayerState dummy;
	if ( !from ) {
		std::memset( &dummy, 0, sizeof( dummy ) );
		from = &dummy;
	}

	bool changed[kNumPlayerFields];
	int lc = 0;
	for ( int i = 0; i < kNumPlayerFields; i++ ) {
		const NetField &field = kPlayerFields[i];
		changed[i] = FieldChanged( field, reinterpret_cast<const byte *>( from ) + field.offset,
			reinterpret_cast<const byte *>( to ) + field.offset );
		if ( changed[i] ) {
			lc = i + 1;
		}
	}

	MSG_WriteByte( msg, lc ); // # of changes

	for ( int i = 0; i < lc; i++ ) {
		if ( !changed[i] ) {
			MSG_WriteBits( msg, 0, 1 );
			continue;
		}
		MSG_WriteBits( msg, 1, 1 );
		const NetField &field = kPlayerFields[i];
		WriteField( msg, field, reinterpret_cast<const byte *>( to ) + field.offset, true );
	}

	// the arrays
	int statsbits = 0;
	for ( int i = 0; i < FNTECH3_MOHAA_MAX_STATS; i++ ) {
		if ( to->stats[i] != from->stats[i] ) {
			statsbits |= 1 << i;
		}
	}
	int activeitemsbits = 0;
	for ( int i = 0; i < FNTECH3_MOHAA_MAX_ACTIVE_ITEMS; i++ ) {
		if ( to->activeItems[i] != from->activeItems[i] ) {
			activeitemsbits |= 1 << i;
		}
	}
	int ammo_amountbits = 0;
	for ( int i = 0; i < FNTECH3_MOHAA_MAX_AMMO; i++ ) {
		if ( to->ammo_amount[i] != from->ammo_amount[i] ) {
			ammo_amountbits |= 1 << i;
		}
	}
	int ammobits = 0;
	for ( int i = 0; i < FNTECH3_MOHAA_MAX_AMMO; i++ ) {
		if ( to->ammo_name_index[i] != from->ammo_name_index[i] ) {
			ammobits |= 1 << i;
		}
	}
	int max_ammo_amountbits = 0;
	for ( int i = 0; i < FNTECH3_MOHAA_MAX_AMMO; i++ ) {
		if ( to->max_ammo_amount[i] != from->max_ammo_amount[i] ) {
			max_ammo_amountbits |= 1 << i;
		}
	}

	if ( !statsbits && !activeitemsbits && !ammobits && !ammo_amountbits && !max_ammo_amountbits ) {
		MSG_WriteBits( msg, 0, 1 ); // no change
		return;
	}
	MSG_WriteBits( msg, 1, 1 ); // changed

	if ( statsbits ) {
		MSG_WriteBits( msg, 1, 1 );
		MSG_WriteLong( msg, statsbits );
		for ( int i = 0; i < FNTECH3_MOHAA_MAX_STATS; i++ ) {
			if ( statsbits & ( 1 << i ) ) {
				MSG_WriteShort( msg, to->stats[i] );
			}
		}
	} else {
		MSG_WriteBits( msg, 0, 1 );
	}

	if ( activeitemsbits ) {
		MSG_WriteBits( msg, 1, 1 );
		MSG_WriteBits( msg, activeitemsbits, FNTECH3_MOHAA_MAX_ACTIVE_ITEMS );
		for ( int i = 0; i < FNTECH3_MOHAA_MAX_ACTIVE_ITEMS; i++ ) {
			if ( activeitemsbits & ( 1 << i ) ) {
				MSG_WriteShort( msg, to->activeItems[i] );
			}
		}
	} else {
		MSG_WriteBits( msg, 0, 1 );
	}

	if ( ammo_amountbits ) {
		MSG_WriteBits( msg, 1, 1 );
		MSG_WriteBits( msg, ammo_amountbits, FNTECH3_MOHAA_MAX_AMMO );
		for ( int i = 0; i < FNTECH3_MOHAA_MAX_AMMO; i++ ) {
			if ( ammo_amountbits & ( 1 << i ) ) {
				MSG_WriteShort( msg, to->ammo_amount[i] );
			}
		}
	} else {
		MSG_WriteBits( msg, 0, 1 );
	}

	if ( ammobits ) {
		MSG_WriteBits( msg, 1, 1 );
		MSG_WriteBits( msg, ammobits, FNTECH3_MOHAA_MAX_AMMO );
		for ( int i = 0; i < FNTECH3_MOHAA_MAX_AMMO; i++ ) {
			if ( ammobits & ( 1 << i ) ) {
				MSG_WriteShort( msg, to->ammo_name_index[i] );
			}
		}
	} else {
		MSG_WriteBits( msg, 0, 1 );
	}

	if ( max_ammo_amountbits ) {
		MSG_WriteBits( msg, 1, 1 );
		MSG_WriteBits( msg, max_ammo_amountbits, FNTECH3_MOHAA_MAX_AMMO );
		for ( int i = 0; i < FNTECH3_MOHAA_MAX_AMMO; i++ ) {
			if ( max_ammo_amountbits & ( 1 << i ) ) {
				MSG_WriteShort( msg, to->max_ammo_amount[i] );
			}
		}
	} else {
		MSG_WriteBits( msg, 0, 1 );
	}
}

void ReadDeltaPlayerstate( msg_t *msg, const PlayerState *from, PlayerState *to ) {
	PlayerState dummy;
	if ( !from ) {
		std::memset( &dummy, 0, sizeof( dummy ) );
		from = &dummy;
	}
	if ( to != from ) {
		*to = *from;
	}

	const int lc = MSG_ReadByte( msg );
	if ( lc < 0 || lc > kNumPlayerFields ) {
		Com_Error( ERR_DROP, "MSG_ReadDeltaPlayerstate: %i changed fields of %i", lc, kNumPlayerFields );
		return;
	}
	for ( int i = 0; i < lc; i++ ) {
		const NetField &field = kPlayerFields[i];
		if ( MSG_ReadBitsExternal( msg, 1 ) ) {
			ReadField( msg, field, reinterpret_cast<byte *>( to ) + field.offset, true );
		}
	}

	// the arrays
	if ( MSG_ReadBitsExternal( msg, 1 ) ) {
		if ( MSG_ReadBitsExternal( msg, 1 ) ) {
			const int bits = MSG_ReadLong( msg );
			for ( int i = 0; i < FNTECH3_MOHAA_MAX_STATS; i++ ) {
				if ( bits & ( 1 << i ) ) {
					to->stats[i] = MSG_ReadShort( msg );
				}
			}
		}
		if ( MSG_ReadBitsExternal( msg, 1 ) ) {
			const int bits = MSG_ReadByte( msg );
			for ( int i = 0; i < FNTECH3_MOHAA_MAX_ACTIVE_ITEMS; i++ ) {
				if ( bits & ( 1 << i ) ) {
					to->activeItems[i] = MSG_ReadShort( msg );
				}
			}
		}
		if ( MSG_ReadBitsExternal( msg, 1 ) ) {
			const int bits = MSG_ReadShort( msg );
			for ( int i = 0; i < FNTECH3_MOHAA_MAX_AMMO; i++ ) {
				if ( bits & ( 1 << i ) ) {
					to->ammo_amount[i] = MSG_ReadShort( msg );
				}
			}
		}
		if ( MSG_ReadBitsExternal( msg, 1 ) ) {
			const int bits = MSG_ReadShort( msg );
			for ( int i = 0; i < FNTECH3_MOHAA_MAX_AMMO; i++ ) {
				if ( bits & ( 1 << i ) ) {
					to->ammo_name_index[i] = MSG_ReadShort( msg );
				}
			}
		}
		if ( MSG_ReadBitsExternal( msg, 1 ) ) {
			const int bits = MSG_ReadShort( msg );
			for ( int i = 0; i < FNTECH3_MOHAA_MAX_AMMO; i++ ) {
				if ( bits & ( 1 << i ) ) {
					to->max_ammo_amount[i] = MSG_ReadShort( msg );
				}
			}
		}
	}
}

void WriteSounds( msg_t *msg, const ServerSound *sounds, int count ) {
	if ( !count ) {
		MSG_WriteBits( msg, 0, 1 );
		return;
	}
	MSG_WriteBits( msg, 1, 1 );
	MSG_WriteBits( msg, count, 7 );

	for ( int i = 0; i < count; i++ ) {
		const ServerSound &sound = sounds[i];
		if ( !sound.stop_flag ) {
			MSG_WriteBits( msg, 0, 1 );
			MSG_WriteBits( msg, sound.streamed, 1 );

			if ( sound.origin[0] == 0.0f && sound.origin[1] == 0.0f && sound.origin[2] == 0.0f ) {
				MSG_WriteBits( msg, 0, 1 );
			} else {
				MSG_WriteBits( msg, 1, 1 );
				MSG_WriteFloat( msg, sound.origin[0] );
				MSG_WriteFloat( msg, sound.origin[1] );
				MSG_WriteFloat( msg, sound.origin[2] );
			}
			MSG_WriteBits( msg, sound.entity_number, 11 );
			MSG_WriteBits( msg, sound.channel, 7 );
			MSG_WriteBits( msg, sound.sound_index, 9 );

			if ( sound.volume != -1.0f ) {
				MSG_WriteBits( msg, 1, 1 );
				MSG_WriteFloat( msg, sound.volume );
			} else {
				MSG_WriteBits( msg, 0, 1 );
			}
			if ( sound.min_dist != -1.0f ) {
				MSG_WriteBits( msg, 1, 1 );
				MSG_WriteFloat( msg, sound.min_dist );
			} else {
				MSG_WriteBits( msg, 0, 1 );
			}
			if ( sound.pitch != -1.0f ) {
				MSG_WriteBits( msg, 1, 1 );
				MSG_WriteFloat( msg, sound.pitch );
			} else {
				MSG_WriteBits( msg, 0, 1 );
			}
			MSG_WriteFloat( msg, sound.maxDist );
		} else {
			MSG_WriteBits( msg, 1, 1 );
			MSG_WriteBits( msg, sound.entity_number, 10 );
			MSG_WriteBits( msg, sound.channel, 7 );
		}
	}
}

bool ReadSounds( msg_t *msg, ServerSound *sounds, int &count ) {
	count = 0;
	if ( !MSG_ReadBitsExternal( msg, 1 ) ) {
		return true;
	}
	const int total = MSG_ReadBitsExternal( msg, 7 );
	if ( total > kMaxSnapshotSounds ) {
		return false;
	}
	count = total;
	for ( int i = 0; i < total; i++ ) {
		ServerSound &sound = sounds[i];
		std::memset( &sound, 0, sizeof( sound ) );
		if ( MSG_ReadBitsExternal( msg, 1 ) == 1 ) {
			sound.entity_number = MSG_ReadBitsExternal( msg, 10 );
			sound.channel = MSG_ReadBitsExternal( msg, 7 );
			sound.stop_flag = qtrue;
		} else {
			sound.stop_flag = qfalse;
			sound.streamed = MSG_ReadBitsExternal( msg, 1 );
			if ( MSG_ReadBitsExternal( msg, 1 ) == 1 ) {
				sound.origin[0] = MSG_ReadFloat( msg );
				sound.origin[1] = MSG_ReadFloat( msg );
				sound.origin[2] = MSG_ReadFloat( msg );
			}
			sound.entity_number = MSG_ReadBitsExternal( msg, 11 );
			sound.channel = MSG_ReadBitsExternal( msg, 7 );
			sound.sound_index = (std::int16_t)MSG_ReadBitsExternal( msg, 9 );
			sound.volume = MSG_ReadBitsExternal( msg, 1 ) ? MSG_ReadFloat( msg ) : -1.0f;
			sound.min_dist = MSG_ReadBitsExternal( msg, 1 ) ? MSG_ReadFloat( msg ) : -1.0f;
			sound.pitch = MSG_ReadBitsExternal( msg, 1 ) ? MSG_ReadFloat( msg ) : 1.0f;
			sound.maxDist = MSG_ReadFloat( msg );
		}
	}
	return true;
}

void WriteDeltaUsercmdKey( msg_t *msg, int key, const UserCmd &from, const UserCmd &to ) {
	if ( to.serverTime - from.serverTime < 256 ) {
		MSG_WriteBits( msg, 1, 1 );
		MSG_WriteBits( msg, to.serverTime - from.serverTime, 8 );
	} else {
		MSG_WriteBits( msg, 0, 1 );
		MSG_WriteBits( msg, to.serverTime, 32 );
	}
	if ( from.angles[0] == to.angles[0] && from.angles[1] == to.angles[1] && from.angles[2] == to.angles[2]
		&& from.forwardmove == to.forwardmove && from.rightmove == to.rightmove && from.upmove == to.upmove
		&& from.buttons == to.buttons ) {
		MSG_WriteBits( msg, 0, 1 ); // no change
		return;
	}

	key ^= to.serverTime;
	MSG_WriteBits( msg, 1, 1 );
	WriteDeltaKey( msg, key, from.angles[0], to.angles[0], 16 );
	WriteDeltaKey( msg, key, from.angles[1], to.angles[1], 16 );
	WriteDeltaKey( msg, key, from.angles[2], to.angles[2], 16 );
	WriteDeltaKey( msg, key, from.forwardmove, to.forwardmove, 8 );
	WriteDeltaKey( msg, key, from.rightmove, to.rightmove, 8 );
	WriteDeltaKey( msg, key, from.upmove, to.upmove, 8 );
	WriteDeltaKey( msg, key, from.buttons, to.buttons, 16 );
}

void ReadDeltaUsercmdKey( msg_t *msg, int key, const UserCmd &from, UserCmd &to ) {
	if ( MSG_ReadBitsExternal( msg, 1 ) ) {
		to.serverTime = from.serverTime + MSG_ReadBitsExternal( msg, 8 );
	} else {
		to.serverTime = MSG_ReadBitsExternal( msg, 32 );
	}
	if ( MSG_ReadBitsExternal( msg, 1 ) ) {
		key ^= to.serverTime;
		to.angles[0] = (std::int16_t)ReadDeltaKey( msg, key, from.angles[0], 16 );
		to.angles[1] = (std::int16_t)ReadDeltaKey( msg, key, from.angles[1], 16 );
		to.angles[2] = (std::int16_t)ReadDeltaKey( msg, key, from.angles[2], 16 );
		to.forwardmove = (std::int8_t)ReadDeltaKey( msg, key, from.forwardmove, 8 );
		to.rightmove = (std::int8_t)ReadDeltaKey( msg, key, from.rightmove, 8 );
		to.upmove = (std::int8_t)ReadDeltaKey( msg, key, from.upmove, 8 );
		to.buttons = (std::uint16_t)ReadDeltaKey( msg, key, from.buttons, 16 );
	} else {
		to.angles[0] = from.angles[0];
		to.angles[1] = from.angles[1];
		to.angles[2] = from.angles[2];
		to.forwardmove = from.forwardmove;
		to.rightmove = from.rightmove;
		to.upmove = from.upmove;
		to.buttons = from.buttons;
	}
}

void WriteDeltaEyeInfo( msg_t *msg, const UserEyes &from, const UserEyes &to ) {
	if ( to.angles[0] != from.angles[0] || to.angles[1] != from.angles[1] || to.ofs[0] != from.ofs[0]
		|| to.ofs[1] != from.ofs[1] || to.ofs[2] != from.ofs[2] ) {
		MSG_WriteBits( msg, 1, 1 );
		WriteDelta( msg, from.ofs[0], to.ofs[0], 8 );
		WriteDelta( msg, from.ofs[1], to.ofs[1], 8 );
		WriteDelta( msg, from.ofs[2], to.ofs[2], 8 );
		WriteDeltaFloat( msg, from.angles[0], to.angles[0] );
		WriteDeltaFloat( msg, from.angles[1], to.angles[1] );
	} else {
		MSG_WriteBits( msg, 0, 1 );
	}
}

void ReadDeltaEyeInfo( msg_t *msg, const UserEyes &from, UserEyes &to ) {
	if ( MSG_ReadBitsExternal( msg, 1 ) ) {
		to.ofs[0] = (std::int8_t)ReadDelta( msg, from.ofs[0], 8 );
		to.ofs[1] = (std::int8_t)ReadDelta( msg, from.ofs[1], 8 );
		to.ofs[2] = (std::int8_t)ReadDelta( msg, from.ofs[2], 8 );
		to.angles[0] = ReadDeltaFloat( msg, from.angles[0] );
		to.angles[1] = ReadDeltaFloat( msg, from.angles[1] );
	} else {
		to = from;
	}
}

int EntityFieldCount() {
	return kNumEntityFields;
}

int PlayerFieldCount() {
	return kNumPlayerFields;
}

} // namespace fntech3::mohaa::wire
