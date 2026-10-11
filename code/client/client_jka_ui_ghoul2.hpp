/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_UI_GHOUL2_HPP
#define FNTECH3_CLIENT_JKA_UI_GHOUL2_HPP

#include "../ghoul2/ghoul2_server.hpp"
#include "../compat/jka_ghoul2_clock.hpp"
#include <array>

/* UI-only ownership and metadata, independently implemented from retail
 * jamp.exe UI dispatch (208:0x424d8a,211:0x424e08,204:0x424c6c,
 * 220:0x424f7b,216:0x424eea,233:0x4250b4) and OpenJK pin
 * 1a6a643427aa347553e9073dac5570b33337c4d9 UI/G2 contract observations.
 * Native modules keep an opaque four-byte identity, never an internal object
 * address. The companion UI renderer consumes only its measured, bounded
 * owned model subset; this metadata layer never submits module pointers. */
namespace {

static bool CL_JKAUIAssetPath( const char *path ) {
	if ( !path || !*path || std::strlen( path ) >= MAX_QPATH || *path == '/' || *path == '\\' ) return false;
	const char *component = path;
	for ( const char *at = path; ; ++at ) {
		if ( *at == ':' || ( *at && static_cast<unsigned char>( *at ) < 32 ) ) return false;
		if ( !*at || *at == '/' || *at == '\\' ) {
			const auto count = at - component;
			if ( count == 0 || ( count == 1 && *component == '.' ) ||
				( count == 2 && component[0] == '.' && component[1] == '.' ) ) return false;
			if ( !*at ) return true;
			component = at + 1;
		}
	}
}

class ClientUIGhoul2Files final : public fntech3::ghoul2::Files {
public:
	std::size_t Read( const char *path, void *destination, std::size_t capacity ) override {
		if ( !CL_JKAUIAssetPath( path ) ) return 0;
		fileHandle_t handle = 0;
		const int length = FS_FOpenFileRead( path, &handle, qfalse );
		if ( length <= 0 || !handle ) {
			if ( handle ) FS_FCloseFile( handle );
			return 0;
		}
		const auto bytes = std::size_t( length );
		int read = length;
		if ( destination && capacity >= bytes && bytes <= fntech3::ghoul2::MaxModelBytes )
			read = FS_Read( destination, length, handle );
		FS_FCloseFile( handle );
		return read == length ? bytes : 0;
	}
};

ClientUIGhoul2Files jkaUIGhoul2Files;
std::unique_ptr<fntech3::ghoul2::Store> jkaUIGhoul2Store;
static int CL_JKAUIGhoul2Time() {
	return fntech3::jka::clock::shared.Time();
}
static fntech3::ghoul2::Store &CL_JKAUIGhoul2Store() {
	if ( !jkaUIGhoul2Store ) jkaUIGhoul2Store = std::make_unique<fntech3::ghoul2::Store>( jkaUIGhoul2Files );
	return *jkaUIGhoul2Store;
}

static void CL_JKAUIGhoul2Native() {
	if ( sizeof( void * ) != sizeof( std::uint32_t ) || !uivm || !uivm->entryPoint || uivm->dllExports )
		Com_Error( ERR_DROP, "Jedi Academy UI Ghoul2 requires its x86 legacy native module" );
}

static fntech3::ghoul2::Instance *CL_JKAUIGhoul2Resolve( std::uint32_t identity ) {
	if ( !identity ) return nullptr;
	auto *instance = CL_JKAUIGhoul2Store().Resolve( reinterpret_cast<void *>( std::uintptr_t( identity ) ) );
	if ( !instance ) Com_Error( ERR_DROP, "Jedi Academy UI Ghoul2 received a stale or foreign model identity" );
	return instance;
}

static void CL_JKAUIGhoul2Name( char *output, const std::string &name ) {
	if ( name.size() >= MAX_QPATH ) Com_Error( ERR_DROP, "Jedi Academy UI Ghoul2 metadata name exceeds MAX_QPATH" );
	std::memcpy( output, name.c_str(), name.size() + 1 );
}

static std::array<char, MAX_QPATH> CL_JKAUIGhoul2BoneName(intptr_t address) {
	std::array<char, MAX_QPATH> name{};
	for (std::size_t n = 0; n < name.size(); ++n) {
		if (std::uintptr_t(address) > (std::numeric_limits<std::uintptr_t>::max)() - n)
			Com_Error(ERR_DROP, "Invalid Jedi Academy UI Ghoul2 bone-name address");
		name[n] = *static_cast<const char *>(CL_JKAUINativeSpan(std::uintptr_t(address) + n, 1));
		if (!name[n]) return name;
	}
	Com_Error(ERR_DROP, "Jedi Academy UI Ghoul2 bone name exceeds MAX_QPATH");
	return name;
}

static std::string CL_JKAUIGhoul2SurfaceName( const fntech3::ghoul2::Model &model, int number ) {
	if ( number < 0 || number >= model.mesh.numSurfaces ) return {};
	if ( std::size_t( number ) < model.surfaceNames.size() ) return model.surfaceNames[std::size_t( number )];
	// Inline *default skeleton meshes do not retain server-side file bytes.
	// Obtain their real surface record through the same bounded client reader.
	const auto size = jkaUIGhoul2Files.Read( model.name.c_str(), nullptr, 0 );
	if ( !size || size > fntech3::ghoul2::MaxModelBytes )
		Com_Error( ERR_DROP, "Jedi Academy UI Ghoul2 surface metadata file is unavailable" );
	std::vector<unsigned char> bytes( size );
	if ( jkaUIGhoul2Files.Read( model.name.c_str(), bytes.data(), size ) != size )
		Com_Error( ERR_DROP, "Jedi Academy UI Ghoul2 surface metadata read failed" );
	fntech3::jka::model::Mesh mesh;
	char name[fntech3::jka::model::NameBytes + 1]{};
	if ( fntech3::jka::model::ParseMesh( bytes.data(), size, model.mesh.numBones, mesh ) != fntech3::jka::model::Error::None ||
		!fntech3::jka::model::SurfaceName( bytes.data(), size, mesh, number, name, sizeof( name ) ) )
		Com_Error( ERR_DROP, "Invalid Jedi Academy UI Ghoul2 surface metadata" );
	return name;
}

} // namespace

static void CL_JKAUIGhoul2Clear() {
	// Destroy the store as well as instances, releasing cached files on every
	// module/search-path lifetime. No retired module slot is followed here.
	jkaUIGhoul2Store.reset();
}

static bool CL_JKAUIGhoul2Call( intptr_t *args, fntech3::jka::module::Call call, intptr_t &result ) {
	using fntech3::jka::module::Call;
	switch ( call ) {
	case Call::UI_G2_GETTIME:
		CL_JKAUIGhoul2Native();
		result = CL_JKAUIGhoul2Time();
		return true;
	case Call::UI_G2_SETTIME: {
		CL_JKAUIGhoul2Native();
		if (args[2] < 0 || args[2] > 1)
			Com_Error(ERR_DROP, "Invalid Jedi Academy UI Ghoul2 clock selector");
		fntech3::jka::clock::shared.Set(static_cast<int>(args[2]), static_cast<std::int32_t>(args[1]));
		result = 0;
		return true;
	}
	case Call::UI_G2_INITGHOUL2MODEL: {
		CL_JKAUIGhoul2Native();
		for ( int argument = 3; argument <= 7; ++argument ) if ( args[argument] )
			Com_Error( ERR_DROP, "Jedi Academy UI Ghoul2 nonzero model index, skin, shader, flags or LOD selector is not served yet" );
		void *slot = CL_JKAUINativeSpan( args[1], sizeof( std::uint32_t ) );
		std::uint32_t identity = 0;
		std::memcpy( &identity, slot, sizeof( identity ) );
		CL_JKAUIGhoul2Resolve( identity );
		std::array<char, MAX_QPATH> filename{};
		bool terminated = false;
		for ( std::size_t n = 0; n < filename.size(); ++n ) {
			if ( std::uintptr_t( args[2] ) > ( std::numeric_limits<std::uintptr_t>::max )() - n )
				Com_Error( ERR_DROP, "Invalid Jedi Academy UI Ghoul2 filename address" );
			filename[n] = *static_cast<const char *>( CL_JKAUINativeSpan( std::uintptr_t( args[2] ) + n, 1 ) );
			if ( !filename[n] ) { terminated = true; break; }
		}
		if ( !terminated ) Com_Error( ERR_DROP, "Jedi Academy UI Ghoul2 filename exceeds MAX_QPATH" );
		auto &store = CL_JKAUIGhoul2Store();
		if ( !identity && store.InstanceCount() >= 1024 ) { result = -1; return true; }
		void *handle = reinterpret_cast<void *>( std::uintptr_t( identity ) );
		fntech3::ghoul2::LoadError error;
		result = store.AddModel( &handle, filename.data(), error );
		if ( result >= 0 ) {
			identity = static_cast<std::uint32_t>( reinterpret_cast<std::uintptr_t>( handle ) );
			std::memcpy( slot, &identity, sizeof( identity ) );
		}
		return true;
	}
	case Call::UI_G2_CLEANMODELS: {
		CL_JKAUIGhoul2Native();
		void *slot = CL_JKAUINativeSpan( args[1], sizeof( std::uint32_t ) );
		std::uint32_t identity = 0;
		std::memcpy( &identity, slot, sizeof( identity ) );
		CL_JKAUIGhoul2Resolve( identity );
		void *handle = reinterpret_cast<void *>( std::uintptr_t( identity ) );
		if ( !CL_JKAUIGhoul2Store().Clean( &handle ) ) Com_Error( ERR_DROP, "Jedi Academy UI Ghoul2 cleanup failed" );
		identity = 0;
		std::memcpy( slot, &identity, sizeof( identity ) );
		result = 0;
		return true;
	}
	case Call::UI_G2_ANGLEOVERRIDE: {
		CL_JKAUIGhoul2Native();
		const auto identity = static_cast<std::uint32_t>(args[1]);
		if (!CL_JKAUIGhoul2Resolve(identity)) { result = qfalse; return true; }
		if (args[9]) Com_Error(ERR_DROP, "Jedi Academy UI Ghoul2 external bone-angle model lists are not served yet");
		const auto bone = CL_JKAUIGhoul2BoneName(args[3]);
		std::array<float, 3> angles;
		std::memcpy(angles.data(), CL_JKAUINativeSpan(args[4], sizeof(angles)), sizeof(angles));
		// Retail UI 212 at0x424dc0 ->0x4df460 transfers eleven words. The
		// scalar model-list argument is not traversed by this owned UI path.
		result = CL_JKAUIGhoul2Store().SetBoneAngles(reinterpret_cast<void *>(std::uintptr_t(identity)),
			static_cast<int>(args[2]), bone.data(), angles.data(), static_cast<int>(args[5]),
			static_cast<int>(args[6]), static_cast<int>(args[7]), static_cast<int>(args[8]),
			static_cast<int>(args[10]), static_cast<int>(args[11])) ? qtrue : qfalse;
		return true;
	}
	case Call::UI_G2_PLAYANIM: {
		CL_JKAUIGhoul2Native();
		const auto identity = static_cast<std::uint32_t>( args[1] );
		if ( !CL_JKAUIGhoul2Resolve( identity ) ) { result = qfalse; return true; }
		const auto bone = CL_JKAUIGhoul2BoneName(args[3]);
		// Retail UI dispatch at 0x424e20 preserves words 7 and 9 as floats.
		// SetBoneAnim owns timing metadata; unsupported pose modes still fail
		// in the shared evaluator before the companion submits a posed model.
		const std::uint32_t speedBits = static_cast<std::uint32_t>( args[7] );
		const std::uint32_t seedBits = static_cast<std::uint32_t>( args[9] );
		float speed, seed;
		std::memcpy( &speed, &speedBits, sizeof( speed ) );
		std::memcpy( &seed, &seedBits, sizeof( seed ) );
		result = CL_JKAUIGhoul2Store().SetBoneAnim( reinterpret_cast<void *>( std::uintptr_t( identity ) ),
			static_cast<int>( args[2] ), bone.data(), static_cast<int>( args[4] ), static_cast<int>( args[5] ),
			static_cast<int>( args[6] ), speed, static_cast<int>( args[8] ), seed,
			static_cast<int>( args[10] ) ) ? qtrue : qfalse;
		return true;
	}
	case Call::UI_G2_HAVEWEGHOULMODELS:
	case Call::UI_G2_HASGHOUL2MODELONINDEX: {
		CL_JKAUIGhoul2Native();
		std::uint32_t identity = static_cast<std::uint32_t>(args[1]);
		// Retail UI 220:0x424f7b ->0x4dec30 reads the identity through
		// args1. Have 204 passes the identity directly to0x4dfc80.
		if (call == Call::UI_G2_HASGHOUL2MODELONINDEX)
			std::memcpy(&identity, CL_JKAUINativeSpan(args[1], sizeof(identity)), sizeof(identity));
		const auto *instance = CL_JKAUIGhoul2Resolve(identity);
		result = instance && ( call == Call::UI_G2_HAVEWEGHOULMODELS ? instance->HoldsAny() :
			instance->At( static_cast<int>( args[2] ) ) != nullptr ) ? qtrue : qfalse;
		return true;
	}
	case Call::UI_G2_GETGLANAME:
	case Call::UI_G2_GETSURFACENAME: {
		CL_JKAUIGhoul2Native();
		const bool gla = call == Call::UI_G2_GETGLANAME;
		const auto *instance = CL_JKAUIGhoul2Resolve( static_cast<std::uint32_t>( args[1] ) );
		if (!instance) { result = 0; return true; }
		const auto *model = instance->At(static_cast<int>(args[gla ? 2 : 3]));
		// Retail UI 216:0x424eff branches before copying a null GLA name.
		if (gla && !model) { result = 0; return true; }
		auto *output = static_cast<char *>(CL_JKAUINativeSpan(args[gla ? 3 : 4], MAX_QPATH));
		std::string name;
		if ( model ) {
			if ( gla ) {
				name = model->skeletonName;
				// G2 returns the GLM's raw animName, without the extension the
				// asset store added while opening its paired skeleton file.
				if ( !name.empty() && name[0] != '*' && name.size() >= 4 ) name.resize( name.size() - 4 );
			} else name = CL_JKAUIGhoul2SurfaceName( *model, static_cast<int>( args[2] ) );
		}
		CL_JKAUIGhoul2Name( output, name );
		result = 0;
		return true;
	}
	default: return false;
	}
}

#endif
