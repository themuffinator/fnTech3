CMAKE_MINIMUM_REQUIRED(VERSION 3.12)

# This must be initialized before PROJECT() enables a compiler.  APPLE is a
# target-system result and is not reliably available this early; the variable
# is harmless on non-Apple targets, where CMake ignores it.
IF(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
	SET(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum supported macOS version")
ENDIF()

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/version/fntech3_version.h" FNTECH3_VERSION_STRING_LINE REGEX "^#define FNTECH3_VERSION_STRING \"[^\"]+\"$")
string(REGEX REPLACE "^#define FNTECH3_VERSION_STRING \"([^\"]+)\"$" "\\1" FNTECH3_VERSION_STRING "${FNTECH3_VERSION_STRING_LINE}")

PROJECT(fnTech3 VERSION ${FNTECH3_VERSION_STRING} LANGUAGES C CXX)

# Retail Quake Live's native modules and WebUI are 32-bit x86 ABI boundaries.
# Probe compiler-defined target macros as well as pointer width so a misleading
# host processor string cannot allow an unsupported artifact through. Use a
# direct try_compile rather than CheckCSourceCompiles: CMAKE_REQUIRED_FLAGS are
# check-only flags and must not be able to select a different architecture from
# the flags inherited by real FnQL targets. The variable remains untouched for
# later feature checks, while try_compile still inherits the configured compiler,
# toolchain file, sysroot, and normal C/C++ flags used by production targets.
SET(FNQL_X86_TARGET_PROBE_SOURCE
	"${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/fnql_x86_target_probe.c")
FILE(WRITE "${FNQL_X86_TARGET_PROBE_SOURCE}" [=[
#if !defined(_M_IX86) && !defined(__i386__)
#error FnQL requires an x86 target
#endif
typedef char fnql_pointer_width_must_be_32_bits[(sizeof(void *) == 4) ? 1 : -1];
int main(void) { return 0; }
]=])
UNSET(FNQL_TARGET_IS_32BIT_X86 CACHE)
UNSET(FNQL_TARGET_IS_32BIT_X86)
TRY_COMPILE(FNQL_TARGET_IS_32BIT_X86
	"${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/fnql_x86_target_probe"
	"${FNQL_X86_TARGET_PROBE_SOURCE}"
	OUTPUT_VARIABLE FNQL_X86_TARGET_PROBE_OUTPUT)
IF(NOT FNQL_TARGET_IS_32BIT_X86)
	MESSAGE(FATAL_ERROR
		"fnTech3 shared Q3/QL builds support only 32-bit x86 targets. Select Win32 on Windows or "
		"configure an i686 toolchain (for example, -m32) on other platforms.")
ENDIF()

SET(FNQL_MACOS_SHORT_VERSION
	"${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
IF(PROJECT_VERSION_TWEAK GREATER 0)
	SET(FNQL_MACOS_BUNDLE_VERSION "${PROJECT_VERSION_TWEAK}")
ELSE()
	SET(FNQL_MACOS_BUNDLE_VERSION "1")
ENDIF()

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_EXTENSIONS OFF)

IF(MSVC)
	# Standard-library templates use exception-aware cleanup even though the
	# engine does not throw across its C ABI boundaries.
	ADD_COMPILE_OPTIONS(
		$<$<COMPILE_LANGUAGE:CXX>:/EHsc>
		$<$<COMPILE_LANGUAGE:C>:/wd4201>
		$<$<COMPILE_LANGUAGE:CXX>:/wd4201>)
ENDIF()

SET(CNAME "fntech3")
SET(DNAME "fntech3.ded")

# Keep the renderer loader ABI names independent of the engine identity.
SET(RENDERER_PREFIX "fnql")
SET(RENDERER_DEFAULT glx CACHE STRING "Default renderer: glx, vk, or rtx")
set_property(CACHE RENDERER_DEFAULT PROPERTY STRINGS glx vk rtx)
set(FNQL_CMAKE_RENDERERS glx vk rtx)
list(FIND FNQL_CMAKE_RENDERERS "${RENDERER_DEFAULT}" FNQL_RENDERER_DEFAULT_INDEX)
if(FNQL_RENDERER_DEFAULT_INDEX EQUAL -1)
	message(FATAL_ERROR "Unsupported RENDERER_DEFAULT='${RENDERER_DEFAULT}'; choose glx, vk, or rtx")
endif()

OPTION(USE_SDL "" ON)
OPTION(USE_CURL "" ON)
IF(WIN32)
	OPTION(USE_CURL_DLOPEN "Resolve libcurl at runtime instead of linking it" OFF)
ELSE()
	OPTION(USE_CURL_DLOPEN "Resolve libcurl at runtime instead of linking it" ON)
ENDIF()
OPTION(USE_OGG_VORBIS "Enable Ogg/Vorbis audio through a system vorbisfile dependency" ON)
OPTION(USE_VK "Build the Vulkan renderer module" ON)
OPTION(USE_RTX "Build the ray-tracing renderer module" ON)
OPTION(USE_GLX "Build the GLx OpenGL-lineage renderer module" ON)
OPTION(USE_SYSTEM_JPEG "Use a system libjpeg instead of the Meson-fetched libjpeg-turbo source tree" OFF)
SET(USE_FREETYPE AUTO CACHE STRING "Rasterize TrueType fonts with FreeType: AUTO, ON or OFF")
set_property(CACHE USE_FREETYPE PROPERTY STRINGS AUTO ON OFF)
OPTION(USE_RENDERER_DLOPEN "" ON)
OPTION(FNQL_AUDIO_LOOPBACK_TESTS "Build deterministic audio runtime and OpenAL loopback tests" ON)
OPTION(FNQL_GLX_LOGIC_TESTS "Build deterministic GLx renderer logic tests" ON)

# Large-file support, as the Makefile and Meson define it: without it 32-bit
# glibc's readdir() and stat() fail with EOVERFLOW on inode numbers past 32
# bits and stat() on files of 2 GiB or more. macOS's off_t and ino_t are
# already 64-bit, and Windows lists files through its own 64-bit calls.
IF(UNIX AND NOT APPLE)
	ADD_COMPILE_DEFINITIONS(_FILE_OFFSET_BITS=64)
ENDIF()

IF(APPLE)
	ADD_COMPILE_DEFINITIONS(MACOS_X)
	IF(NOT USE_SDL)
		MESSAGE(FATAL_ERROR "macOS client builds require SDL3; configure with -DUSE_SDL=ON")
	ENDIF()
ENDIF()

IF(RENDERER_DEFAULT STREQUAL "glx")
	SET(USE_GLX ON CACHE BOOL "Build the GLx OpenGL-lineage renderer module" FORCE)
ENDIF()

IF(RENDERER_DEFAULT STREQUAL "vk")
	SET(USE_VK ON CACHE BOOL "Build the Vulkan renderer module" FORCE)
ENDIF()

IF(RENDERER_DEFAULT STREQUAL "rtx")
	SET(USE_RTX ON CACHE BOOL "Build the ray-tracing renderer module" FORCE)
ENDIF()

IF(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	# win_resource.rc already embeds q3.manifest; CMake's Windows-Clang rule
	# can add a generated manifest that lld-link rejects as a duplicate resource.
	FOREACH(lang C CXX)
		STRING(REPLACE " -Xlinker /MANIFEST:EMBED" "" CMAKE_${lang}_LINK_EXECUTABLE "${CMAKE_${lang}_LINK_EXECUTABLE}")
		STRING(REPLACE " /MANIFEST:NO" "" CMAKE_${lang}_LINK_EXECUTABLE "${CMAKE_${lang}_LINK_EXECUTABLE}")
	ENDFOREACH()
	IF(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
		SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
	ENDIF()
ENDIF()

IF(NOT USE_RENDERER_DLOPEN)
	IF(NOT RENDERER_DEFAULT STREQUAL "vk")
		SET(USE_VK OFF)
	ENDIF()
	IF(NOT RENDERER_DEFAULT STREQUAL "rtx")
		SET(USE_RTX OFF)
	ENDIF()
ENDIF()

AUX_SOURCE_DIRECTORY(code/qcommon QCOMMON_SRCS)
list(APPEND QCOMMON_SRCS
	code/compat/game_abi_adapter.c
	code/platform/fnql_steam.cpp
	code/platform/fnql_workshop.cpp
	code/platform/process_relaunch.cpp)
# exclude platform-dependent QVM bytecode compilers
list(FILTER QCOMMON_SRCS EXCLUDE REGEX ".*vm_(aarch64|armv7l|powerpc|x86)\\.c$")
# The target probe above makes these ABI-sensitive names and sources fixed.
SET(BINEXT "")
SET(RENDEXT _x86)
list(APPEND QCOMMON_SRCS code/qcommon/vm_x86.c)
SET(PLATFORM_ASM_SRCS)
SET(CLIENT_ASM_SRCS)

# botlib
AUX_SOURCE_DIRECTORY(code/botlib BOTLIB_SRCS)
ADD_LIBRARY(botlib OBJECT ${BOTLIB_SRCS})
TARGET_COMPILE_DEFINITIONS(botlib PRIVATE BOTLIB)

# Medal of Honor's TIKI, skeletor and alias runtime keeps OpenMoHAA's upstream
# names and headers (see code/mohaa/README.md). Its sources include "q_shared.h"
# and "qcommon.h" by bare name, so code/mohaa/qcommon leads the include path.
# The client and the dedicated server link the same objects, as meson does.
SET(MOHAA_SRCS
	code/mohaa/corepp/con_set.cpp
	code/mohaa/corepp/mem_blockalloc.cpp
	code/mohaa/corepp/mem_tempalloc.cpp
	code/mohaa/corepp/str.cpp
	code/mohaa/corepp/tiki_main.cpp
	code/mohaa/corepp/tiki_script.cpp
	code/mohaa/qcommon/alias.c
	code/mohaa/qcommon/mohaa_q_shared.c
	code/mohaa/skeletor/bonetable.cpp
	code/mohaa/skeletor/skeletor.cpp
	code/mohaa/skeletor/skeletor_imports.cpp
	code/mohaa/skeletor/skeletor_loadanimation.cpp
	code/mohaa/skeletor/skeletor_model_files.cpp
	code/mohaa/skeletor/skeletor_utilities.cpp
	code/mohaa/skeletor/skeletorbones.cpp
	code/mohaa/skeletor/tokenizer.cpp
	code/mohaa/tiki/tiki_anim.cpp
	code/mohaa/tiki/tiki_cache.cpp
	code/mohaa/tiki/tiki_commands.cpp
	code/mohaa/tiki/tiki_files.cpp
	code/mohaa/tiki/tiki_frame.cpp
	code/mohaa/tiki/tiki_imports.cpp
	code/mohaa/tiki/tiki_mesh.cpp
	code/mohaa/tiki/tiki_parse.cpp
	code/mohaa/tiki/tiki_skel.cpp
	code/mohaa/tiki/tiki_surface.cpp
	code/mohaa/tiki/tiki_tag.cpp
	code/mohaa/tiki/tiki_utility.cpp
	code/mohaa/mohaa_engine.cpp
	code/mohaa/mohaa_system86.cpp
	code/mohaa/mohaa_render_api.cpp
	code/mohaa/mohaa_tiki_api.cpp)
ADD_LIBRARY(mohaa_tiki OBJECT ${MOHAA_SRCS})
TARGET_INCLUDE_DIRECTORIES(mohaa_tiki PRIVATE code/mohaa/qcommon code)
IF(MSVC)
	TARGET_COMPILE_OPTIONS(mohaa_tiki PRIVATE /wd4018 /wd4101 /wd4189 /wd4305 /wd4389
		/wd4456 /wd4457 /wd4458 /wd4459 /wd4701)
ELSE()
	# Upstream type-puns floats (SkelQuat's Slerp), so it is built without
	# strict aliasing, as meson builds every source.
	TARGET_COMPILE_OPTIONS(mohaa_tiki PRIVATE -fno-strict-aliasing
		-Wno-unknown-warning-option -Wno-sign-compare
		-Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function
		-Wno-unused-value -Wno-maybe-uninitialized -Wno-parentheses
		-Wno-format-truncation -Wno-stringop-truncation -Wno-char-subscripts
		$<$<COMPILE_LANGUAGE:C>:-Wno-pointer-sign>
		$<$<COMPILE_LANGUAGE:CXX>:-Wno-class-memaccess>
		$<$<COMPILE_LANGUAGE:CXX>:-Wno-reorder>)
ENDIF()

SET(SERVER_SRCS
	code/server/factory_catalog.cpp
	code/server/factory_rotation.cpp
	code/server/server_game_data.cpp
	code/server/sv_bot.cpp
	code/server/sv_ccmds.cpp
	code/server/sv_client.cpp
	code/server/sv_filter.cpp
	code/server/sv_game.cpp
	code/server/sv_init.cpp
	code/server/sv_main.cpp
	code/server/sv_net_chan.cpp
	code/server/sv_platform.cpp
	code/server/sv_factory.cpp
	code/server/sv_snapshot.cpp
	code/server/sv_world.cpp
	code/server/sv_zmq.cpp
)

# universal binary
ADD_LIBRARY(qcommon OBJECT ${QCOMMON_SRCS} ${SERVER_SRCS} ${PLATFORM_ASM_SRCS})
TARGET_LINK_LIBRARIES(qcommon PRIVATE Threads::Threads ZLIB::ZLIB)

# dedicated server
ADD_LIBRARY(qcommon_ded OBJECT ${QCOMMON_SRCS} ${SERVER_SRCS} ${PLATFORM_ASM_SRCS})
TARGET_COMPILE_DEFINITIONS(qcommon_ded PUBLIC DEDICATED)
TARGET_LINK_LIBRARIES(qcommon_ded PRIVATE Threads::Threads ZLIB::ZLIB)

IF(NO_VM_COMPILED)
	TARGET_COMPILE_DEFINITIONS(qcommon PRIVATE NO_VM_COMPILED)
	TARGET_COMPILE_DEFINITIONS(qcommon_ded PRIVATE NO_VM_COMPILED)
ENDIF()

# client + cURL
AUX_SOURCE_DIRECTORY(code/client CLIENT_SRCS)
list(APPEND CLIENT_SRCS
	code/client/audio/snd_main.cpp
	code/client/audio/legacy/snd_adpcm.cpp
	code/client/audio/legacy/snd_dma.cpp
	code/client/audio/legacy/snd_mem.cpp
	code/client/audio/legacy/snd_mix.cpp
	code/client/audio/legacy/snd_wavelet.cpp
	code/client/audio/codecs/snd_codec.cpp
	code/client/audio/codecs/snd_codec_ogg.cpp
	code/client/audio/codecs/snd_codec_wav.cpp
	code/client/audio/snd_alice.cpp
	code/client/audio/openal/AudioSystem.cpp
	${CLIENT_ASM_SRCS})
# Elite Force's sounds and music and American McGee's Alice's music are MP3;
# minimp3 is header-only and compiled once in snd_codec_mp3.cpp.
set(FNQL_MINIMP3_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/subprojects/minimp3")
if(NOT EXISTS "${FNQL_MINIMP3_INCLUDE_DIR}/minimp3.h")
	message(FATAL_ERROR
		"Elite Force and Alice audio require the pinned minimp3 wrap. Run 'meson subprojects download minimp3' before configuring CMake.")
endif()
list(APPEND CLIENT_SRCS code/client/audio/codecs/snd_codec_mp3.cpp)
ADD_COMPILE_DEFINITIONS(USE_CODEC_MP3)
INCLUDE_DIRECTORIES(SYSTEM "${FNQL_MINIMP3_INCLUDE_DIR}")
# The entity editor's interface is Dear ImGui; its core is compiled into the
# client from the pinned wrap's checkout.
set(FNTECH3_IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/subprojects/imgui")
if(NOT EXISTS "${FNTECH3_IMGUI_DIR}/imgui.h")
	message(FATAL_ERROR
		"The entity editor requires the pinned Dear ImGui wrap. Run 'meson subprojects download imgui' before configuring CMake.")
endif()
list(APPEND CLIENT_SRCS
	${FNTECH3_IMGUI_DIR}/imgui.cpp
	${FNTECH3_IMGUI_DIR}/imgui_draw.cpp
	${FNTECH3_IMGUI_DIR}/imgui_tables.cpp
	${FNTECH3_IMGUI_DIR}/imgui_widgets.cpp
	${FNTECH3_IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp)
ADD_COMPILE_DEFINITIONS(FNTECH3_ENTITY_EDITOR_UI IMGUI_DISABLE_OBSOLETE_FUNCTIONS
	IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS
	IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS)
INCLUDE_DIRECTORIES(SYSTEM "${FNTECH3_IMGUI_DIR}" "${FNTECH3_IMGUI_DIR}/misc/cpp")
IF(NOT USE_CURL)
	list(REMOVE_ITEM CLIENT_SRCS code/client/cl_curl.cpp)
ENDIF()

#ADD_LIBRARY(client OBJECT ${CLIENT_SRCS})

set(FNQL_CLIENT_LINK_LIBS)
set(FNQL_CLIENT_COMPILE_DEFINITIONS)
set(FNQL_CLIENT_INCLUDE_DIRS)

# cURL.  Runtime loading is the Unix default, but headers are still required
# to compile the ABI declarations.  Missing optional system packages keep the
# historical build fallback (feature disabled) instead of breaking unrelated
# CMake configurations.
IF(USE_CURL)
	find_package(CURL QUIET)
	IF(CURL_FOUND)
		list(APPEND FNQL_CLIENT_COMPILE_DEFINITIONS USE_CURL)
		list(APPEND FNQL_CLIENT_INCLUDE_DIRS ${CURL_INCLUDE_DIRS})
		IF(USE_CURL_DLOPEN)
			list(APPEND FNQL_CLIENT_COMPILE_DEFINITIONS USE_CURL_DLOPEN)
		ELSEIF(TARGET CURL::libcurl)
			list(APPEND FNQL_CLIENT_LINK_LIBS CURL::libcurl)
		ELSE()
			list(APPEND FNQL_CLIENT_LINK_LIBS ${CURL_LIBRARIES})
		ENDIF()
	ELSE()
		message(WARNING "USE_CURL=ON but a system libcurl development package was not found; CMake client downloads are disabled")
		list(REMOVE_ITEM CLIENT_SRCS code/client/cl_curl.cpp)
	ENDIF()
ENDIF()

# Ogg/Vorbis.  Legacy Make builds may use system packages and Meson owns the
# pinned fallback graph; CMake likewise consumes the system dependency without
# introducing another bundled-source ownership path.
IF(USE_OGG_VORBIS)
	find_package(PkgConfig QUIET)
	IF(PkgConfig_FOUND)
		pkg_check_modules(VORBISFILE QUIET IMPORTED_TARGET vorbisfile)
	ENDIF()
	IF(TARGET PkgConfig::VORBISFILE)
		list(APPEND FNQL_CLIENT_COMPILE_DEFINITIONS USE_OGG_VORBIS)
		list(APPEND FNQL_CLIENT_INCLUDE_DIRS ${VORBISFILE_INCLUDE_DIRS})
		list(APPEND FNQL_CLIENT_LINK_LIBS PkgConfig::VORBISFILE)
	ELSE()
		message(WARNING "USE_OGG_VORBIS=ON but a system vorbisfile development package was not found; CMake Ogg audio is disabled")
		list(REMOVE_ITEM CLIENT_SRCS code/client/audio/codecs/snd_codec_ogg.cpp)
	ENDIF()
ELSE()
	list(REMOVE_ITEM CLIENT_SRCS code/client/audio/codecs/snd_codec_ogg.cpp)
ENDIF()

# jpeg
IF(USE_SYSTEM_JPEG)
	ADD_LIBRARY(client OBJECT ${CLIENT_SRCS})
	find_package(JPEG REQUIRED)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_SYSTEM_JPEG)
	TARGET_INCLUDE_DIRECTORIES(client PRIVATE ${JPEG_INCLUDE_DIRS})
	list(APPEND FNQL_CLIENT_LINK_LIBS ${JPEG_LIBRARIES})
ELSE()
	file(GLOB FNQL_LIBJPEG_TURBO_CANDIDATES CONFIGURE_DEPENDS
		"${CMAKE_CURRENT_SOURCE_DIR}/subprojects/libjpeg-turbo-*")
	foreach(FNQL_LIBJPEG_TURBO_CANDIDATE IN LISTS FNQL_LIBJPEG_TURBO_CANDIDATES)
		if(EXISTS "${FNQL_LIBJPEG_TURBO_CANDIDATE}/CMakeLists.txt"
				AND EXISTS "${FNQL_LIBJPEG_TURBO_CANDIDATE}/src/jpeglib.h")
			set(FNQL_LIBJPEG_TURBO_SOURCE_DIR "${FNQL_LIBJPEG_TURBO_CANDIDATE}")
		endif()
	endforeach()
	if(NOT FNQL_LIBJPEG_TURBO_SOURCE_DIR)
		message(FATAL_ERROR "USE_SYSTEM_JPEG=OFF requires subprojects/libjpeg-turbo-* from the Meson wrap, or configure with -DUSE_SYSTEM_JPEG=ON and a system libjpeg")
	endif()

	include(ExternalProject)
	set(FNQL_LIBJPEG_TURBO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/libjpeg-turbo")
	set(FNQL_LIBJPEG_TURBO_PLATFORM_ARGS)
	if(APPLE)
		list(APPEND FNQL_LIBJPEG_TURBO_PLATFORM_ARGS
			"-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}"
			"-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
			"-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}")
	endif()
	# The library is configured on its own and inherits none of this
	# project's target selection. Hand it the inputs that choose the target:
	# a Linux i686 build that passes -m32 in CMAKE_C_FLAGS (as Linux CI does)
	# or in CC (CMAKE_C_COMPILER_ARG1) otherwise gets a host x86-64 libjpeg.a
	# that the client cannot link.
	list(APPEND FNQL_LIBJPEG_TURBO_PLATFORM_ARGS "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}")
	foreach(FNQL_LIBJPEG_TURBO_TARGET_INPUT
			CMAKE_C_COMPILER_ARG1 CMAKE_C_COMPILER_TARGET CMAKE_SYSROOT CMAKE_TOOLCHAIN_FILE)
		if(NOT "${${FNQL_LIBJPEG_TURBO_TARGET_INPUT}}" STREQUAL "")
			list(APPEND FNQL_LIBJPEG_TURBO_PLATFORM_ARGS
				"-D${FNQL_LIBJPEG_TURBO_TARGET_INPUT}=${${FNQL_LIBJPEG_TURBO_TARGET_INPUT}}")
		endif()
	endforeach()
	if(MSVC OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
		set(FNQL_LIBJPEG_TURBO_LIBRARY "${FNQL_LIBJPEG_TURBO_BINARY_DIR}/jpeg-static.lib")
	else()
		set(FNQL_LIBJPEG_TURBO_LIBRARY "${FNQL_LIBJPEG_TURBO_BINARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}jpeg${CMAKE_STATIC_LIBRARY_SUFFIX}")
	endif()
	ExternalProject_Add(fnql_libjpeg_turbo
		SOURCE_DIR "${FNQL_LIBJPEG_TURBO_SOURCE_DIR}"
		BINARY_DIR "${FNQL_LIBJPEG_TURBO_BINARY_DIR}"
		CMAKE_ARGS
			"-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
			"-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
			${FNQL_LIBJPEG_TURBO_PLATFORM_ARGS}
			"-DENABLE_SHARED=OFF"
			"-DENABLE_STATIC=ON"
			"-DWITH_CRT_DLL=ON"
			"-DWITH_JPEG8=ON"
			"-DWITH_SIMD=OFF"
			"-DWITH_TESTS=OFF"
			"-DWITH_TOOLS=OFF"
			"-DWITH_TURBOJPEG=OFF"
		BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target jpeg-static
		INSTALL_COMMAND ""
		BUILD_BYPRODUCTS "${FNQL_LIBJPEG_TURBO_LIBRARY}")
	add_library(fnql_jpeg STATIC IMPORTED GLOBAL)
	set_target_properties(fnql_jpeg PROPERTIES
		IMPORTED_LOCATION "${FNQL_LIBJPEG_TURBO_LIBRARY}"
		INTERFACE_INCLUDE_DIRECTORIES "${FNQL_LIBJPEG_TURBO_BINARY_DIR};${FNQL_LIBJPEG_TURBO_SOURCE_DIR}/src")
	add_dependencies(fnql_jpeg fnql_libjpeg_turbo)
	ADD_LIBRARY(client OBJECT ${CLIENT_SRCS})
	add_dependencies(client fnql_libjpeg_turbo)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_SYSTEM_JPEG)
	TARGET_INCLUDE_DIRECTORIES(client PRIVATE "${FNQL_LIBJPEG_TURBO_BINARY_DIR}" "${FNQL_LIBJPEG_TURBO_SOURCE_DIR}/src")
	list(APPEND FNQL_CLIENT_LINK_LIBS fnql_jpeg)
ENDIF()

target_compile_features(client PRIVATE cxx_std_17)
target_link_libraries(client PRIVATE ZLIB::ZLIB)
IF(FNQL_CLIENT_COMPILE_DEFINITIONS)
	target_compile_definitions(client PRIVATE ${FNQL_CLIENT_COMPILE_DEFINITIONS})
ENDIF()
IF(FNQL_CLIENT_INCLUDE_DIRS)
	target_include_directories(client PRIVATE ${FNQL_CLIENT_INCLUDE_DIRS})
ENDIF()

set(FNQL_OPENAL_INCLUDE_DIRS)
set(FNQL_OPENAL_INCLUDE_CANDIDATES
	"${CMAKE_CURRENT_SOURCE_DIR}/code/openal/include")
file(GLOB FNQL_OPENAL_SOFT_INCLUDE_CANDIDATES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/subprojects/openal-soft-*/include")
list(APPEND FNQL_OPENAL_INCLUDE_CANDIDATES ${FNQL_OPENAL_SOFT_INCLUDE_CANDIDATES})
foreach(FNQL_OPENAL_INCLUDE_CANDIDATE IN LISTS FNQL_OPENAL_INCLUDE_CANDIDATES)
	if(EXISTS "${FNQL_OPENAL_INCLUDE_CANDIDATE}/AL/al.h")
		list(APPEND FNQL_OPENAL_INCLUDE_DIRS "${FNQL_OPENAL_INCLUDE_CANDIDATE}")
	endif()
endforeach()
if(FNQL_OPENAL_INCLUDE_DIRS)
	target_include_directories(client PRIVATE ${FNQL_OPENAL_INCLUDE_DIRS})
endif()

ADD_EXECUTABLE(fnql-audiozonesc code/tools/audiozones/audiozonesc.cpp)
target_compile_features(fnql-audiozonesc PRIVATE cxx_std_17)
target_include_directories(fnql-audiozonesc PRIVATE code/client/audio/shared)
IF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
	TARGET_LINK_LIBRARIES(fnql-audiozonesc PRIVATE stdc++fs)
ENDIF()

IF(FNQL_AUDIO_LOOPBACK_TESTS)
	enable_testing()
	ADD_EXECUTABLE(fnql_audio_zone_tests tests/audio/audiozone_runtime_tests.cpp)
	target_compile_features(fnql_audio_zone_tests PRIVATE cxx_std_17)
	target_include_directories(fnql_audio_zone_tests PRIVATE code/client/audio/shared)
	add_test(NAME fnql_audio_zones COMMAND fnql_audio_zone_tests)
	add_test(NAME fnql_audio_zone_authoring
		COMMAND $<TARGET_FILE:fnql-audiozonesc> -o "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/audiozone_authoring_test.azb" "${CMAKE_CURRENT_SOURCE_DIR}/tests/audio/authoring_metadata.audiozones")
	add_test(NAME fnql_audio_zone_authoring_audit
		COMMAND $<TARGET_FILE:fnql-audiozonesc> --audit --strict --samples 512 "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/audiozone_authoring_test.azb")
	set_tests_properties(fnql_audio_zone_authoring PROPERTIES FIXTURES_SETUP fnql_audio_zone_authoring_fixture)
	set_tests_properties(fnql_audio_zone_authoring_audit PROPERTIES
		DEPENDS fnql_audio_zone_authoring
		FIXTURES_REQUIRED fnql_audio_zone_authoring_fixture)

	ADD_EXECUTABLE(fnql_audio_recovery_tests tests/audio/recovery_policy_tests.cpp)
	target_compile_features(fnql_audio_recovery_tests PRIVATE cxx_std_17)
	target_include_directories(fnql_audio_recovery_tests PRIVATE code/client/audio/shared)
	add_test(NAME fnql_audio_recovery COMMAND fnql_audio_recovery_tests)

	ADD_EXECUTABLE(fnql_audio_init_policy_tests tests/audio/init_policy_tests.cpp)
	target_compile_features(fnql_audio_init_policy_tests PRIVATE cxx_std_17)
	target_include_directories(fnql_audio_init_policy_tests PRIVATE code/client/audio/shared)
	add_test(NAME fnql_audio_init_policy COMMAND fnql_audio_init_policy_tests)

	ADD_EXECUTABLE(fnql_audio_occlusion_tests tests/audio/occlusion_policy_tests.cpp)
	target_compile_features(fnql_audio_occlusion_tests PRIVATE cxx_std_17)
	target_include_directories(fnql_audio_occlusion_tests PRIVATE code/client/audio/shared)
	add_test(NAME fnql_audio_occlusion COMMAND fnql_audio_occlusion_tests)

	ADD_EXECUTABLE(fnql_audio_tone_tests tests/audio/tone_policy_tests.cpp)
	target_compile_features(fnql_audio_tone_tests PRIVATE cxx_std_17)
	target_include_directories(fnql_audio_tone_tests PRIVATE code/client/audio/shared)
	add_test(NAME fnql_audio_tone COMMAND fnql_audio_tone_tests)

	ADD_EXECUTABLE(fnql_audio_voice_queue_tests tests/audio/voice_queue_tests.cpp)
	target_compile_features(fnql_audio_voice_queue_tests PRIVATE cxx_std_17)
	target_include_directories(fnql_audio_voice_queue_tests PRIVATE code/client/audio/shared)
	add_test(NAME fnql_audio_voice_queue COMMAND fnql_audio_voice_queue_tests)

	ADD_EXECUTABLE(fnql_audio_loopback_tests tests/audio/openal_loopback_tests.cpp)
	target_compile_features(fnql_audio_loopback_tests PRIVATE cxx_std_17)
	if(FNQL_OPENAL_INCLUDE_DIRS)
		target_include_directories(fnql_audio_loopback_tests PRIVATE ${FNQL_OPENAL_INCLUDE_DIRS})
	endif()
	IF(UNIX)
		TARGET_LINK_LIBRARIES(fnql_audio_loopback_tests PRIVATE m ${CMAKE_DL_LIBS})
	ENDIF()
	add_test(NAME fnql_audio_loopback COMMAND fnql_audio_loopback_tests)
	set_tests_properties(fnql_audio_loopback PROPERTIES SKIP_RETURN_CODE 77)

	find_package(Python3 COMPONENTS Interpreter)
	IF(Python3_Interpreter_FOUND)
		add_test(NAME fnql_retail_sound_compat_source
			COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/audio/retail_sound_compat_source_tests.py")
		add_test(NAME fnql_audio_init_source
			COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/audio/audio_init_source_tests.py")
		add_test(NAME fnql_audio_zone_sweep_script
			COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/audio/audio_zone_sweep_tests.py")
		add_test(NAME fnql_audio_zone_material_map
			COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/audio/audio_zone_material_map_tests.py" $<TARGET_FILE:fnql-audiozonesc>)
	ENDIF()
ENDIF()

IF(FNQL_GLX_LOGIC_TESTS)
	enable_testing()
	ADD_EXECUTABLE(fnql_glx_logic_tests
		tests/glx/glx_logic_tests.cpp
		code/rendererglx/glx_executor.cpp)
	target_compile_features(fnql_glx_logic_tests PRIVATE cxx_std_17)
	target_include_directories(fnql_glx_logic_tests PRIVATE
		code/rendererglx
		code/qcommon
		code/renderercommon)
	add_test(NAME fnql_glx_logic COMMAND fnql_glx_logic_tests)
	find_package(Python3 COMPONENTS Interpreter)
	IF(Python3_Interpreter_FOUND)
		add_test(NAME fnql_glx_header_boundary
			COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/glx/glx_header_boundary_tests.py")
	ENDIF()
ENDIF()

enable_testing()

ADD_EXECUTABLE(fntech3_game_abi_layout_tests tests/game_abi_layout_tests.cpp)
target_compile_features(fntech3_game_abi_layout_tests PRIVATE cxx_std_17)
add_test(NAME fntech3_game_abi_layouts COMMAND fntech3_game_abi_layout_tests)
ADD_EXECUTABLE(fntech3_game_abi_adapter_tests
	tests/game_abi_adapter_tests.cpp
	code/compat/game_abi_adapter.c)
target_compile_features(fntech3_game_abi_adapter_tests PRIVATE cxx_std_17)
target_include_directories(fntech3_game_abi_adapter_tests PRIVATE code/qcommon code/compat)
add_test(NAME fntech3_game_abi_adapters COMMAND fntech3_game_abi_adapter_tests)
ADD_EXECUTABLE(fntech3_server_game_data_tests
	tests/server_game_data_tests.cpp
	code/server/server_game_data.cpp
	code/compat/game_abi_adapter.c)
target_compile_features(fntech3_server_game_data_tests PRIVATE cxx_std_17)
target_include_directories(fntech3_server_game_data_tests PRIVATE
	code/qcommon code/compat code/server)
add_test(NAME fntech3_server_game_data COMMAND fntech3_server_game_data_tests)
ADD_EXECUTABLE(fntech3_collision_game_contract_tests
	tests/collision_game_contract_tests.cpp)
target_compile_features(fntech3_collision_game_contract_tests PRIVATE cxx_std_17)
add_test(NAME fntech3_collision_game_contract
	COMMAND fntech3_collision_game_contract_tests)
ADD_EXECUTABLE(fnql_bmp_loader_tests
	tests/bmp_loader_tests.c
	code/renderercommon/tr_image_bmp.c)
target_include_directories(fnql_bmp_loader_tests PRIVATE
	code/qcommon
	code/renderercommon)
add_test(NAME fnql_bmp_loader COMMAND fnql_bmp_loader_tests)

ADD_EXECUTABLE(fnql_huffman_decoder_tests
	tests/huffman_decoder_tests.c
	code/qcommon/huffman.c)
target_include_directories(fnql_huffman_decoder_tests PRIVATE code/qcommon)
add_test(NAME fnql_huffman_decoder COMMAND fnql_huffman_decoder_tests)

ADD_EXECUTABLE(fnql_qvm_header_tests tests/qvm_header_tests.c)
target_include_directories(fnql_qvm_header_tests PRIVATE code/qcommon)
add_test(NAME fnql_qvm_header_bounds COMMAND fnql_qvm_header_tests)

ADD_EXECUTABLE(fnql_filesystem_write_bounds_tests
	tests/filesystem_write_bounds_tests.c)
target_include_directories(fnql_filesystem_write_bounds_tests PRIVATE code/qcommon)
add_test(NAME fnql_filesystem_write_bounds COMMAND fnql_filesystem_write_bounds_tests)

ADD_EXECUTABLE(fnql_png_loader_tests
	tests/png_loader_tests.c
	code/renderercommon/tr_image_png.c)
target_link_libraries(fnql_png_loader_tests PRIVATE ZLIB::ZLIB)
target_include_directories(fnql_png_loader_tests PRIVATE
	code/qcommon
	code/renderercommon)
add_test(NAME fnql_png_loader COMMAND fnql_png_loader_tests)

ADD_EXECUTABLE(fntech3_unzip_tests tests/unzip_tests.c code/qcommon/unzip.c)
target_link_libraries(fntech3_unzip_tests PRIVATE ZLIB::ZLIB)
add_test(NAME fntech3_unzip COMMAND fntech3_unzip_tests "${CMAKE_CURRENT_BINARY_DIR}/unzip-test.pk3")

ADD_EXECUTABLE(fnql_window_placement_tests tests/window_placement_tests.cpp)
target_compile_features(fnql_window_placement_tests PRIVATE cxx_std_17)
add_test(NAME fnql_window_placement COMMAND fnql_window_placement_tests)

ADD_EXECUTABLE(fnql_client_cvar_compat_tests tests/client_cvar_compat_tests.cpp)
target_compile_features(fnql_client_cvar_compat_tests PRIVATE cxx_std_17)
add_test(NAME fnql_client_cvar_compat COMMAND fnql_client_cvar_compat_tests)

ADD_EXECUTABLE(fnql_server_cvar_compat_tests tests/server_cvar_compat_tests.cpp)
target_compile_features(fnql_server_cvar_compat_tests PRIVATE cxx_std_17)
add_test(NAME fnql_server_cvar_compat COMMAND fnql_server_cvar_compat_tests)

ADD_EXECUTABLE(fnql_ql_font_text_tests tests/ql_font_text_tests.cpp)
target_compile_features(fnql_ql_font_text_tests PRIVATE cxx_std_17)
add_test(NAME fnql_ql_font_text COMMAND fnql_ql_font_text_tests)

foreach(jk2_test handshake client_adapter font message_codec)
	ADD_EXECUTABLE(fntech3_jk2_${jk2_test}_tests tests/jk2_${jk2_test}_tests.cpp)
	add_test(NAME fntech3_jk2_${jk2_test} COMMAND fntech3_jk2_${jk2_test}_tests)
endforeach()

ADD_EXECUTABLE(fnql_ql_bsp_contract_tests tests/ql_bsp_contract_tests.cpp)
target_compile_features(fnql_ql_bsp_contract_tests PRIVATE cxx_std_17)
target_include_directories(fnql_ql_bsp_contract_tests PRIVATE code/qcommon)
add_test(NAME fnql_ql_bsp_contract COMMAND fnql_ql_bsp_contract_tests)

ADD_EXECUTABLE(fnql_collision_model_parity_tests tests/collision_model_parity_tests.cpp)
target_compile_features(fnql_collision_model_parity_tests PRIVATE cxx_std_17)
add_test(NAME fnql_collision_model_parity COMMAND fnql_collision_model_parity_tests)

ADD_EXECUTABLE(fnql_netchan_profile_tests tests/netchan_profile_tests.cpp)
target_compile_features(fnql_netchan_profile_tests PRIVATE cxx_std_17)
target_include_directories(fnql_netchan_profile_tests PRIVATE code/qcommon)
add_test(NAME fnql_netchan_profiles COMMAND fnql_netchan_profile_tests)

ADD_EXECUTABLE(fnql_netchan_codec_tests tests/netchan_codec_tests.cpp)
target_compile_features(fnql_netchan_codec_tests PRIVATE cxx_std_17)
target_include_directories(fnql_netchan_codec_tests PRIVATE code/qcommon)
add_test(NAME fnql_netchan_codec COMMAND fnql_netchan_codec_tests)

ADD_EXECUTABLE(fnql_message_codec_tests
	tests/message_codec_tests.cpp
	tests/message_drop_trap.c
	code/qcommon/msg.cpp
	code/qcommon/huffman_static.c)
target_compile_features(fnql_message_codec_tests PRIVATE cxx_std_17)
target_compile_definitions(fnql_message_codec_tests PRIVATE DEDICATED)
target_include_directories(fnql_message_codec_tests PRIVATE code/qcommon)
add_test(NAME fnql_message_codec COMMAND fnql_message_codec_tests)

# Meson's implicit include directories put the source root on these tests'
# path, and jka_render_tests.cpp includes code/compat/ from there.
foreach(jka_test sp_abi sp_game_bridge sp_ghoul2 wire snapshot ui font font_host client string_package string_host render sp_player_abi sp_model_precache sp_animation_cfg)
	ADD_EXECUTABLE(fntech3_jka_${jka_test}_tests tests/jka_${jka_test}_tests.cpp)
	target_compile_features(fntech3_jka_${jka_test}_tests PRIVATE cxx_std_17)
	target_include_directories(fntech3_jka_${jka_test}_tests PRIVATE code/compat "${CMAKE_CURRENT_SOURCE_DIR}")
	add_test(NAME fntech3_jka_${jka_test} COMMAND fntech3_jka_${jka_test}_tests)
endforeach()
target_sources(fntech3_jka_sp_game_bridge_tests PRIVATE
	code/server/server_game_data.cpp code/compat/game_abi_adapter.c tests/message_drop_trap.c)
ADD_EXECUTABLE(fntech3_demo_wire_profile_tests tests/demo_wire_profile_tests.cpp)
target_compile_features(fntech3_demo_wire_profile_tests PRIVATE cxx_std_17)
add_test(NAME fntech3_demo_wire_profile COMMAND fntech3_demo_wire_profile_tests)
ADD_EXECUTABLE(fntech3_jka_sp_state_tests tests/jka_sp_state_tests.cpp
	code/server/server_game_data.cpp code/compat/game_abi_adapter.c)
target_compile_features(fntech3_jka_sp_state_tests PRIVATE cxx_std_17)
add_test(NAME fntech3_jka_sp_state COMMAND fntech3_jka_sp_state_tests)
ADD_EXECUTABLE(fntech3_jka_netchan_tests tests/jka_netchan_tests.cpp
	code/qcommon/net_chan.cpp code/qcommon/msg.cpp code/qcommon/huffman_static.c)
target_compile_features(fntech3_jka_netchan_tests PRIVATE cxx_std_17)
target_compile_definitions(fntech3_jka_netchan_tests PRIVATE DEDICATED)
target_include_directories(fntech3_jka_netchan_tests PRIVATE code/qcommon)
add_test(NAME fntech3_jka_netchan COMMAND fntech3_jka_netchan_tests)

ADD_EXECUTABLE(fnql_demo_stream_tests tests/demo_stream_tests.cpp)
target_compile_features(fnql_demo_stream_tests PRIVATE cxx_std_17)
target_include_directories(fnql_demo_stream_tests PRIVATE code/client)
add_test(NAME fnql_demo_stream COMMAND fnql_demo_stream_tests)

ADD_EXECUTABLE(fnql_webpak_format_tests tests/webpak_format_tests.cpp)
target_compile_features(fnql_webpak_format_tests PRIVATE cxx_std_17)
target_include_directories(fnql_webpak_format_tests PRIVATE code/client)
add_test(NAME fnql_webpak_format COMMAND fnql_webpak_format_tests)

ADD_EXECUTABLE(fnql_webui_backend_tests tests/webui_backend_tests.cpp)
target_compile_features(fnql_webui_backend_tests PRIVATE cxx_std_17)
target_include_directories(fnql_webui_backend_tests PRIVATE code/client)
add_test(NAME fnql_webui_backend COMMAND fnql_webui_backend_tests)

ADD_EXECUTABLE(fnql_zmq_endpoint_tests tests/zmq_endpoint_tests.cpp)
target_compile_features(fnql_zmq_endpoint_tests PRIVATE cxx_std_17)
target_include_directories(fnql_zmq_endpoint_tests PRIVATE code/server)
add_test(NAME fnql_zmq_endpoint COMMAND fnql_zmq_endpoint_tests)

ADD_EXECUTABLE(fnql_factory_catalog_tests
	tests/factory_catalog_tests.cpp
	code/server/factory_catalog.cpp)
target_compile_features(fnql_factory_catalog_tests PRIVATE cxx_std_17)
target_include_directories(fnql_factory_catalog_tests PRIVATE code/server)
add_test(NAME fnql_factory_catalog COMMAND fnql_factory_catalog_tests)

ADD_EXECUTABLE(fnql_factory_rotation_tests
	tests/factory_rotation_tests.cpp
	code/server/factory_rotation.cpp)
target_compile_features(fnql_factory_rotation_tests PRIVATE cxx_std_17)
target_include_directories(fnql_factory_rotation_tests PRIVATE code/server)
add_test(NAME fnql_factory_rotation COMMAND fnql_factory_rotation_tests)

find_package(Python3 COMPONENTS Interpreter)
IF(Python3_Interpreter_FOUND)
	add_test(NAME fntech3_server_user_move
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/server_user_move_tests.py")
	add_test(NAME fntech3_cgame_usercmd_abi
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/cgame_usercmd_abi_tests.py")
	add_test(NAME fntech3_cgame_extension_contract
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/cgame_extension_contract_tests.py")
	add_test(NAME fntech3_vm_native_module
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/vm_native_module_tests.py")
	add_test(NAME fntech3_ui_module_abi
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/ui_module_abi_tests.py")
	add_test(NAME fntech3_q3_fov_profile
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/q3_fov_profile_tests.py")
	add_test(NAME fntech3_profile_config
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/profile_config_tests.py")
	add_test(NAME fntech3_pure_profile_contract
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/pure_profile_contract_tests.py")
	add_test(NAME fntech3_demo_record_profile
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/demo_record_profile_tests.py")
	set(FNQL_WEBPAK_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/fnql-web.pak")
	set(FNQL_WEBPAK_SOURCES
		"${CMAKE_CURRENT_SOURCE_DIR}/code/client/webui/index.html"
		"${CMAKE_CURRENT_SOURCE_DIR}/code/client/webui/fnql-settings.js"
		"${CMAKE_CURRENT_SOURCE_DIR}/code/client/webui/css/fnql-settings.css")
	add_custom_command(
		OUTPUT "${FNQL_WEBPAK_OUTPUT}"
		COMMAND ${Python3_EXECUTABLE}
			"${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_webpak.py"
			--source-root "${CMAKE_CURRENT_SOURCE_DIR}/code/client/webui"
			--output "${FNQL_WEBPAK_OUTPUT}"
		DEPENDS ${FNQL_WEBPAK_SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_webpak.py"
		VERBATIM)
	add_custom_target(fnql_webpak ALL DEPENDS "${FNQL_WEBPAK_OUTPUT}")
	add_test(NAME fnql_webpak_builder_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/webpak_builder_tests.py")
	add_test(NAME fnql_ql_font_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/ql_font_source_tests.py")
	add_test(NAME fntech3_jk2_sp_host
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/jk2_sp_host_tests.py")
	add_test(NAME fnql_window_management_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/window_management_source_tests.py")
	add_test(NAME fnql_macos_support_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/macos_support_source_tests.py")
	add_test(NAME fnql_client_cvar_compat_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/client_cvar_compat_source_tests.py")
	add_test(NAME fnql_server_cvar_compat_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/server_cvar_compat_source_tests.py")
	add_test(NAME fnql_spawn_view_angle_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/spawn_view_angle_source_tests.py")
	add_test(NAME fnql_collision_model_parity_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/collision_model_parity_source_tests.py")
	add_test(NAME fnql_ql_services_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/ql_services_source_tests.py")
	add_test(NAME fnql_ql_cvar_configstring_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/ql_cvar_configstring_source_tests.py")
	add_test(NAME fnql_join_menu_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/join_menu_source_tests.py")
	add_test(NAME fnql_ql_demo_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/ql_demo_source_tests.py")
	add_test(NAME fnql_workshop_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/workshop_source_tests.py")
	add_test(NAME fnql_cvar_factory_snapshot_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/cvar_factory_snapshot_tests.py")
	add_test(NAME fnql_factory_runtime_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/factory_runtime_source_tests.py")
	add_test(NAME fnql_map_overbright_cap_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/map_overbright_cap_source_tests.py")
	add_test(NAME fnql_renderer_contract_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/renderer_contract_source_tests.py")
	add_test(NAME fnql_surface_geometry_limits_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/surface_geometry_limits_source_tests.py")
	add_test(NAME fnql_renderer_cvar_compat_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/renderer_cvar_compat_source_tests.py")
	add_test(NAME fnql_fnq3_bloom_parity_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/fnq3_bloom_parity_source_tests.py")
	add_test(NAME fnql_rtx_renderer_contract_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/rtx_renderer_contract_source_tests.py")
	add_test(NAME fnql_rtx_raster_feature_parity_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/rtx_raster_feature_parity_source_tests.py")
	add_test(NAME fnql_rtx_vulkan_safety_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/rtx_vulkan_safety_source_tests.py")
	add_test(NAME fnql_rtx_runtime_smoke_source
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/rtx_runtime_smoke_tests.py")
	add_test(NAME fnql_retail_factory_probe
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/retail_factory_probe_tests.py" --meson)
	set_tests_properties(fnql_retail_factory_probe PROPERTIES SKIP_RETURN_CODE 77)
ENDIF()

# renderers
set(FNQL_FONTSTASH_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/subprojects/fontstash/src")
if(NOT EXISTS "${FNQL_FONTSTASH_INCLUDE_DIR}/fontstash.h")
	message(FATAL_ERROR
		"Retail host fonts require the pinned FontStash wrap. Run 'meson subprojects download fontstash' before configuring CMake.")
endif()
ADD_COMPILE_DEFINITIONS(BUILD_FONTSTASH)
INCLUDE_DIRECTORIES(SYSTEM "${FNQL_FONTSTASH_INCLUDE_DIR}")

# FreeType rasterizes TrueType fonts and Quake Live's host text; Meson builds
# require it for clients. ON requires it here too and OFF builds without it,
# when fonts fall back to their pre-rendered glyph pages. AUTO uses a FreeType
# that links a probe for this target, since a host library of another width is
# found as readily as the target's.
SET(FNQL_FREETYPE_LIBRARY)
IF(USE_FREETYPE STREQUAL "ON")
	find_package(Freetype REQUIRED)
	SET(FNQL_FREETYPE_LIBRARY Freetype::Freetype)
ELSEIF(USE_FREETYPE STREQUAL "AUTO")
	find_package(Freetype QUIET)
	IF(FREETYPE_FOUND)
		SET(FNQL_FREETYPE_PROBE_SOURCE
			"${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/fnql_freetype_probe.c")
		FILE(WRITE "${FNQL_FREETYPE_PROBE_SOURCE}" [=[
#include <ft2build.h>
#include FT_FREETYPE_H
int main(void) { FT_Library library; return FT_Init_FreeType(&library); }
]=])
		UNSET(FNQL_FREETYPE_LINKS CACHE)
		UNSET(FNQL_FREETYPE_LINKS)
		TRY_COMPILE(FNQL_FREETYPE_LINKS
			"${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/fnql_freetype_probe"
			"${FNQL_FREETYPE_PROBE_SOURCE}"
			CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${FREETYPE_INCLUDE_DIRS}"
			LINK_LIBRARIES ${FREETYPE_LIBRARIES}
			OUTPUT_VARIABLE FNQL_FREETYPE_PROBE_OUTPUT)
		IF(FNQL_FREETYPE_LINKS)
			SET(FNQL_FREETYPE_LIBRARY Freetype::Freetype)
		ENDIF()
	ENDIF()
ELSEIF(NOT USE_FREETYPE STREQUAL "OFF")
	MESSAGE(FATAL_ERROR "Unsupported USE_FREETYPE='${USE_FREETYPE}'; choose AUTO, ON or OFF")
ENDIF()
IF(FNQL_FREETYPE_LIBRARY)
	MESSAGE(STATUS "FreeType fonts: enabled")
ELSE()
	MESSAGE(STATUS "FreeType fonts: disabled; fonts use their pre-rendered glyph pages")
ENDIF()

AUX_SOURCE_DIRECTORY(code/renderercommon RENDERER_COMMON_SRCS)
AUX_SOURCE_DIRECTORY(code/renderer RENDERER_GL_SRCS)
AUX_SOURCE_DIRECTORY(code/rendererglx RENDERER_GLX_SRCS)
AUX_SOURCE_DIRECTORY(code/renderervk RENDERER_VK_SRCS)
AUX_SOURCE_DIRECTORY(code/rendererrtx RENDERER_RTX_SRCS)

IF(USE_RENDERER_DLOPEN)
	SET(AUX_SRCS
		code/qcommon/q_shared.c
		code/qcommon/puff.c
		code/qcommon/q_math.c
		${PLATFORM_ASM_SRCS}
	)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_RENDERER_DLOPEN RENDERER_PREFIX="${RENDERER_PREFIX}" RENDERER_DEFAULT=${RENDERER_DEFAULT})

	IF(USE_GLX)
		ADD_LIBRARY(${RENDERER_PREFIX}_glx${RENDEXT} SHARED ${RENDERER_GL_SRCS} ${RENDERER_GLX_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
		TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_glx${RENDEXT} PRIVATE USE_RENDERER_DLOPEN RENDERER_GLX)
		SET_TARGET_PROPERTIES(${RENDERER_PREFIX}_glx${RENDEXT} PROPERTIES PREFIX "")
	ENDIF()

	IF(USE_VK)
		ADD_LIBRARY(${RENDERER_PREFIX}_vk${RENDEXT} SHARED ${RENDERER_VK_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
		TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vk${RENDEXT} PRIVATE USE_RENDERER_DLOPEN RENDERER_VULKAN)
		SET_TARGET_PROPERTIES(${RENDERER_PREFIX}_vk${RENDEXT} PROPERTIES PREFIX "")
	ENDIF()

	IF(USE_RTX)
		ADD_LIBRARY(${RENDERER_PREFIX}_rtx${RENDEXT} SHARED ${RENDERER_RTX_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
		TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_rtx${RENDEXT} PRIVATE USE_RENDERER_DLOPEN RENDERER_RTX)
		SET_TARGET_PROPERTIES(${RENDERER_PREFIX}_rtx${RENDEXT} PROPERTIES PREFIX "")
	ENDIF()

	# q_shared.h pins legacy glibc math symbols for binary compatibility.  Each
	# independently loaded Unix renderer therefore needs its own libm linkage;
	# linking libm only into the client does not resolve a module's symbols.
	IF(UNIX)
		FOREACH(FNQL_RENDERER_KIND glx vk rtx)
			SET(FNQL_RENDERER_TARGET ${RENDERER_PREFIX}_${FNQL_RENDERER_KIND}${RENDEXT})
			IF(TARGET ${FNQL_RENDERER_TARGET})
				TARGET_LINK_LIBRARIES(${FNQL_RENDERER_TARGET} PRIVATE m)
			ENDIF()
		ENDFOREACH()
	ENDIF()
ELSE()
	IF(RENDERER_DEFAULT STREQUAL "glx")
		ADD_LIBRARY(${RENDERER_PREFIX}_glx OBJECT ${RENDERER_GL_SRCS} ${RENDERER_GLX_SRCS} ${RENDERER_COMMON_SRCS})
		TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_glx PRIVATE
			RENDERER_GLX
			RE_DrawScaledText=R_GLX_DrawScaledText
			RE_MeasureScaledText=R_GLX_MeasureScaledText)
	ELSEIF(RENDERER_DEFAULT STREQUAL "rtx")
		ADD_LIBRARY(${RENDERER_PREFIX}_rtx OBJECT ${RENDERER_RTX_SRCS} ${RENDERER_COMMON_SRCS})
		# The statically linked client owns fallback host-font wrappers with the
		# public RE_* names. Keep the renderer implementation private so both
		# halves can coexist in one executable while GetRefAPI supplies the ABI.
		TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_rtx PRIVATE
			RENDERER_RTX
			RE_DrawScaledText=R_RTX_DrawScaledText
			RE_MeasureScaledText=R_RTX_MeasureScaledText)
	ELSEIF(RENDERER_DEFAULT STREQUAL "vk")
		ADD_LIBRARY(${RENDERER_PREFIX}_vk OBJECT ${RENDERER_VK_SRCS} ${RENDERER_COMMON_SRCS})
		TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vk PRIVATE
			RENDERER_VULKAN
			RE_DrawScaledText=R_VK_DrawScaledText
			RE_MeasureScaledText=R_VK_MeasureScaledText)
	ENDIF()
ENDIF()

FOREACH(FNQL_RENDERER_KIND glx vk rtx)
	IF(USE_RENDERER_DLOPEN)
		SET(FNQL_RENDERER_TARGET ${RENDERER_PREFIX}_${FNQL_RENDERER_KIND}${RENDEXT})
	ELSE()
		SET(FNQL_RENDERER_TARGET ${RENDERER_PREFIX}_${FNQL_RENDERER_KIND})
	ENDIF()
	IF(TARGET ${FNQL_RENDERER_TARGET})
		TARGET_LINK_LIBRARIES(${FNQL_RENDERER_TARGET} PRIVATE ZLIB::ZLIB)
		IF(FNQL_FREETYPE_LIBRARY)
			TARGET_COMPILE_DEFINITIONS(${FNQL_RENDERER_TARGET} PRIVATE BUILD_FREETYPE)
			TARGET_LINK_LIBRARIES(${FNQL_RENDERER_TARGET} PRIVATE ${FNQL_FREETYPE_LIBRARY})
		ENDIF()
	ENDIF()
ENDFOREACH()

# common function, window system
IF(UNIX)
	SET (Q3_SRCS
		code/unix/unix_main.cpp
		code/unix/unix_shared.cpp
		code/unix/unix_syscon.cpp
		code/unix/linux_signals.cpp)
	# Without Windows resources the client links the game icons' PNG frames.
	SET(Q3_CLIENT_RESOURCES code/platform/game_icon_data.cpp)
	SET(Q3_SERVER_RESOURCES)

	IF(USE_SDL)
		SET(Q3_UI_SRCS
			code/sdl/sdl_gamma.cpp
			code/sdl/sdl_glimp.cpp
			code/sdl/sdl_input.cpp
			code/sdl/sdl_snd.cpp)
		IF(APPLE)
			list(APPEND Q3_UI_SRCS code/sdl/sdl_macos_window.m)
			# Keep compatibility with the project's CMake 3.12 minimum: Objective-C
			# became a first-class CMake language only in CMake 3.16.
			set_source_files_properties(code/sdl/sdl_macos_window.m PROPERTIES
				LANGUAGE C
				COMPILE_FLAGS "-x objective-c")
		ENDIF()
	ELSE()
		SET(Q3_UI_SRCS
			code/unix/linux_glimp.cpp
			code/unix/linux_qgl.cpp
			code/unix/linux_snd.cpp
			code/unix/x11_dga.cpp
			code/unix/x11_randr.cpp
			code/unix/x11_vidmode.cpp)
		IF(CMAKE_SYSTEM_NAME STREQUAL "Linux")
			list(APPEND Q3_UI_SRCS code/unix/linux_joystick.cpp)
		ENDIF()
		IF(USE_VK OR USE_RTX)
			list(APPEND Q3_UI_SRCS code/unix/linux_qvk.cpp)
		ENDIF()
	ENDIF()

	IF(APPLE)
		SET(CLIENT_EXE_TYPE MACOSX_BUNDLE)
		list(APPEND Q3_SRCS assets/icons/fntech3.icns)
		set_source_files_properties(assets/icons/fntech3.icns PROPERTIES
			MACOSX_PACKAGE_LOCATION Resources)
		SET(MACOSX_BUNDLE_ICON_FILE fntech3.icns)
	ENDIF()
ELSEIF(WIN32)
	SET(CLIENT_EXE_TYPE WIN32)
	SET(SERVER_EXE_TYPE WIN32)
	# Prevent Windows headers from exporting legacy min/max macros into shared
	# client headers that use the C++ standard-library overloads.
	ADD_COMPILE_DEFINITIONS(NOMINMAX)
	IF(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		ADD_COMPILE_DEFINITIONS(_CRT_SECURE_NO_WARNINGS) # v3.12+
	ELSEIF(MSVC)
		SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
		ADD_COMPILE_DEFINITIONS(_CRT_SECURE_NO_WARNINGS) # v3.12+
	ENDIF()

	SET(Q3_SRCS
		code/win32/win_main.cpp
		code/win32/win_shared.cpp
		code/win32/win_syscon.cpp)
	# Both Windows executables embed the engine icon and every game's remake.
	SET(Q3_CLIENT_RESOURCES code/win32/win_client_resource.rc)
	SET(Q3_SERVER_RESOURCES code/win32/win_client_resource.rc)

	SET(Q3_UI_SRCS
		code/win32/win_gamma.cpp
		code/win32/win_glimp.cpp
		code/win32/win_input.cpp
		code/win32/win_minimize.cpp
		code/win32/win_qgl.cpp
		code/win32/win_snd.cpp
		code/win32/win_wndproc.cpp)
	IF(USE_VK OR USE_RTX)
		list(APPEND Q3_UI_SRCS code/win32/win_qvk.cpp)
	ENDIF()
ENDIF(UNIX)

ADD_LIBRARY(q3ui OBJECT ${Q3_UI_SRCS})
IF(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT USE_SDL)
	TARGET_COMPILE_DEFINITIONS(q3ui PRIVATE USE_JOYSTICK)
ENDIF()

IF(UNIX AND USE_SDL)
	find_package(SDL3 3.2.0 CONFIG QUIET COMPONENTS SDL3)
	IF(TARGET SDL3::SDL3)
		SET(Q3_SDL_TARGET SDL3::SDL3)
	ELSE()
		find_package(PkgConfig REQUIRED)
		pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3>=3.2.0)
		SET(Q3_SDL_TARGET PkgConfig::SDL3)
	ENDIF()

	TARGET_COMPILE_DEFINITIONS(q3ui PRIVATE SDL_FUNCTION_POINTER_IS_VOID_POINTER=1)
	TARGET_LINK_LIBRARIES(q3ui PRIVATE ${Q3_SDL_TARGET})
	IF(APPLE)
		find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
		TARGET_LINK_LIBRARIES(q3ui PRIVATE ${APPKIT_FRAMEWORK})
	ENDIF()
ELSEIF(UNIX AND NOT MSVC)
	IF(NOT USE_SDL)
		find_package (X11 REQUIRED)
		TARGET_LINK_LIBRARIES(q3ui ${X11_LIBRARIES})
	ENDIF()
ENDIF()

IF(USE_VK OR USE_RTX)
	TARGET_COMPILE_DEFINITIONS(q3ui PRIVATE USE_VULKAN_API)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_VULKAN_API)
ENDIF()

IF(USE_GLX OR RENDERER_DEFAULT STREQUAL "glx")
	TARGET_COMPILE_DEFINITIONS(q3ui PRIVATE USE_OPENGL_API)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_OPENGL_API)
ENDIF()

# client executable

ADD_EXECUTABLE(${CNAME}${BINEXT} ${CLIENT_EXE_TYPE} ${Q3_SRCS} ${Q3_CLIENT_RESOURCES})
TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} qcommon botlib mohaa_tiki client q3ui Threads::Threads ZLIB::ZLIB ${FNQL_CLIENT_LINK_LIBS})
set_property(TARGET ${CNAME}${BINEXT} PROPERTY LINKER_LANGUAGE CXX)
IF(APPLE)
	SET_TARGET_PROPERTIES(${CNAME}${BINEXT} PROPERTIES
		MACOSX_BUNDLE_BUNDLE_NAME "FnQL"
		MACOSX_BUNDLE_GUI_IDENTIFIER "org.fnql.fnql"
		MACOSX_BUNDLE_SHORT_VERSION_STRING "${FNQL_MACOS_SHORT_VERSION}"
		MACOSX_BUNDLE_BUNDLE_VERSION "${FNQL_MACOS_BUNDLE_VERSION}"
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/misc/macos/Info.plist.in")
ENDIF()
IF(TARGET fnql_webpak)
	add_dependencies(${CNAME}${BINEXT} fnql_webpak)
	add_custom_command(
		TARGET ${CNAME}${BINEXT} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${FNQL_WEBPAK_OUTPUT}"
			"$<TARGET_FILE_DIR:${CNAME}${BINEXT}>/fnql-web.pak")
ENDIF()
#TARGET_LINK_OPTIONS(${CNAME} PRIVATE -fno-pie) # v3.13+

IF(NOT USE_RENDERER_DLOPEN)
	IF(RENDERER_DEFAULT STREQUAL "glx")
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${RENDERER_PREFIX}_glx)
	ELSEIF(RENDERER_DEFAULT STREQUAL "rtx")
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${RENDERER_PREFIX}_rtx)
	ELSEIF(RENDERER_DEFAULT STREQUAL "vk")
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${RENDERER_PREFIX}_vk)
	ENDIF()
ENDIF()

# dedicated server executable

ADD_EXECUTABLE(${DNAME}${BINEXT} ${SERVER_EXE_TYPE} ${Q3_SRCS} ${Q3_SERVER_RESOURCES})
TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} qcommon_ded botlib mohaa_tiki Threads::Threads ZLIB::ZLIB)
set_property(TARGET ${DNAME}${BINEXT} PROPERTY LINKER_LANGUAGE CXX)
#TARGET_LINK_OPTIONS(${DNAME} PRIVATE -fno-pie) # v3.13+

IF(WIN32)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} winmm comctl32 ws2_32)
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} winmm comctl32 ws2_32)
ELSE()
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} m ${CMAKE_DL_LIBS})
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} m ${CMAKE_DL_LIBS})
ENDIF()

# fntech3-launcher (code/launcher, docs/LAUNCHER.md) finds the player's game
# installations and starts this build's client for one of them. It draws with
# SDL3 and Dear ImGui, so it is built wherever SDL3 is found; its core --
# discovery, settings and the engine command line -- is tested without either.
SET(FNTECH3_LAUNCHER_CORE_SRCS
	code/launcher/launcher_catalog.cpp
	code/launcher/launcher_discovery.cpp
	code/launcher/launcher_fs.cpp
	code/launcher/launcher_json.cpp
	code/launcher/launcher_launch.cpp
	code/launcher/launcher_settings.cpp
	code/launcher/launcher_vdf.cpp)
ADD_EXECUTABLE(fntech3_launcher_core_tests tests/launcher_core_tests.cpp ${FNTECH3_LAUNCHER_CORE_SRCS}
	code/qcommon/game_profile.cpp)
TARGET_INCLUDE_DIRECTORIES(fntech3_launcher_core_tests PRIVATE code/qcommon)
add_test(NAME fntech3_launcher_core COMMAND fntech3_launcher_core_tests)

OPTION(BUILD_LAUNCHER "Build fntech3-launcher where SDL3 is available" ON)
IF(BUILD_LAUNCHER)
	IF(Q3_SDL_TARGET)
		SET(FNTECH3_LAUNCHER_SDL ${Q3_SDL_TARGET})
	ELSE()
		find_package(SDL3 3.2.0 CONFIG QUIET COMPONENTS SDL3)
		IF(TARGET SDL3::SDL3)
			SET(FNTECH3_LAUNCHER_SDL SDL3::SDL3)
		ELSE()
			find_package(PkgConfig QUIET)
			IF(PKG_CONFIG_FOUND)
				pkg_check_modules(FNTECH3_LAUNCHER_SDL3 QUIET IMPORTED_TARGET sdl3>=3.2.0)
				IF(TARGET PkgConfig::FNTECH3_LAUNCHER_SDL3)
					SET(FNTECH3_LAUNCHER_SDL PkgConfig::FNTECH3_LAUNCHER_SDL3)
				ENDIF()
			ENDIF()
		ENDIF()
	ENDIF()
	IF(FNTECH3_LAUNCHER_SDL)
		SET(FNTECH3_LAUNCHER_SRCS ${FNTECH3_LAUNCHER_CORE_SRCS}
			code/launcher/launcher_app.cpp
			code/launcher/launcher_fonts.cpp
			code/launcher/launcher_imgui_sdl.cpp
			code/launcher/launcher_main.cpp
			code/launcher/launcher_widgets.cpp
			code/platform/game_icon_data.cpp
			code/qcommon/game_profile.cpp
			code/qcommon/puff.c
			${FNTECH3_IMGUI_DIR}/imgui.cpp
			${FNTECH3_IMGUI_DIR}/imgui_draw.cpp
			${FNTECH3_IMGUI_DIR}/imgui_tables.cpp
			${FNTECH3_IMGUI_DIR}/imgui_widgets.cpp)
		IF(WIN32)
			LIST(APPEND FNTECH3_LAUNCHER_SRCS code/launcher/launcher.rc)
		ENDIF()
		ADD_EXECUTABLE(fntech3-launcher${BINEXT} WIN32 ${FNTECH3_LAUNCHER_SRCS})
		TARGET_INCLUDE_DIRECTORIES(fntech3-launcher${BINEXT} PRIVATE code/qcommon)
		TARGET_COMPILE_DEFINITIONS(fntech3-launcher${BINEXT} PRIVATE
			FNTECH3_LAUNCHER_CLIENT_NAME="${CNAME}${BINEXT}" FNTECH3_LAUNCHER_SERVER_NAME="${DNAME}${BINEXT}")
		TARGET_LINK_LIBRARIES(fntech3-launcher${BINEXT} ${FNTECH3_LAUNCHER_SDL} Threads::Threads)
	ELSE()
		MESSAGE(STATUS "fntech3-launcher is not built: SDL3 was not found")
	ENDIF()
ENDIF()
