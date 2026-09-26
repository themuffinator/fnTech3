/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_STVEF_SP_FEEDBACK_HPP
#define FNTECH3_CLIENT_STVEF_SP_FEEDBACK_HPP

#include "stvef_force_feedback_native.hpp"
#include <charconv>
#include <filesystem>

// Included by cl_main.cpp. Platform input owns the window lifetime and tells
// this controller when the window can drive a device. No device is opened by
// registration, another profile, a disabled cvar, or a background window.
struct STVEFSPFeedback {
	fntech3::stvef::feedback::Native native;
	fntech3::stvef::feedback::Player<fntech3::stvef::feedback::Native> player{ native };
	cvar_t *enabled = nullptr, *gain = nullptr, *tension = nullptr;
	void *instance = nullptr, *window = nullptr;
	bool active = false, attempted = false, mouseFirst = true;
};
static STVEFSPFeedback stvefFeedback;

void CL_STVEFSPFeedbackSuspend( void ) {
	auto &state = stvefFeedback;
	state.native.Close();
	state.player.Clear();
	state.active = state.attempted = false;
}

static bool CL_STVEFSPFeedbackReady() {
	const auto &state = stvefFeedback;
	return Com_GameIsSTVEFSP() && state.enabled && state.enabled->integer && state.active && state.native.Available();
}

void CL_STVEFSPFeedbackPlay( int id, qboolean ensure ) {
	if ( !CL_STVEFSPFeedbackReady() ) return;
	auto &state = stvefFeedback;
	bool restoreSpring = !ensure;
	if ( ensure ) restoreSpring = state.player.Ensure( id, state.gain->value );
	else state.player.Play( id, state.gain->value );
	if ( restoreSpring ) state.native.Spring( fntech3::stvef::feedback::Tension( state.tension->integer ) );
}

void CL_STVEFSPFeedbackStop( int id ) {
	if ( Com_GameIsSTVEFSP() ) stvefFeedback.player.Stop( id );
}
void CL_STVEFSPFeedbackStopAll( void ) {
	if ( Com_GameIsSTVEFSP() ) stvefFeedback.player.StopAll();
}

static void CL_STVEFSPFeedbackMouse_f() {
	CL_STVEFSPFeedbackSuspend();
	stvefFeedback.mouseFirst = true;
}
static void CL_STVEFSPFeedbackJoystick_f() {
	CL_STVEFSPFeedbackSuspend();
	stvefFeedback.mouseFirst = false;
}
static int CL_STVEFSPFeedbackArgument() {
	if ( Cmd_Argc() != 2 ) return -1;
	const char *text = Cmd_Argv( 1 ), *end = text + strlen( text );
	int value = -1;
	const auto result = std::from_chars( text, end, value );
	return result.ec == std::errc{} && result.ptr == end ? value : -1;
}
static void CL_STVEFSPFeedbackTension_f() {
	const int value = CL_STVEFSPFeedbackArgument();
	if ( value < 0 || value > 3 ) { Com_Printf( "Usage: ff_tension <0..3>\n" ); return; }
	Cvar_Set( "ff_defaultTension", va( "%d", value ) );
	if ( CL_STVEFSPFeedbackReady() ) stvefFeedback.native.Spring( fntech3::stvef::feedback::Tension( value ) );
}
static void CL_STVEFSPFeedbackSpring_f() {
	const int value = CL_STVEFSPFeedbackArgument();
	if ( value < 0 || value > 10000 ) { Com_Printf( "Usage: ff_spring <0..10000>\n" ); return; }
	if ( !CL_STVEFSPFeedbackReady() || !stvefFeedback.native.Spring( value ) )
		Com_Printf( "Force-feedback spring unavailable.\n" );
}
static void CL_STVEFSPFeedbackPlay_f() {
	using namespace fntech3::stvef::feedback;
	int id = -1;
	if ( Cmd_Argc() == 2 ) id = CL_STVEFSPFeedbackArgument();
	else if ( Cmd_Argc() == 3 && !Q_stricmp( Cmd_Argv( 1 ), "name" ) ) {
		for ( int i = 0; i < EffectCount; ++i ) if ( !Q_stricmp( Cmd_Argv( 2 ), Effects[i].name ) ) { id = i; break; }
	}
	if ( !ValidEffect( id ) ) { Com_Printf( "Usage: ff_play <0..31> or ff_play name <effect>\n" ); return; }
	if ( !CL_STVEFSPFeedbackReady() ) { Com_Printf( "Force feedback unavailable; enable use_ff and focus the game window.\n" ); return; }
	CL_STVEFSPFeedbackPlay( id, qfalse );
}

void CL_STVEFSPFeedbackShutdown( void ) {
	CL_STVEFSPFeedbackSuspend();
	auto &state = stvefFeedback;
	if ( state.enabled ) {
		for ( const char *name : { "ff_usemouse", "ff_usejoy", "ff_tension", "ff_spring", "ff_play" } ) Cmd_RemoveCommand( name );
	}
	state.enabled = state.gain = state.tension = nullptr;
	state.instance = state.window = nullptr;
	state.mouseFirst = true;
}

void CL_STVEFSPFeedbackInit( void *instance, void *window ) {
	CL_STVEFSPFeedbackShutdown();
	if ( !Com_GameIsSTVEFSP() ) return;
	auto &state = stvefFeedback;
	state.instance = instance;
	state.window = window;
	state.enabled = Cvar_Get( "use_ff", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( state.enabled, "0", "1", CV_INTEGER );
	Cvar_SetDescription( state.enabled, "Elite Force SP force feedback through the installation's FFC10.dll; requires a compatible device and Windows x86." );
	state.gain = Cvar_Get( "js_ffmult", "3.0", CVAR_ARCHIVE );
	state.tension = Cvar_Get( "ff_defaultTension", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( state.tension, "0", "3", CV_INTEGER );
	Cmd_AddCommand( "ff_usemouse", CL_STVEFSPFeedbackMouse_f );
	Cmd_AddCommand( "ff_usejoy", CL_STVEFSPFeedbackJoystick_f );
	Cmd_AddCommand( "ff_tension", CL_STVEFSPFeedbackTension_f );
	Cmd_AddCommand( "ff_spring", CL_STVEFSPFeedbackSpring_f );
	Cmd_AddCommand( "ff_play", CL_STVEFSPFeedbackPlay_f );
}

void CL_STVEFSPFeedbackFrame( qboolean active ) {
	auto &state = stvefFeedback;
	if ( !Com_GameIsSTVEFSP() || !state.enabled ) return;
	if ( !active || !state.enabled->integer ) {
		if ( state.active || state.attempted ) CL_STVEFSPFeedbackSuspend();
		return;
	}
	state.active = true;
	if ( !state.attempted ) {
		state.attempted = true; // Do not retry a failed device every frame.
		std::error_code error;
		const auto path = std::filesystem::absolute(
			std::filesystem::u8path( Cvar_VariableString( "fs_basepath" ) ) / "FFC10.dll", error ).lexically_normal();
		if ( error ) { Com_Printf( "Cannot resolve the installation's FFC10.dll path.\n" ); return; }
		if ( !state.native.Open( path.u8string().c_str(), state.instance, state.window, state.mouseFirst, state.tension->integer ) ) {
			Com_Printf( "Force feedback unavailable: %s\n", state.native.Error() );
			return;
		}
		Com_Printf( "Elite Force force feedback: %s%s\n", state.native.IsMouse() ? "FEELit mouse" : "DirectInput device",
			!state.native.IsMouse() && !state.native.HasSpring() ? " (spring unavailable)" : "" );
		state.tension->modified = qfalse;
	}
	if ( state.native.Available() && state.tension->modified ) {
		state.native.Spring( fntech3::stvef::feedback::Tension( state.tension->integer ) );
		state.tension->modified = qfalse;
	}
}

#endif
