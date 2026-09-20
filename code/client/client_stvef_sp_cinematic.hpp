/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_STVEF_SP_CINEMATIC_HPP
#define FNTECH3_CLIENT_STVEF_SP_CINEMATIC_HPP

/* Included by cl_cin.cpp after its shared handle cleanup. RoQ and Bink keep
 * the same handle lifecycle but never enter each other's decoder state. */
static e_status CIN_RunBink( int handle ) {
	using Result = fntech3::stvef::bink::Movie::Result;
	auto &movie = cinTable[handle];
	if ( movie.status == FMV_IDLE ) return FMV_IDLE;
	const bool mute = movie.silent ||
		( !gw_active && Cvar_VariableIntegerValue( "s_muteWhenUnfocused" ) ) ||
		( gw_minimized && Cvar_VariableIntegerValue( "s_muteWhenMinimized" ) );
	movie.bink->Volume( mute ? 0.0f : Cvar_VariableValue( "s_volume" ) );
	const auto result = movie.bink->Step( static_cast<std::uint32_t>( Sys_Milliseconds() ) );
	if ( result == Result::Frame ) {
		movie.buf = movie.bink->Pixels();
		movie.dirty = true;
	}
	if ( result != Result::End ) return movie.status;
	if ( !*movie.bink->Error() && movie.looping && movie.bink->Rewind() ) {
		movie.buf = nullptr;
		return FMV_PLAY;
	}
	if ( !*movie.bink->Error() && movie.holdAtEnd ) {
		movie.bink->Volume( 0 );
		movie.status = FMV_IDLE;
		return FMV_IDLE;
	}
	if ( *movie.bink->Error() ) Com_Printf( "Cannot decode %s: %s\n", movie.fileName.data(), movie.bink->Error() );
	Com_DPrintf( "Bink finished: %s\n", movie.fileName.data() );
	currentHandle = handle;
	RoQShutdown();
	return FMV_EOF;
}

static std::unique_ptr<fntech3::stvef::bink::Movie> CIN_OpenBink(
	const char *qpath, const char *runtime, bool german, bool sound ) {
	ScopedFileHandle file;
	const int length = OpenFileRead( qpath, file, qtrue );
	if ( !file ) return nullptr;
	if ( length < 44 || static_cast<std::size_t>( length ) > fntech3::stvef::bink::MaxFileBytes ) {
		Com_Printf( "Invalid Bink movie size: %s\n", qpath );
		return nullptr;
	}
	std::vector<std::uint8_t> bytes( static_cast<std::size_t>( length ) );
	if ( FileRead( file.get(), bytes.data(), length ) != length ) {
		Com_Printf( "Truncated Bink movie: %s\n", qpath );
		return nullptr;
	}
	file.reset();
	auto decoder = std::make_unique<fntech3::stvef::bink::Movie>();
	if ( !decoder->Open( std::move( bytes ), runtime, german, sound ) ) {
		Com_Printf( "Cannot play %s: %s\n", qpath, decoder->Error() );
		return nullptr;
	}
	return decoder;
}

static int CIN_PlayBink( const char *qpath, int x, int y, int w, int h, int bits ) {
	// Find the index before acquiring resources: ERR_DROP uses longjmp.
	const int handle = CIN_HandleForVideo();
	char chosen[MAX_OSPATH], alternate[MAX_OSPATH], stem[MAX_OSPATH];
	Q_strncpyz( chosen, qpath, sizeof( chosen ) );
	COM_StripExtension( qpath, stem, sizeof( stem ) );
	Com_sprintf( alternate, sizeof( alternate ), "%s_lo.bik", stem );
	const bool alreadyLow = strlen( stem ) >= 3 && !Q_stricmp( stem + strlen( stem ) - 3, "_lo" );
	if ( Cvar_VariableIntegerValue( "cl_VideoUsingLoRes" ) && !alreadyLow ) {
		Q_strncpyz( chosen, alternate, sizeof( chosen ) );
		Q_strncpyz( alternate, qpath, sizeof( alternate ) );
	}
	std::error_code pathError;
	const auto runtime = std::filesystem::absolute( std::filesystem::u8path(
		Cvar_VariableString( "fs_basepath" ) ) / "binkw32.dll", pathError ).lexically_normal().u8string();
	if ( pathError ) {
		Com_Printf( "Cannot resolve the installation's Bink runtime path\n" );
		return -1;
	}
	const bool sound = !( bits & CIN_silent ) && Cvar_VariableIntegerValue( "s_initsound" );
	const bool german = !Q_stricmp( Cvar_VariableString( "s_language" ), "DEUTSCH" );
	auto decoder = CIN_OpenBink( chosen, runtime.c_str(), german, sound );
	// Retail retries the other resolution when opening the preferred file
	// fails, including a damaged header, before giving up on the movie.
	if ( !decoder && !alreadyLow ) {
		decoder = CIN_OpenBink( alternate, runtime.c_str(), german, sound );
		if ( decoder ) Q_strncpyz( chosen, alternate, sizeof( chosen ) );
	}
	if ( !decoder ) {
		Com_Printf( "Bink movie unavailable: %s\n", qpath );
		return -1;
	}
	const auto info = decoder->Info();
	Com_Printf( "Bink: %s, %ux%u, %u frames, track %u, sound %s\n", chosen,
		info.width, info.height, info.frames, info.soundTrack, decoder->Sound() ? "on" : "off" );
	if ( sound && info.tracks && !decoder->Sound() ) Com_Printf( "Bink audio device unavailable\n" );
	auto &movie = cinTable[handle];
	movie.bink = std::move( decoder );
	Q_strncpyz( movie.fileName.data(), chosen, static_cast<int>( movie.fileName.size() ) );
	movie.status = FMV_PLAY;
	movie.buf = nullptr;
	movie.startTime = Sys_Milliseconds();
	movie.CIN_WIDTH = movie.drawX = info.width;
	movie.CIN_HEIGHT = movie.drawY = info.height;
	movie.xpos = x; movie.ypos = y; movie.width = w; movie.height = h;
	movie.looping = ( bits & CIN_loop ) != 0;
	movie.holdAtEnd = ( bits & CIN_hold ) != 0;
	movie.alterGameState = ( bits & CIN_system ) != 0;
	movie.silent = ( bits & CIN_silent ) != 0;
	movie.shader = ( bits & CIN_shader ) != 0;
	movie.rtcwLetterBox = false;
	movie.stvefInGame = false;
	movie.playonwalls = 1;
	movie.dirty = true;
	if ( movie.alterGameState ) {
		if ( uivm ) VM_Call( uivm, 1, UI_SET_ACTIVE_MENU, UIMENU_NONE );
		cls.state = CA_CINEMATIC;
	}
	Con_Close();
	return CIN_RunBink( handle ) == FMV_EOF ? -1 : handle;
}

#endif
