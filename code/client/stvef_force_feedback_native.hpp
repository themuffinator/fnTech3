/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_FORCE_FEEDBACK_NATIVE_HPP
#define FNTECH3_STVEF_FORCE_FEEDBACK_NATIVE_HPP

#include "../compat/stvef_force_feedback.hpp"
#include <cstddef>
#include <cstring>
#include <string>

namespace fntech3::stvef::feedback {

/* Independently declared CFeel 1.0 interoperability boundary, observed in
 * stvoy.exe 1.20 and its FFC10.dll 1.0.11 imports. No SDK implementation or
 * proprietary runtime is included. Native constructors own their internals;
 * normal destructors release them before we unload the installed runtime. */
#if defined(_WIN32) && ( defined(_M_IX86) || defined(__i386__) )
#define FN_FEEL_NATIVE 1
#define FN_FEEL_CALL __thiscall
#else
#define FN_FEEL_NATIVE 0
#define FN_FEEL_CALL
#endif

struct Guid {
	std::uint32_t first;
	std::uint16_t second, third;
	std::uint8_t rest[8];
};
struct Point { std::int32_t x, y; };
static_assert( sizeof( Guid ) == 16 && sizeof( Point ) == 8 );

class Native {
public:
	Native() = default;
	Native( const Native & ) = delete;
	Native &operator=( const Native & ) = delete;
	~Native() { Close(); }

	bool Open( const char *path, void *instance, void *window, bool mouseFirst, int tension ) {
		Close();
		error_.clear();
#if FN_FEEL_NATIVE
		if ( !path || !*path || !instance || !window ) {
			error_ = "force feedback needs an installation path and a live window";
			return false;
		}
		library_ = Sys_LoadLibrary( path );
		if ( !library_ ) {
			error_ = "the installation's FFC10.dll could not be loaded";
			return false;
		}
		if ( !Load( mouseCtor_, "??0CFeelMouse@@QAE@XZ" ) ||
			!Load( mouseDtor_, "??1CFeelMouse@@UAE@XZ" ) ||
			!Load( mouseInit_, "?Initialize@CFeelMouse@@QAEHPAX0K@Z" ) ||
			!Load( mouseServices_, "?UsesWin32MouseServices@CFeelDevice@@QAEHH@Z" ) ||
			!Load( joyCtor_, "??0CFeelDXDevice@@QAE@XZ" ) ||
			!Load( joyDtor_, "??1CFeelDXDevice@@UAE@XZ" ) ||
			!Load( joyInit_, "?Initialize@CFeelDXDevice@@QAEHPAX0PAUIDirectInputA@@PAUIDirectInputDevice2A@@@Z" ) ||
			!Load( constantCtor_, "??0CFeelConstant@@QAE@XZ" ) ||
			!Load( constantDtor_, "??1CFeelConstant@@UAE@XZ" ) ||
			!Load( constantInit_, "?InitializePolar@CFeelConstant@@UAEHPAVCFeelDevice@@JKJPAUFEELIT_ENVELOPE@@@Z" ) ||
			!Load( periodicCtor_, "??0CFeelPeriodic@@QAE@ABU_GUID@@@Z" ) ||
			!Load( periodicDtor_, "??1CFeelPeriodic@@UAE@XZ" ) ||
			!Load( periodicInit_, "?InitializePolar@CFeelPeriodic@@UAEHPAVCFeelDevice@@KKKJJKPAUFEELIT_ENVELOPE@@@Z" ) ||
			!Load( start_, "?Start@CFeelEffect@@UAEHKK@Z" ) ||
			!Load( stop_, "?Stop@CFeelEffect@@UAEHXZ" ) ||
			!Load( status_, "?GetStatus@CFeelEffect@@QAEHPAK@Z" ) ||
			!Load( springCtor_, "??0CFeelSpring@@QAE@XZ" ) ||
			!Load( springDtor_, "??1CFeelSpring@@UAE@XZ" ) ||
			!Load( springInit_, "?Initialize@CFeelSpring@@UAEHPAVCFeelDevice@@JKKKUtagPOINT@@JJH@Z" ) ||
			!Load( springStart_, "?Start@CFeelCondition@@UAEHKK@Z" ) ||
			!Load( springChange_, "?ChangeParameters@CFeelSpring@@QAEHUtagPOINT@@JKKJJ@Z" ) ) {
			error_ = "FFC10.dll does not expose Elite Force's CFeel 1.0 interface";
			Close();
			return false;
		}
		if ( mouseFirst ) {
			mouseCtor_( device_.data );
			if ( mouseInit_( device_.data, instance, window, 4 ) ) deviceKind_ = 2;
			else mouseDtor_( device_.data );
		}
		if ( !deviceKind_ ) {
			device_ = {};
			joyCtor_( device_.data );
			if ( joyInit_( device_.data, instance, window, nullptr, nullptr ) ) deviceKind_ = 3;
			else joyDtor_( device_.data );
		}
		if ( !deviceKind_ ) {
			error_ = "no compatible force-feedback device initialized";
			Close();
			return false;
		}
		if ( deviceKind_ == 2 ) {
			mouseServices_( device_.data, 0 );
		} else {
			springCtor_( spring_.data );
			springReady_ = springInit_( spring_.data, device_.data, 2000, 10000, 1000, 3,
				{ 0x7fffffff, 0x7fffffff }, 1, 1, 1 ) != 0;
			if ( !springReady_ ) springDtor_( spring_.data );
			else Spring( Tension( tension ) );
		}
		return true;
#else
		(void)path; (void)instance; (void)window; (void)mouseFirst; (void)tension;
		error_ = "Elite Force's installed force-feedback runtime requires Windows x86";
		return false;
#endif
	}

	bool Available() const { return deviceKind_ != 0; }
	bool HasSpring() const { return springReady_; }
	bool IsMouse() const { return deviceKind_ == 2; }
	const char *Error() const { return error_.c_str(); }

	bool Build( int slot, int id, float gain ) {
		if ( !ValidSlot( slot ) || !ValidEffect( id ) || !Available() ) return false;
		Clear( slot );
		bool success = true;
		for ( int index = 0; index < ComponentCount; ++index ) {
			const auto &definition = Effects[id].components[index];
			if ( !definition.kind ) continue;
			auto &component = components_[slot][index];
			component.envelope = definition.envelope;
			auto *envelope = component.envelope.bytes ? &component.envelope : nullptr;
			const auto magnitude = Magnitude( definition.magnitude, gain );
			component.kind = definition.kind;
			if ( definition.kind == 1 ) {
				constantCtor_( component.object.data );
				if ( !constantInit_( component.object.data, device_.data, definition.angle,
					definition.duration, magnitude, envelope ) ) success = false;
			} else {
				component.guid = { 0x5959df00u + definition.kind, 0x2911, 0x11d1,
					{ 0xb0, 0x49, 0x00, 0x20, 0xaf, 0x30, 0x26, 0x9a } };
				periodicCtor_( component.object.data, component.guid );
				if ( !periodicInit_( component.object.data, device_.data, magnitude, definition.period,
					definition.duration, definition.angle, definition.offset, definition.phase, envelope ) ) success = false;
			}
		}
		if ( !success ) Clear( slot );
		return success;
	}
	bool Start( int slot ) {
		if ( !ValidSlot( slot ) || !Available() ) return false;
		bool success = true;
		for ( auto &component : components_[slot] ) {
			if ( component.kind && !start_( component.object.data, 1, 0 ) ) success = false;
		}
		return success;
	}
	bool Stop( int slot ) {
		if ( !ValidSlot( slot ) || !Available() ) return false;
		bool success = true;
		for ( auto &component : components_[slot] ) {
			if ( component.kind && !stop_( component.object.data ) ) success = false;
		}
		return success;
	}
	bool Playing( int slot ) {
		if ( !ValidSlot( slot ) || !Available() ) return false;
		for ( auto &component : components_[slot] ) {
			std::uint32_t status = 0;
			if ( component.kind && status_( component.object.data, &status ) && ( status & 1 ) ) return true;
		}
		return false;
	}
	void Clear( int slot ) {
		if ( !ValidSlot( slot ) ) return;
		for ( auto &component : components_[slot] ) {
			if ( component.kind == 1 ) constantDtor_( component.object.data );
			else if ( component.kind ) periodicDtor_( component.object.data );
			component = {};
		}
	}
	bool Spring( int value ) {
		if ( !Available() || !springReady_ ) return false;
		value = std::clamp( value, 0, 10000 );
		if ( !value ) {
			if ( springRunning_ && !stop_( spring_.data ) ) return false;
			springRunning_ = false;
			return true;
		}
		if ( !springRunning_ && !springStart_( spring_.data, 1, 0 ) ) return false;
		springRunning_ = true;
		constexpr auto keep = std::numeric_limits<std::int32_t>::min();
		return springChange_( spring_.data, { 0, 0 }, value, 0x80000000u, 0x80000000u, keep, keep ) != 0;
	}
	void Close() {
		if ( springReady_ ) {
			Spring( 0 );
			springDtor_( spring_.data );
		}
		springReady_ = springRunning_ = false;
		for ( int slot = 0; slot < SlotCount; ++slot ) { Stop( slot ); Clear( slot ); }
		if ( deviceKind_ == 2 ) mouseDtor_( device_.data );
		else if ( deviceKind_ ) joyDtor_( device_.data );
		deviceKind_ = 0;
		device_ = {};
		spring_ = {};
#if FN_FEEL_NATIVE
		if ( library_ ) Sys_UnloadLibrary( library_ );
#endif
		library_ = nullptr;
	}

private:
	static bool ValidSlot( int slot ) { return slot >= 0 && slot < SlotCount; }
	template <std::size_t Bytes> struct alignas( std::max_align_t ) Storage { unsigned char data[Bytes]{}; };
	struct NativeComponent {
		Storage<144> object;
		Guid guid{};
		Envelope envelope{};
		unsigned kind = 0;
	};
	template <typename T> bool Load( T &function, const char *name ) {
#if FN_FEEL_NATIVE
		function = reinterpret_cast<T>( Sys_LoadFunction( library_, name ) );
		return function != nullptr;
#else
		(void)function; (void)name; return false;
#endif
	}
	using Constructor = void *(FN_FEEL_CALL *)( void * );
	using Destructor = void (FN_FEEL_CALL *)( void * );
	Constructor mouseCtor_ = nullptr, joyCtor_ = nullptr, constantCtor_ = nullptr, springCtor_ = nullptr;
	Destructor mouseDtor_ = nullptr, joyDtor_ = nullptr, constantDtor_ = nullptr,
		periodicDtor_ = nullptr, springDtor_ = nullptr;
	void *(FN_FEEL_CALL *periodicCtor_)( void *, const Guid & ) = nullptr;
	int (FN_FEEL_CALL *mouseInit_)( void *, void *, void *, std::uint32_t ) = nullptr;
	int (FN_FEEL_CALL *joyInit_)( void *, void *, void *, void *, void * ) = nullptr;
	int (FN_FEEL_CALL *mouseServices_)( void *, int ) = nullptr;
	int (FN_FEEL_CALL *constantInit_)( void *, void *, std::int32_t, std::uint32_t, std::int32_t, Envelope * ) = nullptr;
	int (FN_FEEL_CALL *periodicInit_)( void *, void *, std::uint32_t, std::uint32_t, std::uint32_t,
		std::int32_t, std::int32_t, std::uint32_t, Envelope * ) = nullptr;
	int (FN_FEEL_CALL *start_)( void *, std::uint32_t, std::uint32_t ) = nullptr;
	int (FN_FEEL_CALL *stop_)( void * ) = nullptr;
	int (FN_FEEL_CALL *status_)( void *, std::uint32_t * ) = nullptr;
	int (FN_FEEL_CALL *springInit_)( void *, void *, std::int32_t, std::uint32_t, std::uint32_t,
		std::uint32_t, Point, std::int32_t, std::int32_t, int ) = nullptr;
	int (FN_FEEL_CALL *springStart_)( void *, std::uint32_t, std::uint32_t ) = nullptr;
	int (FN_FEEL_CALL *springChange_)( void *, Point, std::int32_t, std::uint32_t, std::uint32_t,
		std::int32_t, std::int32_t ) = nullptr;
	Storage<48> device_; // CFeelMouse uses 40; CFeelDXDevice uses 48.
	Storage<188> spring_;
	NativeComponent components_[SlotCount][ComponentCount]{};
	void *library_ = nullptr;
	int deviceKind_ = 0;
	bool springReady_ = false, springRunning_ = false;
	std::string error_;
};

#undef FN_FEEL_CALL
#undef FN_FEEL_NATIVE
} // namespace fntech3::stvef::feedback
#endif
