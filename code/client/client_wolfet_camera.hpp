/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_WOLFET_CAMERA_HPP
#define FNTECH3_CLIENT_WOLFET_CAMERA_HPP
#include "../compat/rtcw_camera.hpp"
#include "../compat/wolfet_module_abi.h"
#include <climits>

/* Enemy Territory's spline camera paths. ET keeps 64 slots, as the evaluator
 * does, and its own camera reads the same path files.
 *
 * Read against the pinned original, ET starts the position and clears its
 * velocities *before* it builds the timeline, and its feather event ramps the
 * speed up over the first second and down over the last. Those are the two
 * behaviours the shared evaluator distinguishes, and both are the ones it calls
 * SinglePlayer. That name is the evaluator's, not a claim that ET is another
 * profile's single player; ET keeps its own slots and neither profile can see
 * the other's. */
inline fntech3::rtcw::camera::Manager &CL_WolfETCameras() {
	static fntech3::rtcw::camera::Manager manager;
	return manager;
}

inline void CL_ClearWolfETCameras() {
	CL_WolfETCameras().Clear();
	cl.wolfetCameraMode = qfalse;
}

inline int CL_WolfETCameraTime( intptr_t value ) {
	if ( value < INT_MIN || value > INT_MAX ) Com_Error( ERR_DROP, "ET camera time exceeds signed 32 bits" );
	return static_cast<int>( value );
}

inline void *CL_WolfETCameraSpan( vm_t *vm, intptr_t address, std::size_t bytes ) {
	if ( !vm || !address || bytes > (std::numeric_limits<std::uintptr_t>::max)() - static_cast<std::uintptr_t>( address ) )
		Com_Error( ERR_DROP, "Invalid ET camera buffer address" );
	VM_CHECKBOUNDS( vm, address, bytes );
	void *result = VM_ExplicitArgPtr( vm, address );
	if ( !result ) Com_Error( ERR_DROP, "Null ET camera buffer" );
	return result;
}

inline bool CL_LoadWolfETCamera( vm_t *vm, int index, intptr_t address ) {
	using namespace fntech3::rtcw::camera;
	// A failed replacement must not leave an older camera in this slot.
	CL_WolfETCameras().Clear( index );
	std::array<char, 256> name{};
	bool terminated = false;
	for ( std::size_t i = 0; i < name.size(); ++i ) {
		const auto *source = static_cast<const char *>( CL_WolfETCameraSpan( vm, address, i + 1 ) );
		name[i] = source[i];
		if ( !name[i] ) { terminated = true; break; }
	}
	if ( !terminated || !name[0] || name[0] == '/' || name[0] == '\\' ||
		std::strchr( name.data(), ':' ) || std::strstr( name.data(), ".." ) ) return false;
	fileHandle_t file = 0;
	const int bytes = FS_FOpenFileRead( name.data(), &file, qtrue );
	if ( !file ) return false;
	if ( bytes <= 0 || static_cast<std::size_t>( bytes ) > MaxFileBytes ) { FS_FCloseFile( file ); return false; }
	void *text = Hunk_AllocateTempMemory( bytes );
	const int read = FS_Read( text, bytes, file );
	FS_FCloseFile( file );
	Diagnostic diagnostic;
	const bool loaded = read == bytes && CL_WolfETCameras().Load( index,
		static_cast<const char *>( text ), bytes, diagnostic, Mode::SinglePlayer );
	Hunk_FreeTempMemory( text );
	if ( diagnostic.message ) Com_Printf( "ET camera %s at byte %zu: %s\n", name.data(), diagnostic.offset, diagnostic.message );
	return loaded;
}

/* False means the caller must continue its own dispatch. */
inline bool CL_WolfETCameraCall( vm_t *vm, fntech3::wolfet::module::Call call,
	intptr_t *args, intptr_t &result ) {
	using namespace fntech3::rtcw::camera;
	using fntech3::wolfet::module::Call;
	if ( !Com_GameIsWolfET() ) return false;
	switch ( call ) {
	case Call::CG_LOADCAMERA: case Call::CG_STARTCAMERA:
	case Call::CG_STOPCAMERA: case Call::CG_GETCAMERAINFO: break;
	default: return false;
	}
	result = 0;
	// An invalid slot never examines a filename, a time or an output pointer.
	if ( !Manager::ValidSlot( args[1] ) ) return true;
	const int index = static_cast<int>( args[1] );
	if ( call == Call::CG_LOADCAMERA ) { result = CL_LoadWolfETCamera( vm, index, args[2] ); return true; }
	if ( call == Call::CG_STOPCAMERA ) {
		// ET's own client only releases the primary camera's input policy here;
		// it does not stop a path already running.
		if ( index == 0 ) cl.wolfetCameraMode = qfalse;
		return true;
	}
	const int time = CL_WolfETCameraTime( args[2] );
	Diagnostic diagnostic;
	if ( call == Call::CG_STARTCAMERA ) {
		if ( index == 0 ) cl.wolfetCameraMode = qtrue;
		if ( !CL_WolfETCameras().Start( index, time, diagnostic ) )
			Com_Error( ERR_DROP, "ET camera %d: %s", index, diagnostic.message );
		return true;
	}
	/* ET writes an origin, a pitch and a yaw, and a field of view. Roll is the
	 * module's and is left alone. Overlapping outputs would make the result
	 * depend on write order, so they are refused. */
	std::array<void *, 3> outputs{};
	constexpr std::array<std::size_t, 3> sizes{ { 12, 12, 4 } };
	for ( std::size_t i = 0; i < outputs.size(); ++i ) outputs[i] = CL_WolfETCameraSpan( vm, args[i + 3], sizes[i] );
	for ( std::size_t i = 0; i < outputs.size(); ++i ) for ( std::size_t j = 0; j < i; ++j ) {
		const auto a = reinterpret_cast<std::uintptr_t>( outputs[i] ), b = reinterpret_cast<std::uintptr_t>( outputs[j] );
		if ( a < b + sizes[j] && b < a + sizes[i] ) Com_Error( ERR_DROP, "Overlapping ET camera outputs" );
	}
	Frame frame;
	const auto status = CL_WolfETCameras().Sample( index, time, frame, diagnostic );
	if ( status == Result::Invalid ) Com_Error( ERR_DROP, "ET camera %d: %s", index, diagnostic.message );
	if ( status == Result::Frame ) {
		const float pitchYaw[2] = { frame.pitch, frame.yaw };
		std::memcpy( outputs[0], frame.origin.data(), sizes[0] );
		std::memcpy( outputs[1], pitchYaw, sizeof( pitchYaw ) );
		std::memcpy( outputs[2], &frame.fov, sizes[2] );
		result = qtrue;
	}
	/* The file supplies only typed numeric fade parameters, never executable
	 * text. Dispatch after evaluation so a module callback cannot invalidate a
	 * path while the evaluator still holds references into it. */
	for ( std::size_t i = 0; i < frame.fadeCount; ++i ) {
		char command[96];
		Com_sprintf( command, sizeof( command ), "fade 0 0 0 %d %f\n", frame.fades[i].alpha, frame.fades[i].seconds );
		Cbuf_ExecuteText( EXEC_NOW, command );
	}
	return true;
}
#endif
