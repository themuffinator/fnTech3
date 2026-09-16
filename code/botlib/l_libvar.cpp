/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

/*****************************************************************************
 * name:		l_libvar.c
 *
 * desc:		bot library variables
 *
 * $Archive: /MissionPack/code/botlib/l_libvar.c $
 *
 *****************************************************************************/

#include "../qcommon/q_shared.h"
#include "l_memory.h"
#include "l_libvar.h"

//list with library variables
libvar_t *libvarlist = nullptr;

//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
static float LibVarStringValue( const char *string )
{
	bool fraction = false;
	float divisor = 10.0f;
	float value = 0;
	// Preserve ordinary unsigned decimal arithmetic. A trailing dot must not
	// read past NUL; long fractions must not overflow an integer divisor.
	for ( ; *string; ++string ) {
		if ( *string == '.' && !fraction ) { fraction = true; continue; }
		if ( *string < '0' || *string > '9' ) return 0;
		if ( fraction ) {
			if ( divisor ) {
				value += static_cast<float>( *string - '0' ) / divisor;
				divisor = divisor < 1.0e37f ? divisor * 10.0f : 0.0f;
			}
		} else {
			value = value * 10.0 + static_cast<float>( *string - '0' );
		}
		uint32_t bits;
		memcpy( &bits, &value, sizeof( bits ) );
		if ( ( bits & 0x7f800000u ) == 0x7f800000u ) return 0;
	}
	return value;
} //end of the function LibVarStringValue
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
libvar_t *LibVarAlloc( const char *var_name )
{
	libvar_t *v;

	v = botlib::Alloc<libvar_t>();
	*v = {};
	// Publish ownership before another allocation can abort initialization.
	v->next = libvarlist;
	libvarlist = v;
	v->name = botlib::CopyString(var_name);
	return v;
} //end of the function LibVarAlloc
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void LibVarDeAlloc( libvar_t *v )
{
	if ( v->string )
		FreeMemory( v->string );
	if ( v->name )
		FreeMemory( v->name );
	FreeMemory( v );
} //end of the function LibVarDeAlloc
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void LibVarDeAllocAll(void)
{
	libvar_t *v;

	for ( v = libvarlist; v; v = libvarlist )
	{
		libvarlist = libvarlist->next;
		LibVarDeAlloc( v );
	} //end for
	libvarlist = nullptr;
} //end of the function LibVarDeAllocAll
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
libvar_t *LibVarGet( const char *var_name )
{
	libvar_t *v;

	for ( v = libvarlist; v; v = v->next )
	{
		if ( !Q_stricmp( v->name, var_name ) )
		{
			return v;
		} //end if
	} //end for
	return nullptr;
} //end of the function LibVarGet
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
const char *LibVarGetString( const char *var_name )
{
	libvar_t *v;

	v = LibVarGet( var_name );
	if ( v )
	{
		return v->string;
	} //end if
	else
	{
		return "";
	} //end else
} //end of the function LibVarGetString
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
float LibVarGetValue( const char *var_name )
{
	libvar_t *v;

	v = LibVarGet( var_name );
	if ( v )
	{
		return v->value;
	} //end if
	else
	{
		return 0;
	} //end else
} //end of the function LibVarGetValue
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
libvar_t *LibVar( const char *var_name, const char *value )
{
	libvar_t *v;
	v = LibVarGet( var_name );
	if ( v ) 
		return v;
	//create new variable
	v = LibVarAlloc( var_name );
	//variable string
	v->string = botlib::CopyString(value);
	//the value
	v->value = LibVarStringValue( v->string );
	//variable is modified
	v->modified = qtrue;
	//
	return v;
} //end of the function LibVar
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
const char *LibVarString( const char *var_name, const char *value )
{
	libvar_t *v;

	v = LibVar( var_name, value );
	return v->string;
} //end of the function LibVarString
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
float LibVarValue( const char *var_name, const char *value )
{
	libvar_t *v;

	v = LibVar( var_name, value );
	return v->value;
} //end of the function LibVarValue
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void LibVarSet( const char *var_name, const char *value )
{
	libvar_t *v;

	v = LibVarGet(var_name);
	if ( !v )
	{
		v = LibVarAlloc( var_name );
	} //end else
	// Keep the previous value owned if copying fails, including aliased input.
	char *replacement = botlib::CopyString(value);
	if ( v->string ) FreeMemory( v->string );
	v->string = replacement;
	//the value
	v->value = LibVarStringValue( v->string );
	//variable is modified
	v->modified = qtrue;
} //end of the function LibVarSet
#if 0
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
qboolean LibVarChanged( const char *var_name )
{
	libvar_t *v;

	v = LibVarGet( var_name );
	if ( v )
	{
		return v->modified;
	} //end if
	else
	{
		return qfalse;
	} //end else
} //end of the function LibVarChanged
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void LibVarSetNotModified( const char *var_name )
{
	libvar_t *v;

	v = LibVarGet( var_name );
	if ( v )
	{
		v->modified = qfalse;
	} //end if
} //end of the function LibVarSetNotModified
#endif
