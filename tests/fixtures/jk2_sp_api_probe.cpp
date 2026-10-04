/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Native boundary probe; no game window, input, game Init or save is used.
 * Supply the legitimate 1.02a jk2gamex86.dll as argv[1]. GetGameAPI parses
 * optional item data through two imports; this probe supplies an empty input
 * and makes every other import fail explicitly. That proves table discovery
 * and version/stride validation only, not item data or gameplay. */
#include "../../code/compat/jk2_sp_game_api.h"
#include <cstdio>
#include <cstring>
#include <csetjmp>
#include <cstdint>
#include <utility>
#if defined(_WIN32) && UINTPTR_MAX == UINT32_MAX
#include <windows.h>

static std::jmp_buf refused;
static int refusal = -1;
template<int Slot> static void Refuse() { refusal = Slot; std::longjmp( refused, 1 ); }
template<std::size_t... I> static void FillRefusals( fntech3_jk2_sp_game_import_t &table, std::index_sequence<I...> ) {
	void ( *callbacks[] )() = { &Refuse<static_cast<int>( I )>... };
	std::memcpy( &table, callbacks, sizeof( table ) );
}
static int ReadFile( const char *path, void **buffer ) {
	std::printf( "FS_ReadFile: %s (empty fixture)\n", path );
	if ( buffer ) *buffer = nullptr;
	return -1;
}
static void FreeFile( void *buffer ) {
	if ( buffer ) { refusal = 15; std::longjmp( refused, 1 ); }
}
#endif

int main( int argc, char **argv ) {
#if defined(_WIN32) && UINTPTR_MAX == UINT32_MAX
	if ( argc != 2 ) { std::fprintf( stderr, "usage: probe.exe absolute-jk2gamex86.dll-path\n" ); return 2; }
	HMODULE module = LoadLibraryExA( argv[1], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH );
	if ( !module ) { std::fprintf( stderr, "LoadLibrary failed: %lu\n", GetLastError() ); return 3; }
	auto getAPI = reinterpret_cast<fntech3_jk2_sp_GetGameAPI_t>( GetProcAddress( module, "GetGameAPI" ) );
	if ( !getAPI ) { FreeLibrary( module ); return 4; }
	fntech3_jk2_sp_game_import_t imports{};
	FillRefusals( imports, std::make_index_sequence<FNTECH3_JK2_SP_GAME_IMPORT_COUNT>{} );
	std::int32_t voices[1024]{}; imports.VoiceVolume = voices;
	imports.FS_ReadFile = ReadFile; imports.FS_FreeFile = FreeFile;
	int result = 0;
	if ( setjmp( refused ) ) { std::fprintf( stderr, "refused import %i\n", refusal ); result = 5; }
	else {
		auto *game = getAPI( &imports );
		const auto error = fntech3::jk2::sp::ValidateExports( game );
		std::printf( "GetGameAPI validation=%i api=%i imports=%zu exports=%zu entity_stride=%i count=%i\n",
			static_cast<int>( error ), game ? game->apiversion : -1, sizeof( imports ),
			sizeof( fntech3_jk2_sp_game_export_t ), game ? game->gentitySize : -1, game ? game->num_entities : -1 );
		if ( error != fntech3::jk2::sp::ExportError::None ) result = 6;
	}
	FreeLibrary( module );
	return result;
#else
	(void)argc; (void)argv;
	std::fprintf( stderr, "Jedi Outcast SP retail modules require Windows x86\n" );
	return 2;
#endif
}
