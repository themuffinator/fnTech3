project('fnTech3', 'c', 'cpp',
  license: 'GPL-2.0-or-later',
  version: run_command(
    find_program('python3', 'python', required: true),
    '-c',
    'import re, pathlib; text = pathlib.Path("version/fntech3_version.h").read_text(); print(re.search(r"#define FNTECH3_VERSION_STRING \\"([^\\"]+)\\"", text).group(1))',
    check: true,
  ).stdout().strip(),
  meson_version: '>= 0.60.0',
  default_options: [
    meson.version().version_compare('>=1.3.0') ? 'c_std=gnu11,c11' : 'c_std=gnu11',
    'cpp_std=c++17',
    'buildtype=debugoptimized',
    'b_vscrt=from_buildtype',
    # curl's static fallback has a separate zlib subproject. Keep that nested
    # fallback static too, so installed engines do not depend on z-1.dll.
    'zlib:default_library=static',
    'warning_level=3',
  ],
)

python = find_program('python3', 'python', required: true)

# Native fixture compilers must follow the configured proof architecture,
# including when tests run from a shell without the Visual Studio environment.
if host_machine.system() == 'windows' and host_machine.cpu_family() == 'x86'
  native_fixture_env = environment()
  native_fixture_env.set('VSCMD_ARG_TGT_ARCH', 'x86')
  add_test_setup('configurednativearchitecture', env: native_fixture_env, is_default: true)
endif
fs = import('fs')
cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

fallback_defaults = ['default_library=static']
quiet_fallback_defaults = fallback_defaults + ['warning_level=0']
curl_fallback_defaults = quiet_fallback_defaults + [
  'tests=disabled',
  'tool=disabled',
  'unittests=disabled',
  # Windows releases use Schannel and the built-in resolver. Keep optional
  # transfer encodings and protocols from silently linking runner-installed
  # DLLs into an otherwise self-contained package.
  'brotli=disabled',
  'gsasl=disabled',
  'gss-api=disabled',
  'http2=disabled',
  'idn=disabled',
  'libuv=disabled',
  'openssl=disabled',
  'psl=disabled',
  'rtmp=disabled',
  'ssh=disabled',
  'zstd=disabled',
]
openal_fallback_defaults = quiet_fallback_defaults
vorbis_fallback_defaults = quiet_fallback_defaults + [
  'tests=false',
  'ogg:default_library=static',
  'ogg:warning_level=0',
]
jpeg_fallback_defaults = quiet_fallback_defaults + ['tests=disabled']

win32 = host_machine.system() == 'windows'
darwin = host_machine.system() == 'darwin'
linux = host_machine.system() == 'linux'
if darwin
  add_languages('objc')
endif
cpu_family = host_machine.cpu_family()
q3_only = get_option('q3-only')
rtcw_development = get_option('rtcw-development')
wolfet_development = get_option('wolfet-development')
stvef_development = get_option('stvef-development')
jka_development = get_option('jka-development')
jk2_development = get_option('jk2-development')
alice_development = get_option('alice-development')
fakk2_development = get_option('fakk2-development')
mohaa_development = get_option('mohaa-development')
q3_ihv_development = get_option('q3-ihv-development')
q3test_development = get_option('q3test-development')
if q3test_development and q3_only
  error('Q3Test developer probes require the shared Windows x86 executable, not the Q3-only target.')
endif
if jka_development and q3_only
  error('Jedi Academy developer probes require the shared x86 executable, not the Q3-only target.')
endif
if jk2_development and q3_only
  error('Jedi Outcast developer probes require the shared x86 executable, not the Q3-only target.')
endif
if alice_development and q3_only
  error('Alice developer probes require the shared x86 executable, not the Q3-only target.')
endif
if fakk2_development and q3_only
  error('F.A.K.K.2 developer probes require the shared x86 executable, not the Q3-only target.')
endif
if mohaa_development and q3_only
  error('Medal of Honor developer probes require the shared x86 executable, not the Q3-only target.')
endif
if q3_ihv_development and q3_only
  error('IHV developer probes require the shared Windows x86 executable.')
endif
if wolfet_development and q3_only
  error('WolfET developer probes require the shared executable.')
endif
if stvef_development and q3_only
  error('Elite Force developer probes require the shared x86 executable, not the Q3-only target.')
endif
if rtcw_development and q3_only
  error('RTCW developer probes require the shared x86 executable, not the Q3-only target.')
endif
fnql_x86_target_probe = '''
#if !defined(_M_IX86) && !defined(__i386__)
#error FnQL requires an x86 target
#endif
typedef char fnql_pointer_width_must_be_32_bits[(sizeof(void *) == 4) ? 1 : -1];
int main(void) { return 0; }
'''
fnql_target_is_32bit_x86 = cc.compiles(fnql_x86_target_probe, name: '32-bit x86 target')
if q3_only
  q3_x64_target_probe = '''
#if !defined(_WIN32) || (!defined(_M_AMD64) && !defined(__x86_64__))
#error Q3-only diagnostic builds require Windows x86_64
#endif
typedef char q3_pointer_width_must_be_64_bits[(sizeof(void *) == 8) ? 1 : -1];
int main(void) { return 0; }
'''
  if not win32 or cpu_family != 'x86_64' or not cc.compiles(q3_x64_target_probe, name: 'Q3-only Windows x86_64 target')
    error('-Dq3-only=true is an experimental Windows x86_64 diagnostic target. Configure with a 64-bit Windows compiler; use the default shared Win32 build for Quake Live.')
  endif
  warning('Q3-only diagnostic build: Quake Live is disabled. This build does not satisfy the shared Q3/QL release contract.')
else
if cpu_family != 'x86' or not fnql_target_is_32bit_x86
  error('fnTech3 shared Q3/QL builds support only 32-bit x86 targets; Meson configured host CPU family "' + cpu_family + '" or the compiler target probe failed. Select Win32 on Windows or use an i686 cross file on other platforms.')
endif
endif

renderer_prefix = 'fnql'
build_client = get_option('build-client')
build_server = get_option('build-server')
renderer_default = get_option('renderer-default')
renderer_dlopen = get_option('renderer-dlopen')
enabled_renderers = get_option('renderers')

# A modular client must always include its default GLx module, even when a
# developer requests only a Vulkan-family module in an explicit list.
if build_client and renderer_default == 'glx' and 'glx' not in enabled_renderers
  enabled_renderers += 'glx'
endif

if not build_client
  enabled_renderers = []
elif not renderer_dlopen
  enabled_renderers = [renderer_default]
endif

renderer_arch = 'x86'
binary_suffix = ''
if q3_only
  renderer_arch = 'x86_64'
  binary_suffix = '-q3'
endif

client_binary_name = 'fntech3' + binary_suffix
server_binary_name = 'fntech3.ded' + binary_suffix

common_c_args = []
common_cpp_args = []
link_args = []
dll_link_args = []
if q3_only
  common_c_args += ['-DFNTECH3_Q3_ONLY=1']
  common_cpp_args += ['-DFNTECH3_Q3_ONLY=1']
endif

if cc.get_argument_syntax() == 'gcc'
  common_c_args += cc.get_supported_arguments([
    '-Wall',
    '-Wextra',
    '-Wimplicit',
    '-Wstrict-prototypes',
    '-Wno-unused-result',
    '-fvisibility=hidden',
    '-fno-strict-aliasing',
    '-fsigned-char',
  ])
  common_cpp_args += cpp.get_supported_arguments([
    '-Wall',
    '-Wextra',
    '-Wno-unused-result',
    '-fvisibility=hidden',
    '-fno-strict-aliasing',
    '-fsigned-char',
  ])
  if win32
    common_c_args += cc.get_supported_arguments(['-ffunction-sections'])
    common_cpp_args += cpp.get_supported_arguments(['-ffunction-sections'])
    if cc.get_linker_id() == 'link' or cc.get_linker_id() == 'lld-link'
      link_args += ['-Wl,/DYNAMICBASE', '-Wl,/NXCOMPAT']
    else
      link_args += ['-Wl,--dynamicbase', '-Wl,--nxcompat']
    endif
  else
    link_args += cc.get_supported_link_arguments(['-Wl,--gc-sections'])
  endif
elif cc.get_id() == 'msvc'
  common_c_args += ['/wd4201']
  common_cpp_args += ['/wd4201', '/EHsc']
endif

if get_option('strict-warnings')
  if cc.get_argument_syntax() == 'gcc'
    common_c_args += cc.get_supported_arguments(['-Werror'])
    common_cpp_args += cpp.get_supported_arguments(['-Werror'])
  elif cc.get_id() == 'msvc'
    common_c_args += ['/WX']
    common_cpp_args += ['/WX']
  endif
endif

if win32
  # MSVC marks portable C and Winsock interfaces as deprecated. Apply the
  # compatibility policy globally so bundled fallback dependencies receive the
  # same warning-clean Windows build as project-owned targets.
  win32_compat_args = ['-D_CRT_SECURE_NO_WARNINGS', '-D_CRT_NONSTDC_NO_WARNINGS', '-D_WINSOCK_DEPRECATED_NO_WARNINGS']
  add_global_arguments(win32_compat_args, language: ['c', 'cpp'])
  common_c_args += ['-DWIN32', '-D_WINDOWS', '-DNOMINMAX']
  common_cpp_args += ['-DWIN32', '-D_WINDOWS', '-DNOMINMAX']
elif darwin
  common_c_args += ['-DMACOS_X']
  common_cpp_args += ['-DMACOS_X']
endif

if get_option('local-headers')
  common_c_args += ['-DUSE_LOCAL_HEADERS=1']
  common_cpp_args += ['-DUSE_LOCAL_HEADERS=1']
endif

default_basedir = get_option('default-basedir')
if default_basedir != ''
  common_c_args += ['-DDEFAULT_BASEDIR="@0@"'.format(default_basedir)]
  common_cpp_args += ['-DDEFAULT_BASEDIR="@0@"'.format(default_basedir)]
endif

if not ['x86', 'x86_64', 'aarch64', 'arm', 'ppc64le', 'ppc64'].contains(cpu_family) and not cpu_family.startswith('arm')
  common_c_args += ['-DNO_VM_COMPILED']
  common_cpp_args += ['-DNO_VM_COMPILED']
endif

use_opengl_api = build_client and (renderer_default == 'glx' or 'glx' in enabled_renderers)
use_vulkan_api = build_client and (renderer_default in ['vk', 'rtx'] or 'vk' in enabled_renderers or 'rtx' in enabled_renderers)

if use_opengl_api
  common_c_args += ['-DUSE_OPENGL_API']
  common_cpp_args += ['-DUSE_OPENGL_API']
endif

if use_vulkan_api
  common_c_args += ['-DUSE_VULKAN_API']
  common_cpp_args += ['-DUSE_VULKAN_API']
endif

if renderer_dlopen
  common_c_args += [
    '-DUSE_RENDERER_DLOPEN',
    '-DRENDERER_PREFIX="@0@"'.format(renderer_prefix),
    '-DRENDERER_DEFAULT=@0@'.format(renderer_default),
  ]
  common_cpp_args += [
    '-DUSE_RENDERER_DLOPEN',
    '-DRENDERER_PREFIX="@0@"'.format(renderer_prefix),
    '-DRENDERER_DEFAULT=@0@'.format(renderer_default),
  ]
endif

include_dirs = include_directories(
  'code',
)

qcommon_src = [
	'code/compat/game_abi_adapter.c',
	'code/platform/fnql_steam.cpp',
	'code/platform/fnql_workshop.cpp',
	'code/platform/process_relaunch.cpp',
	'code/qcommon/cm_load.c',
  'code/qcommon/alice_lip.cpp',
  'code/qcommon/alice_runtime.cpp',
  'code/qcommon/alice_alias.cpp',
  'code/qcommon/alice_tiki.cpp',
  'code/qcommon/com_alice_ui.cpp',
  'code/qcommon/cm_mohaa_terrain.c',
  'code/qcommon/cm_mohaa_lights.c',
  'code/qcommon/mohaa_msg.cpp',
  'code/qcommon/cm_patch.c',
  'code/qcommon/cm_polylib.c',
  'code/qcommon/cm_test.c',
  'code/qcommon/cm_trace.c',
  'code/qcommon/cmd.c',
  'code/qcommon/common.c',
  'code/qcommon/cvar.c',
	'code/qcommon/files.c',
	'code/qcommon/game_profile.cpp',
	'code/qcommon/history.c',
  'code/qcommon/huffman_static.c',
  'code/qcommon/huffman.c',
  'code/qcommon/keys.c',
  'code/qcommon/md4.c',
  'code/qcommon/md5.c',
  'code/qcommon/msg.cpp',
  'code/qcommon/native_seh.c',
  'code/qcommon/net_chan.cpp',
  'code/qcommon/net_ip.c',
  'code/qcommon/puff.c',
  'code/qcommon/q_math.c',
  'code/qcommon/q_shared.c',
  'code/qcommon/unzip.c',
  'code/qcommon/vm_interpreted.c',
  'code/qcommon/vm.c',
]

if cpu_family in ['x86', 'x86_64']
  qcommon_src += 'code/qcommon/vm_x86.c'
elif cpu_family == 'aarch64'
  qcommon_src += 'code/qcommon/vm_aarch64.c'
elif cpu_family.startswith('arm')
  qcommon_src += 'code/qcommon/vm_armv7l.c'
elif cpu_family in ['ppc64le', 'ppc64']
  qcommon_src += 'code/qcommon/vm_powerpc.c'
endif

platform_asm_src = []
client_asm_src = []
if win32 and cc.get_id() == 'msvc' and cpu_family == 'x86_64'
  message('Meson MSVC x64 builds use portable C fallbacks instead of MASM helpers; behavior stays the same, but hot-path setjmp/audio mixing optimizations are disabled for this build path')
  add_project_arguments('-DQ3_NO_CUSTOM_SETJMP', language: ['c', 'cpp'])
elif cpu_family == 'x86_64'
  client_asm_src += 'code/asm/snd_mix_x86_64_preprocessed.S'
endif

server_src = [
	'code/server/factory_catalog.cpp',
	'code/server/factory_rotation.cpp',
	'code/server/server_game_data.cpp',
  'code/server/sv_bot.cpp',
  'code/server/sv_ccmds.cpp',
  'code/server/sv_client.cpp',
  'code/server/sv_filter.cpp',
  'code/server/sv_game.cpp',
  'code/server/sv_init.cpp',
  'code/server/sv_main.cpp',
  'code/server/sv_net_chan.cpp',
  'code/server/sv_platform.cpp',
	'code/server/sv_factory.cpp',
  'code/server/sv_snapshot.cpp',
  'code/server/sv_world.cpp',
  'code/server/sv_zmq.cpp',
]

client_src = [
  'code/client/cl_alice.cpp',
  'code/client/cl_alice_font.cpp',
  'code/client/cl_alice_hud.cpp',
  'code/client/cl_alice_loading.cpp',
  'code/client/cl_alice_saveshot.cpp',
  'code/client/cl_alice_screen.cpp',
  'code/client/cl_alice_ui.cpp',
  'code/client/cl_avi.cpp',
  'code/client/cl_cgame.cpp',
  'code/client/cl_cin.cpp',
  'code/client/cl_console.cpp',
  'code/client/cl_fakk2_inventory.cpp',
  'code/client/cl_hud.cpp',
  'code/client/cl_input.cpp',
  'code/client/cl_jpeg.cpp',
  'code/client/cl_keys.cpp',
  'code/client/cl_main.cpp',
  'code/client/cl_mohaa.cpp',
  'code/client/cl_mohaa_cgame.cpp',
  'code/client/cl_mohaa_decals.cpp',
  'code/client/cl_mohaa_sound.cpp',
  'code/client/cl_mohaa_ui.cpp',
  'code/client/cl_workshop.cpp',
  'code/client/cl_net_chan.cpp',
  'code/client/cl_parse.cpp',
  'code/client/cl_q3test.cpp',
  'code/client/cl_q3test_cgame.cpp',
  'code/client/cl_q3test_menus.cpp',
  'code/client/cl_q3test_ui.cpp',
  'code/client/cl_scrn.cpp',
  'code/client/cl_ui.cpp',
  'code/client/cl_webpak.cpp',
  'code/client/cl_webui.cpp',
  'code/client/awesomium_backend_win32.cpp',
  'code/client/audio/snd_main.cpp',
  'code/client/audio/snd_alice.cpp',
  'code/client/audio/legacy/snd_adpcm.cpp',
  'code/client/audio/legacy/snd_dma.cpp',
  'code/client/audio/legacy/snd_mem.cpp',
  'code/client/audio/legacy/snd_mix.cpp',
  'code/client/audio/legacy/snd_wavelet.cpp',
  'code/client/audio/codecs/snd_codec_wav.cpp',
  'code/client/audio/codecs/snd_codec.cpp',
  'code/client/audio/openal/AudioSystem.cpp',
]

botlib_src = [
  'code/botlib/be_aas_bspq3.cpp',
  'code/botlib/be_aas_cluster.cpp',
  'code/botlib/be_aas_debug.cpp',
  'code/botlib/be_aas_entity.cpp',
  'code/botlib/be_aas_file.cpp',
  'code/botlib/be_aas_main.cpp',
  'code/botlib/be_aas_move.cpp',
  'code/botlib/be_aas_optimize.cpp',
  'code/botlib/be_aas_reach.cpp',
  'code/botlib/be_aas_route.cpp',
  'code/botlib/be_aas_routealt.cpp',
  'code/botlib/be_aas_sample.cpp',
  'code/botlib/be_ai_char.cpp',
  'code/botlib/be_ai_chat.cpp',
  'code/botlib/be_ai_gen.cpp',
  'code/botlib/be_ai_goal.cpp',
  'code/botlib/be_ai_move.cpp',
  'code/botlib/be_ai_weap.cpp',
  'code/botlib/be_ai_weight.cpp',
  'code/botlib/be_ea.cpp',
  'code/botlib/be_interface.cpp',
  'code/botlib/l_crc.cpp',
  'code/botlib/l_libvar.cpp',
  'code/botlib/l_log.cpp',
  'code/botlib/l_memory.cpp',
  'code/botlib/l_precomp.cpp',
  'code/botlib/l_script.cpp',
  'code/botlib/l_struct.cpp',
]

renderer_common_src = [
  'code/renderercommon/tr_font.c',
  'code/renderercommon/tr_font_stash.c',
  'code/renderercommon/tr_image_bmp.c',
  'code/renderercommon/tr_image_ftx.c',
  'code/renderercommon/tr_image_jpg.c',
  'code/renderercommon/tr_image_pcx.c',
  'code/renderercommon/tr_image_png.c',
  'code/renderercommon/tr_image_tga.c',
  'code/renderercommon/tr_image_wal.c',
  'code/renderercommon/tr_noise.c',
  'code/renderercommon/tr_mohaa_world.c',
]

renderer_gl_src = [
  'code/renderer/tr_alice.c',
  'code/renderer/tr_mohaa.c',
  'code/renderer/tr_animation.c',
  'code/renderer/tr_arb.c',
  'code/renderer/tr_backend.c',
  'code/renderer/tr_bsp.c',
  'code/renderer/tr_cmds.c',
  'code/renderer/tr_curve.c',
  'code/renderer/tr_flares.c',
  'code/renderer/tr_image.c',
  'code/renderer/tr_init.c',
  'code/renderer/tr_light.c',
  'code/renderer/tr_main.c',
  'code/renderer/tr_marks.c',
  'code/renderer/tr_mesh.c',
  'code/renderer/tr_model_iqm.c',
  'code/renderer/tr_model.c',
  'code/renderer/tr_scene.c',
  'code/renderer/tr_shade_calc.c',
  'code/renderer/tr_shade.c',
  'code/renderer/tr_shader.c',
  'code/renderer/tr_shadows.c',
  'code/renderer/tr_sky.c',
  'code/renderer/tr_surface.c',
  'code/renderer/tr_vbo.c',
  'code/renderer/tr_world.c',
]

renderer_glx_src = [
  'code/renderercommon/tr_alice_tiki.cpp',
  'code/rendererglx/glx_caps.cpp',
  'code/rendererglx/glx_debug.cpp',
  'code/rendererglx/glx_draw.cpp',
  'code/rendererglx/glx_executor.cpp',
  'code/rendererglx/glx_material.cpp',
  'code/rendererglx/glx_module.cpp',
  'code/rendererglx/glx_post_shader.cpp',
  'code/rendererglx/glx_postprocess.cpp',
  'code/rendererglx/glx_profiler.cpp',
  'code/rendererglx/glx_static_world.cpp',
  'code/rendererglx/glx_stream.cpp',
]

renderer_vk_src = [
  'code/renderercommon/tr_alice_tiki.cpp',
  'code/renderervk/tr_alice.c',
  'code/renderervk/tr_mohaa.c',
  'code/renderervk/tr_animation.c',
  'code/renderervk/tr_backend.c',
  'code/renderervk/tr_bsp.c',
  'code/renderervk/tr_cmds.c',
  'code/renderervk/tr_curve.c',
  'code/renderervk/tr_image.c',
  'code/renderervk/tr_init.c',
  'code/renderervk/tr_light.c',
  'code/renderervk/tr_main.c',
  'code/renderervk/tr_marks.c',
  'code/renderervk/tr_mesh.c',
  'code/renderervk/tr_model_iqm.c',
  'code/renderervk/tr_model.c',
  'code/renderervk/tr_scene.c',
  'code/renderervk/tr_shade_calc.c',
  'code/renderervk/tr_shade.c',
  'code/renderervk/tr_shader.c',
  'code/renderervk/tr_shadows.c',
  'code/renderervk/tr_sky.c',
  'code/renderervk/tr_surface.c',
  'code/renderervk/tr_world.c',
  'code/renderervk/vk_flares.c',
  'code/renderervk/vk_vbo.c',
  'code/renderervk/vk.c',
]

# RTX remains a separate renderer while sharing Vulkan's file contract. Keep
# this derived list so Vulkan-family source drift is visible at configure time.
renderer_rtx_src = []
foreach renderer_vk_file : renderer_vk_src
  renderer_rtx_src += renderer_vk_file.replace('code/renderervk/', 'code/rendererrtx/')
endforeach

client_deps = []
server_deps = []
renderer_deps = []
copy_runtime_files = []

thread_dep = dependency('threads')
client_deps += thread_dep
server_deps += thread_dep

# The pak reader retains its filesystem contract but uses maintained DEFLATE.
# Keep the fallback in the existing wrap instead of embedding another decoder.
zlib_dep = dependency('zlib', required: true, fallback: 'zlib')
client_deps += zlib_dep
server_deps += zlib_dep
renderer_deps += zlib_dep

fontstash_dep = dependency('', required: false)
if build_client
  fontstash_dep = dependency('fontstash',
    required: true,
    fallback: 'fontstash',
  )
endif
if fontstash_dep.found()
  # Retail Quake Live's retained host-text lane uses the 2014 FontStash/STB
  # implementation. Keep it separate from the classic FreeType fontInfo_t lane.
  common_c_args += ['-DBUILD_FONTSTASH']
  renderer_deps += fontstash_dep
  # Static-renderer builds extract renderer objects into the client target.
  client_deps += fontstash_dep
endif

freetype_dep = dependency('', required: false)
if build_client
  freetype_dep = dependency('freetype2',
    required: true,
    fallback: 'freetype2',
    default_options: quiet_fallback_defaults + [
      'brotli=disabled',
      'bzip2=disabled',
      'harfbuzz=disabled',
      'png=disabled',
      'zlib=disabled',
    ],
  )
endif
if freetype_dep.found()
  common_c_args += ['-DBUILD_FREETYPE']
  renderer_deps += freetype_dep
  # Static-renderer builds extract renderer objects into the client target.
  client_deps += freetype_dep
endif

libm = cc.find_library('m', required: false)
if libm.found()
  client_deps += libm
  server_deps += libm
  renderer_deps += libm
endif

if not win32
  libdl = cc.find_library('dl', required: false)
  if libdl.found()
    client_deps += libdl
    server_deps += libdl
    renderer_deps += libdl
  endif
endif

if win32
  foreach lib : ['winmm', 'comctl32', 'ws2_32', 'gdi32', 'ole32', 'psapi', 'shell32', 'user32', 'advapi32']
    found_lib = cc.find_library(lib, required: true)
    client_deps += found_lib
    server_deps += found_lib
  endforeach
endif

openal_dep = dependency('', required: false)
if build_client or get_option('audio-tests')
  openal_dep = dependency('openal',
    method: 'pkg-config',
    required: true,
    fallback: 'openal-soft',
    default_options: openal_fallback_defaults,
  )
endif
openal_header_dep = dependency('', required: false)
openal_source = 'disabled'
if openal_dep.found()
  openal_header_dep = openal_dep.partial_dependency(compile_args: true, includes: true)
  openal_source = openal_dep.type_name() == 'internal' ? 'subproject headers' : 'system headers'
  client_deps += openal_header_dep
endif

sdl_opt = get_option('sdl')
sdl_dep = dependency('', required: false)
sdl_source = 'disabled'
if build_client and not sdl_opt.disabled()
  sdl_dep = dependency('sdl3',
    version: '>=3.2.0',
    required: false,
    fallback: 'sdl3',
    default_options: quiet_fallback_defaults,
  )
  if sdl_dep.found()
    sdl_source = sdl_dep.type_name() == 'internal' ? 'subproject' : 'system'
  endif
endif

if build_client and sdl_opt.enabled() and not sdl_dep.found()
  error('SDL3 was enabled but no SDL3 dependency was found')
endif

# Cocoa window, input, audio, and Vulkan-surface integration are provided by
# SDL3.  Falling through to the X11 backend on Darwin produces a configuration
# that can never build, so reject it at configure time with a useful diagnosis.
if darwin and build_client and not sdl_dep.found()
  error('macOS client builds require SDL3; configure with -Dsdl=enabled')
endif
use_sdl = build_client and not sdl_opt.disabled() and sdl_dep.found()

if use_sdl
  client_deps += sdl_dep
  common_c_args += ['-DSDL_FUNCTION_POINTER_IS_VOID_POINTER=1', '-DUSE_SDL_SYSCON=1']
  common_cpp_args += ['-DSDL_FUNCTION_POINTER_IS_VOID_POINTER=1', '-DUSE_SDL_SYSCON=1']
endif

if darwin and build_client
  client_deps += dependency('appleframeworks', modules: 'AppKit')
endif

curl_opt = get_option('curl')
use_curl = false
# Windows has no system libcurl to resolve at runtime (the engine would look for
# libcurl-3.dll and nothing ships it), so 'auto' links libcurl there and keeps
# runtime resolution on platforms with a system libcurl.
curl_dlopen_opt = get_option('curl-dlopen')
use_curl_dlopen = curl_dlopen_opt == 'true' or (curl_dlopen_opt == 'auto' and not win32)
curl_dep = dependency('', required: false)
curl_header_dep = dependency('', required: false)
curl_c_args = []
curl_source = 'disabled'

if build_client and not curl_opt.disabled()
  curl_dep = dependency('libcurl',
    required: curl_opt.enabled(),
    fallback: 'curl',
    default_options: curl_fallback_defaults,
  )

  if curl_dep.found()
    use_curl = true
    curl_source = curl_dep.type_name() == 'internal' ? 'subproject' : 'system'
    curl_header_dep = curl_dep.partial_dependency(compile_args: true, includes: true)
    if use_curl_dlopen
      client_deps += curl_header_dep
    else
      client_deps += curl_dep
    endif
    server_deps += curl_header_dep
  endif
endif

if use_curl
  client_src += 'code/client/cl_curl.cpp'
  common_c_args += ['-DUSE_CURL'] + curl_c_args
  common_cpp_args += ['-DUSE_CURL'] + curl_c_args
  if use_curl_dlopen
    common_c_args += ['-DUSE_CURL_DLOPEN']
    common_cpp_args += ['-DUSE_CURL_DLOPEN']
  endif
endif

jpeg_dep = dependency('', required: false)
jpeg_source = 'disabled'
if build_client
  jpeg_force_system = get_option('system-jpeg')
  if jpeg_force_system
    jpeg_dep = dependency('libjpeg', required: false)
    if not jpeg_dep.found()
      jpeg_dep = dependency('jpeg', required: true)
    endif
    jpeg_source = 'system'
  else
    jpeg_dep = dependency('libjpeg',
      required: true,
      fallback: 'libjpeg-turbo',
      default_options: jpeg_fallback_defaults,
    )
    jpeg_source = jpeg_dep.type_name() == 'internal' ? 'subproject' : 'system'
  endif
  client_deps += jpeg_dep
  common_c_args += ['-DUSE_SYSTEM_JPEG']
  common_cpp_args += ['-DUSE_SYSTEM_JPEG']
endif

if build_client and get_option('ogg-vorbis')
  vorbisfile_dep = dependency('vorbisfile',
    required: false,
    fallback: 'vorbis',
    default_options: vorbis_fallback_defaults,
  )
  if not vorbisfile_dep.found()
    error('Ogg Vorbis support was enabled but vorbisfile was not found')
  endif
  client_src += 'code/client/audio/codecs/snd_codec_ogg.cpp'
  client_deps += vorbisfile_dep
  common_c_args += ['-DUSE_OGG_VORBIS']
  common_cpp_args += ['-DUSE_OGG_VORBIS']
  ogg_vorbis_source = vorbisfile_dep.type_name() == 'internal' ? 'subproject' : 'system'
else
  ogg_vorbis_source = 'disabled'
endif

if build_client and get_option('mp3')
  # Elite Force's sounds and music and American McGee's Alice's music are
  # MPEG-1 Layer III; minimp3 is CC0.
  minimp3_dep = dependency('minimp3', required: true, fallback: 'minimp3')
  client_src += 'code/client/audio/codecs/snd_codec_mp3.cpp'
  client_deps += minimp3_dep
  common_c_args += ['-DUSE_CODEC_MP3']
  common_cpp_args += ['-DUSE_CODEC_MP3']
  mp3_source = minimp3_dep.type_name() == 'internal' ? 'subproject' : 'system'
else
  mp3_source = 'disabled'
endif

# Profiles with a built-in icon remake under assets/icons/games, in the order
# scripts/generate_game_icons.py assigns their Windows resource ids.
game_icon_keys = ['quake3', 'quakelive', 'rtcw-sp', 'rtcw-mp', 'wolfet', 'stvef-sp',
  'stvef-mp', 'q3-ihv', 'jka-sp', 'jka-mp', 'jk2-sp', 'jk2-mp', 'alice', 'fakk2', 'mohaa',
  'q3test']

if win32
  win_resource = []
  win_client_resource = []
  if cc.get_argument_syntax() == 'gcc'
    rc_prog = find_program('windres', 'rc', 'llvm-rc', required: false)
  else
    rc_prog = find_program('rc', 'llvm-rc', 'windres', required: false)
  endif
  # Neither resource compiler emits a depfile, so the files the script pulls in
  # have to be listed explicitly. Without this an edit to the DPI manifest, the
  # icon, or the version header leaves the previous resource blob linked in and
  # the change appears to have no effect until the build tree is wiped.
  win_resource_deps = files(
    'code/win32/q3.manifest',
    'code/win32/resource.h',
    'assets/icons/fntech3.ico',
    'version/fnql_version.h',
    'version/fntech3_version.h',
  )
  # The client's script wraps win_resource.rc and adds every game's icon; one
  # script keeps the compiler's RT_ICON numbering unique across all groups.
  win_client_resource_deps = win_resource_deps + files(
    'code/win32/win_resource.rc',
    'code/win32/win_game_icons.rc',
  )
  foreach game_icon : game_icon_keys
    win_client_resource_deps += files('assets/icons/games' / game_icon / game_icon + '.ico')
  endforeach
  if rc_prog.found()
    if rc_prog.full_path().to_lower().contains('windres')
      win_resource = custom_target('win_resource',
        input: 'code/win32/win_resource.rc',
        output: 'win_resource.o',
        depend_files: win_resource_deps,
        command: [rc_prog, '-i', '@INPUT@', '-o', '@OUTPUT@', '--include-dir', meson.project_source_root()],
      )
      win_client_resource = custom_target('win_client_resource',
        input: 'code/win32/win_client_resource.rc',
        output: 'win_client_resource.o',
        depend_files: win_client_resource_deps,
        command: [rc_prog, '-i', '@INPUT@', '-o', '@OUTPUT@', '--include-dir', meson.project_source_root()],
      )
    else
      win_resource = custom_target('win_resource',
        input: 'code/win32/win_resource.rc',
        output: 'win_resource.res',
        depend_files: win_resource_deps,
        command: [rc_prog, '/fo', '@OUTPUT@', '/I', meson.project_source_root(), '@INPUT@'],
      )
      win_client_resource = custom_target('win_client_resource',
        input: 'code/win32/win_client_resource.rc',
        output: 'win_client_resource.res',
        depend_files: win_client_resource_deps,
        command: [rc_prog, '/fo', '@OUTPUT@', '/I', meson.project_source_root(), '@INPUT@'],
      )
    endif
  else
    warning('No Windows resource compiler found; Meson targets will build without win_resource.rc')
  endif
  q3_platform_src = [
    'code/win32/win_main.cpp',
    'code/win32/win_shared.cpp',
    'code/win32/win_syscon.cpp',
  ]
  # Both console executables carry the active game's remade icon. The engine
  # icon remains first, preserving their on-disk identity in Explorer.
  client_resource_src = win_client_resource
  server_resource_src = win_client_resource
  if build_client and use_sdl
    q3_ui_src = [
      'code/sdl/sdl_gamma.cpp',
      'code/sdl/sdl_glimp.cpp',
      'code/sdl/sdl_input.cpp',
      'code/sdl/sdl_snd.cpp',
    ]
  elif build_client
    q3_ui_src = [
      'code/win32/win_gamma.cpp',
      'code/win32/win_glimp.cpp',
      'code/win32/win_input.cpp',
      'code/win32/win_minimize.cpp',
      'code/win32/win_snd.cpp',
      'code/win32/win_wndproc.cpp',
    ]
    if use_opengl_api
      q3_ui_src += 'code/win32/win_qgl.cpp'
    endif
    if use_vulkan_api
      q3_ui_src += 'code/win32/win_qvk.cpp'
    endif
  endif

else
  q3_platform_src = [
    'code/unix/unix_main.cpp',
    'code/unix/unix_shared.cpp',
    'code/unix/unix_syscon.cpp',
    'code/unix/linux_signals.cpp',
  ]
  # Without Windows resources the client links the game icons' PNG frames.
  client_resource_src = ['code/platform/game_icon_data.cpp']
  server_resource_src = []
  if build_client and use_sdl
    q3_ui_src = [
      'code/sdl/sdl_gamma.cpp',
      'code/sdl/sdl_glimp.cpp',
      'code/sdl/sdl_input.cpp',
      'code/sdl/sdl_snd.cpp',
    ]
    if darwin
      q3_ui_src += 'code/sdl/sdl_macos_window.m'
    endif
  elif build_client
    x11_dep = dependency('x11', required: true)
    client_deps += x11_dep
    q3_ui_src = [
      'code/unix/linux_glimp.cpp',
      'code/unix/linux_snd.cpp',
      'code/unix/x11_dga.cpp',
      'code/unix/x11_randr.cpp',
      'code/unix/x11_vidmode.cpp',
    ]
    if linux
      q3_ui_src += 'code/unix/linux_joystick.cpp'
    endif
    if use_opengl_api
      q3_ui_src += 'code/unix/linux_qgl.cpp'
    endif
    if use_vulkan_api
      q3_ui_src += 'code/unix/linux_qvk.cpp'
    endif
  endif
endif

foreach runtime_file : copy_runtime_files
  configure_file(
    input: runtime_file,
    output: runtime_file.split('/')[-1],
    copy: true,
    install: true,
    install_dir: '.',
  )
endforeach

botlib = static_library('botlib', botlib_src,
  include_directories: include_dirs,
  cpp_args: common_cpp_args + ['-DBOTLIB'],
  install: false,
)
botlib_objects = [botlib.extract_all_objects(recursive: false)]

# Medal of Honor: Allied Assault's model definitions (TIKI), skeletal models
# and alias lists: OpenMoHAA's code (GPL-2.0-or-later), kept under code/mohaa
# with its upstream names and headers; see code/mohaa/README.md. Its sources
# include "q_shared.h" and "qcommon.h" by bare name, so code/mohaa/qcommon
# comes first on their include path. The warnings silenced here are the
# upstream code's own style (shadowed members, sign comparisons, unused
# locals), not defects fnTech3 introduced.
mohaa_src = [
  'code/mohaa/corepp/con_set.cpp',
  'code/mohaa/corepp/mem_blockalloc.cpp',
  'code/mohaa/corepp/mem_tempalloc.cpp',
  'code/mohaa/corepp/str.cpp',
  'code/mohaa/corepp/tiki_main.cpp',
  'code/mohaa/corepp/tiki_script.cpp',
  'code/mohaa/qcommon/alias.c',
  'code/mohaa/qcommon/mohaa_q_shared.c',
  'code/mohaa/skeletor/bonetable.cpp',
  'code/mohaa/skeletor/skeletor.cpp',
  'code/mohaa/skeletor/skeletor_imports.cpp',
  'code/mohaa/skeletor/skeletor_loadanimation.cpp',
  'code/mohaa/skeletor/skeletor_model_files.cpp',
  'code/mohaa/skeletor/skeletor_utilities.cpp',
  'code/mohaa/skeletor/skeletorbones.cpp',
  'code/mohaa/skeletor/tokenizer.cpp',
  'code/mohaa/tiki/tiki_anim.cpp',
  'code/mohaa/tiki/tiki_cache.cpp',
  'code/mohaa/tiki/tiki_commands.cpp',
  'code/mohaa/tiki/tiki_files.cpp',
  'code/mohaa/tiki/tiki_frame.cpp',
  'code/mohaa/tiki/tiki_imports.cpp',
  'code/mohaa/tiki/tiki_mesh.cpp',
  'code/mohaa/tiki/tiki_parse.cpp',
  'code/mohaa/tiki/tiki_skel.cpp',
  'code/mohaa/tiki/tiki_surface.cpp',
  'code/mohaa/tiki/tiki_tag.cpp',
  'code/mohaa/tiki/tiki_utility.cpp',
  'code/mohaa/mohaa_engine.cpp',
  'code/mohaa/mohaa_system86.cpp',
  'code/mohaa/mohaa_render_api.cpp',
  'code/mohaa/mohaa_tiki_api.cpp',
]
if cc.get_id() == 'msvc'
  mohaa_quiet = ['/wd4018', '/wd4101', '/wd4189', '/wd4305', '/wd4389', '/wd4456', '/wd4457',
    '/wd4458', '/wd4459', '/wd4701']
  mohaa_c_quiet = mohaa_quiet
else
  mohaa_quiet = cpp.get_supported_arguments(['-Wno-shadow', '-Wno-sign-compare',
    '-Wno-unused-variable', '-Wno-unused-but-set-variable', '-Wno-unused-parameter',
    '-Wno-maybe-uninitialized', '-Wno-class-memaccess', '-Wno-missing-field-initializers',
    '-Wno-implicit-fallthrough', '-Wno-unused-function', '-Wno-deprecated-copy',
    '-Wno-parentheses', '-Wno-misleading-indentation', '-Wno-format-truncation',
    '-Wno-stringop-truncation', '-Wno-type-limits', '-Wno-empty-body', '-Wno-unused-value',
    '-Wno-reorder', '-Wno-char-subscripts', '-Wno-extra', '-Wno-pedantic'])
  mohaa_c_quiet = cc.get_supported_arguments(['-Wno-shadow', '-Wno-sign-compare',
    '-Wno-unused-variable', '-Wno-unused-but-set-variable', '-Wno-unused-parameter',
    '-Wno-maybe-uninitialized', '-Wno-missing-field-initializers', '-Wno-implicit-fallthrough',
    '-Wno-unused-function', '-Wno-parentheses', '-Wno-format-truncation',
    '-Wno-stringop-truncation', '-Wno-strict-prototypes', '-Wno-pointer-sign'])
endif
mohaa_lib = static_library('mohaa_tiki', mohaa_src,
  include_directories: include_directories('code/mohaa/qcommon', 'code'),
  c_args: common_c_args + mohaa_c_quiet,
  cpp_args: common_cpp_args + mohaa_quiet,
  install: false,
)
mohaa_objects = [mohaa_lib.extract_all_objects(recursive: false)]

renderer_base_src = renderer_common_src + ['code/qcommon/q_shared.c', 'code/qcommon/puff.c', 'code/qcommon/q_math.c'] + platform_asm_src
renderer_targets = []
renderer_module_type = darwin ? 'shared_module' : 'shared_library'

if build_client and renderer_dlopen
  renderer_c_args = common_c_args + ['-DUSE_RENDERER_DLOPEN']
  renderer_cpp_args = common_cpp_args + ['-DUSE_RENDERER_DLOPEN']

  if 'glx' in enabled_renderers
    renderer_targets += build_target(renderer_prefix + '_glx_' + renderer_arch,
      renderer_gl_src + renderer_glx_src + renderer_base_src,
      target_type: renderer_module_type,
      include_directories: include_dirs,
      dependencies: renderer_deps,
      c_args: renderer_c_args + ['-DRENDERER_CLASSIC', '-DRENDERER_GLX'],
      cpp_args: renderer_cpp_args + ['-DRENDERER_GLX'],
      link_args: dll_link_args,
      name_prefix: '',
      gnu_symbol_visibility: 'hidden',
      install: true,
      install_dir: '.',
    )
  endif

  if 'vk' in enabled_renderers
    renderer_targets += build_target(renderer_prefix + '_vk_' + renderer_arch,
      renderer_vk_src + renderer_base_src,
      target_type: renderer_module_type,
      include_directories: include_dirs,
      dependencies: renderer_deps,
      c_args: renderer_c_args + ['-DRENDERER_VULKAN'],
      cpp_args: renderer_cpp_args,
      link_args: dll_link_args,
      name_prefix: '',
      gnu_symbol_visibility: 'hidden',
      install: true,
      install_dir: '.',
    )
  endif

  if 'rtx' in enabled_renderers
    renderer_targets += build_target(renderer_prefix + '_rtx_' + renderer_arch,
      renderer_rtx_src + renderer_base_src,
      target_type: renderer_module_type,
      include_directories: include_dirs,
      dependencies: renderer_deps,
      c_args: renderer_c_args + ['-DRENDERER_RTX'],
      cpp_args: renderer_cpp_args,
      link_args: dll_link_args,
      name_prefix: '',
      gnu_symbol_visibility: 'hidden',
      install: true,
      install_dir: '.',
    )
  endif

endif

static_renderer_objects = []
if build_client and not renderer_dlopen
  if renderer_default == 'glx'
    renderer_glx_static_c_args = common_c_args + [
      '-DRENDERER_CLASSIC',
      '-DRENDERER_GLX',
      '-DRE_DrawScaledText=R_GLX_DrawScaledText',
      '-DRE_MeasureScaledText=R_GLX_MeasureScaledText',
    ]
    static_renderer = static_library('renderer_static_glx',
      renderer_gl_src + renderer_glx_src + renderer_common_src,
      include_directories: include_dirs,
      dependencies: renderer_deps,
      c_args: renderer_glx_static_c_args,
      cpp_args: common_cpp_args + ['-DRENDERER_GLX'],
      install: false,
    )
    static_renderer_objects = [static_renderer.extract_all_objects(recursive: false)]
  elif renderer_default == 'vk'
    renderer_vk_static_c_args = common_c_args + [
      '-DRENDERER_VULKAN',
      '-DRE_DrawScaledText=R_VK_DrawScaledText',
      '-DRE_MeasureScaledText=R_VK_MeasureScaledText',
    ]
    static_renderer = static_library('renderer_static_vk',
      renderer_vk_src + renderer_common_src,
      include_directories: include_dirs,
      dependencies: renderer_deps,
      c_args: renderer_vk_static_c_args,
      cpp_args: common_cpp_args,
      install: false,
    )
    static_renderer_objects = [static_renderer.extract_all_objects(recursive: false)]
  elif renderer_default == 'rtx'
    renderer_rtx_static_c_args = common_c_args + [
      '-DRENDERER_RTX',
      '-DRE_DrawScaledText=R_RTX_DrawScaledText',
      '-DRE_MeasureScaledText=R_RTX_MeasureScaledText',
    ]
    static_renderer = static_library('renderer_static_rtx',
      renderer_rtx_src + renderer_common_src,
      include_directories: include_dirs,
      dependencies: renderer_deps,
      c_args: renderer_rtx_static_c_args,
      cpp_args: common_cpp_args,
      install: false,
    )
    static_renderer_objects = [static_renderer.extract_all_objects(recursive: false)]
  endif
endif

engine_cpp_args = common_cpp_args
if q3_ihv_development
  warning('IHV developer build: isolated dedicated native-module probes only. Client and network integration remain gated.')
  engine_cpp_args += ['-DFNTECH3_Q3_IHV_DEVELOPMENT=1']
endif
if wolfet_development
  warning('WolfET developer build: the ET client and server run with real modules for isolated probes. Release compatibility gates remain open.')
  engine_cpp_args += ['-DFNTECH3_WOLFET_DEVELOPMENT=1']
endif
if rtcw_development
  warning('RTCW developer build: incomplete SP/MP adapters are enabled for isolated probes. Release compatibility gates remain open.')
  engine_cpp_args += ['-DFNTECH3_RTCW_DEVELOPMENT=1']
endif
if stvef_development
  warning('Elite Force developer build: incomplete SP/Holomatch adapters are enabled for isolated probes. Release compatibility gates remain open.')
  engine_cpp_args += ['-DFNTECH3_STVEF_DEVELOPMENT=1']
endif
if jka_development
  warning('Jedi Academy developer build: incomplete single-player/multiplayer adapters are enabled for isolated probes. Release compatibility gates remain open.')
  engine_cpp_args += ['-DFNTECH3_JKA_DEVELOPMENT=1']
endif
if jk2_development
  warning('Jedi Outcast developer build: incomplete single-player/multiplayer adapters are enabled for isolated probes. Release compatibility gates remain open.')
  engine_cpp_args += ['-DFNTECH3_JK2_DEVELOPMENT=1']
endif
if alice_development
  warning('Alice developer build: incomplete adapters are enabled for isolated probes. Release compatibility gates remain open.')
  engine_cpp_args += ['-DFNTECH3_ALICE_DEVELOPMENT=1']
endif
if fakk2_development
  warning('F.A.K.K.2 developer build: incomplete adapters are enabled for isolated probes. Release compatibility gates remain open.')
  engine_cpp_args += ['-DFNTECH3_FAKK2_DEVELOPMENT=1']
endif
if mohaa_development
  warning('Medal of Honor developer build: incomplete adapters are enabled for isolated probes. Release compatibility gates remain open.')
  engine_cpp_args += ['-DFNTECH3_MOHAA_DEVELOPMENT=1']
endif
if q3test_development
  warning('Q3Test developer build: incomplete 1.02-1.08 adapters are enabled for isolated Windows x86 probes. Release compatibility gates remain open.')
  engine_cpp_args += ['-DFNTECH3_Q3TEST_DEVELOPMENT=1']
endif

client_c_args = common_c_args
client_cpp_args = engine_cpp_args
if linux and build_client and not use_sdl
  client_cpp_args += ['-DUSE_JOYSTICK']
endif
if client_asm_src.length() > 0 and cc.get_id() != 'msvc'
  client_c_args += ['-DELF']
endif

runtime_build_targets = renderer_targets

if build_client and linux
  foreach icon_size : ['16', '24', '32', '48', '64', '128', '256', '512', '1024']
    install_data('assets/icons/fntech3-' + icon_size + '.png',
      install_dir: get_option('datadir') / 'icons/hicolor' / (icon_size + 'x' + icon_size) / 'apps',
      rename: 'fntech3.png',
    )
  endforeach
  # This portable distribution needs the user's installation/profile arguments;
  # provide a launcher template without registering an unconfigured launcher.
  install_data('assets/icons/fntech3.desktop', install_dir: get_option('datadir') / 'fntech3')
  # Each game's icon is themed as fntech3-<profile>; its launcher template sets
  # the matching application id and, like the engine's, is not registered.
  foreach game_icon : game_icon_keys
    foreach icon_size : ['16', '22', '24', '32', '48', '64', '96', '128', '256', '512']
      install_data('assets/icons/games' / game_icon / game_icon + '-' + icon_size + '.png',
        install_dir: get_option('datadir') / 'icons/hicolor' / (icon_size + 'x' + icon_size) / 'apps',
        rename: 'fntech3-' + game_icon + '.png',
      )
    endforeach
    install_data('assets/icons/games' / game_icon / 'fntech3-' + game_icon + '.desktop',
      install_dir: get_option('datadir') / 'fntech3' / 'applications')
  endforeach
endif

if build_client
  client_exe = executable(client_binary_name,
    q3_platform_src + client_resource_src + q3_ui_src + qcommon_src + server_src + client_src + platform_asm_src + client_asm_src,
    include_directories: include_dirs,
    dependencies: client_deps,
    objects: botlib_objects + mohaa_objects + static_renderer_objects,
    c_args: client_c_args,
    cpp_args: client_cpp_args,
    link_args: link_args,
    win_subsystem: win32 ? 'windows' : 'console',
    install: true,
    install_dir: '.',
  )
  runtime_build_targets += client_exe
endif

if build_server
  server_exe = executable(server_binary_name,
    q3_platform_src + server_resource_src + qcommon_src + server_src + platform_asm_src,
    include_directories: include_dirs,
    dependencies: server_deps,
    objects: botlib_objects + mohaa_objects,
    c_args: common_c_args + ['-DDEDICATED'],
    cpp_args: engine_cpp_args + ['-DDEDICATED'],
    link_args: link_args,
    win_subsystem: win32 ? 'windows' : 'console',
    install: true,
    install_dir: '.',
  )
  runtime_build_targets += server_exe
endif

audiozonesc = executable('fnql-audiozonesc',
  'code/tools/audiozones/audiozonesc.cpp',
  include_directories: include_directories('code/client/audio/shared'),
  cpp_args: common_cpp_args,
  install: true,
  install_dir: '.',
)
runtime_build_targets += audiozonesc

stock_ql_maps_result = run_command(
  python,
  files('scripts/stock_ql_maps.py'),
  check: true,
)
stock_ql_maps = stock_ql_maps_result.stdout().strip().split('\n')
stock_audio_zone_sidecars = []
stock_global_fog_sidecars = []
foreach map_name : stock_ql_maps
  stock_audio_zone_sidecars += files('pkg/games/quakelive/baseq3/maps/' + map_name + '.azb')
  stock_global_fog_sidecars += files('pkg/games/quakelive/baseq3/maps/' + map_name + '.fog')
endforeach
root_package_assets = stock_audio_zone_sidecars + stock_global_fog_sidecars + files(
  'pkg/games/quakelive/baseq3/scripts/fnql.shader',
  'pkg/games/quakelive/baseq3/sound/fnql-weapon-sounds.sndshd',
  'pkg/games/rtcw-sp/main/fntech3-hud.json',
  'pkg/games/rtcw-mp/main/fntech3-hud.json',
  'pkg/games/fakk2/fakk/fntech3-hud.json',
  'pkg/games/alice/base/fntech3-hud.json',
  'pkg/games/wolfet/etmain/fntech3-hud.json',
  'pkg/games/stvef-sp/baseEF/fntech3-hud.json',
  'pkg/games/stvef-mp/baseEF/fntech3-hud.json',
)
stock_q3_assets_result = run_command(python, files('scripts/stock_q3_assets.py'), check: true)
foreach asset : stock_q3_assets_result.stdout().strip().split('\n')
  root_package_assets += files('pkg/games/quake3/' + asset)
endforeach
stock_rtcw_assets_result = run_command(python, files('scripts/stock_rtcw_maps.py'), check: true)
foreach asset : stock_rtcw_assets_result.stdout().strip().split('\n')
  root_package_assets += files('pkg/games/' + asset)
endforeach
stock_wolfet_assets_result = run_command(python, files('scripts/stock_wolfet_maps.py'), check: true)
foreach asset : stock_wolfet_assets_result.stdout().strip().split('\n')
  root_package_assets += files('pkg/games/' + asset)
endforeach
stock_alice_assets_result = run_command(python, files('scripts/stock_alice_maps.py'), check: true)
foreach asset : stock_alice_assets_result.stdout().strip().split('\n')
  root_package_assets += files('pkg/games/' + asset)
endforeach
stock_fakk2_assets_result = run_command(python, files('scripts/stock_fakk2_maps.py'), check: true)
foreach asset : stock_fakk2_assets_result.stdout().strip().split('\n')
  root_package_assets += files('pkg/games/' + asset)
endforeach
stock_mohaa_assets_result = run_command(python, files('scripts/stock_mohaa_maps.py'), check: true)
foreach asset : stock_mohaa_assets_result.stdout().strip().split('\n')
  root_package_assets += files('pkg/games/' + asset)
endforeach

custom_target('fntech3-pkg.fnz',
  input: root_package_assets + files(
    'scripts/build_root_archive.py',
    'scripts/fntech3_meta.py',
    'scripts/root_archive.py',
    'scripts/stock_alice_maps.py',
    'scripts/stock_fakk2_maps.py',
    'scripts/stock_mohaa_maps.py',
    'scripts/stock_q3_assets.py',
    'scripts/stock_ql_maps.py',
    'scripts/stock_rtcw_maps.py',
    'scripts/stock_stvef_maps.py',
    'scripts/stock_wolfet_maps.py',
  ),
  output: 'fntech3-pkg.fnz',
  command: [
    python,
    files('scripts/build_root_archive.py'),
    '--package-root', join_paths(meson.current_source_dir(), 'pkg'),
    '--output', '@OUTPUT@',
  ],
  depends: runtime_build_targets,
  build_always_stale: true,
  build_by_default: true,
  install: true,
  install_dir: '.',
  install_tag: 'runtime',
)

fnql_webpak_sources = files(
  'code/client/webui/index.html',
  'code/client/webui/fnql-settings.js',
  'code/client/webui/css/fnql-settings.css',
)
custom_target('fnql-web.pak',
  input: fnql_webpak_sources + files('scripts/build_webpak.py'),
  output: 'fnql-web.pak',
  command: [
    python,
    files('scripts/build_webpak.py'),
    '--source-root', join_paths(meson.current_source_dir(), 'code/client/webui'),
    '--output', '@OUTPUT@',
  ],
  build_by_default: true,
  install: true,
  install_dir: '.',
  install_tag: 'runtime',
)

if get_option('audio-tests')
  audio_zone_tests = executable('fnql_audio_zone_tests',
    'tests/audio/audiozone_runtime_tests.cpp',
    include_directories: include_directories('code/client/audio/shared'),
    cpp_args: common_cpp_args,
  )
  test('fnql_audio_zones', audio_zone_tests)

  audiozone_fixture = custom_target('audiozone_authoring_test',
    output: 'audiozone_authoring_test.azb',
    input: 'tests/audio/authoring_metadata.audiozones',
    command: [audiozonesc, '-o', '@OUTPUT@', '@INPUT@'],
    build_by_default: true,
  )
  test('fnql_audio_zone_authoring_audit', audiozonesc,
    args: ['--audit', '--strict', '--samples', '512', audiozone_fixture],
    depends: audiozone_fixture,
  )

  audio_recovery_tests = executable('fnql_audio_recovery_tests',
    'tests/audio/recovery_policy_tests.cpp',
    include_directories: include_directories('code/client/audio/shared'),
    cpp_args: common_cpp_args,
  )
  test('fnql_audio_recovery', audio_recovery_tests)

  audio_init_policy_tests = executable('fnql_audio_init_policy_tests',
    'tests/audio/init_policy_tests.cpp',
    include_directories: include_directories('code/client/audio/shared'),
    cpp_args: common_cpp_args,
  )
  test('fnql_audio_init_policy', audio_init_policy_tests)

  audio_occlusion_tests = executable('fnql_audio_occlusion_tests',
    'tests/audio/occlusion_policy_tests.cpp',
    include_directories: include_directories('code/client/audio/shared'),
    cpp_args: common_cpp_args,
  )
  test('fnql_audio_occlusion', audio_occlusion_tests)

  audio_tone_tests = executable('fnql_audio_tone_tests',
    'tests/audio/tone_policy_tests.cpp',
    include_directories: include_directories('code/client/audio/shared'),
    cpp_args: common_cpp_args,
  )
  test('fnql_audio_tone', audio_tone_tests)

  audio_voice_queue_tests = executable('fnql_audio_voice_queue_tests',
    'tests/audio/voice_queue_tests.cpp',
    include_directories: include_directories('code/client/audio/shared'),
    cpp_args: common_cpp_args,
  )
  test('fnql_audio_voice_queue', audio_voice_queue_tests)

  test('fnql_retail_sound_compat_source', python,
    args: files('tests/audio/retail_sound_compat_source_tests.py'),
  )

  test('fnql_audio_init_source', python,
    args: files('tests/audio/audio_init_source_tests.py'),
  )

  test('fnql_audio_impact_occlusion_source', python,
    args: files('tests/audio/impact_occlusion_source_tests.py'),
  )

  audio_loopback_tests = executable('fnql_audio_loopback_tests',
    'tests/audio/openal_loopback_tests.cpp',
    dependencies: client_deps + [openal_header_dep],
    cpp_args: common_cpp_args,
  )
  test('fnql_audio_loopback', audio_loopback_tests, should_fail: false)

  test('fnql_audio_zone_sweep_script', python,
    args: files('tests/audio/audio_zone_sweep_tests.py'),
  )
  # 2 to 15 s in earlier runs, past Meson's 30 s default in a full run
  # beside other builds.
  test('fnql_audio_zone_material_map', python,
    args: [files('tests/audio/audio_zone_material_map_tests.py'), audiozonesc],
    timeout: 120,
  )
endif

jka_abi_layout_tests = executable('fntech3_jka_abi_layout_tests',
  'tests/jka_abi_layout_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_jka_abi_layouts', jka_abi_layout_tests)

jka_sp_abi_tests = executable('fntech3_jka_sp_abi_tests',
  'tests/jka_sp_abi_tests.cpp', cpp_args: common_cpp_args)
test('fntech3_jka_sp_abi', jka_sp_abi_tests)
jka_sp_bridge_tests = executable('fntech3_jka_sp_game_bridge_tests',
  ['tests/jka_sp_game_bridge_tests.cpp', 'code/server/server_game_data.cpp',
   'code/compat/game_abi_adapter.c', 'tests/message_drop_trap.c'],
  c_args: common_c_args, cpp_args: common_cpp_args)
test('fntech3_jka_sp_game_bridge', jka_sp_bridge_tests)
jka_sp_state_tests = executable('fntech3_jka_sp_state_tests',
  ['tests/jka_sp_state_tests.cpp', 'code/server/server_game_data.cpp',
   'code/compat/game_abi_adapter.c'], c_args: common_c_args, cpp_args: common_cpp_args)
test('fntech3_jka_sp_state', jka_sp_state_tests)
jka_wire_tests = executable('fntech3_jka_wire_tests',
  'tests/jka_wire_tests.cpp', include_directories: include_directories('code/compat'),
  cpp_args: common_cpp_args)
test('fntech3_jka_wire', jka_wire_tests)
jka_snapshot_tests = executable('fntech3_jka_snapshot_tests',
  'tests/jka_snapshot_tests.cpp', cpp_args: common_cpp_args)
test('fntech3_jka_snapshot', jka_snapshot_tests)
jka_ui_tests = executable('fntech3_jka_ui_tests',
  'tests/jka_ui_tests.cpp', cpp_args: common_cpp_args)
test('fntech3_jka_ui', jka_ui_tests)
jka_font_tests = executable('fntech3_jka_font_tests',
  'tests/jka_font_tests.cpp', cpp_args: common_cpp_args)
test('fntech3_jka_font', jka_font_tests)
jka_font_host_tests = executable('fntech3_jka_font_host_tests',
  'tests/jka_font_host_tests.cpp', cpp_args: common_cpp_args)
test('fntech3_jka_font_host', jka_font_host_tests)
foreach jka_test : ['client', 'string_package', 'string_host', 'render', 'sp_player_abi', 'sp_model_precache', 'sp_animation_cfg']
  jka_executable = executable('fntech3_jka_' + jka_test + '_tests',
    'tests/jka_' + jka_test + '_tests.cpp', cpp_args: common_cpp_args)
  test('fntech3_jka_' + jka_test, jka_executable)
endforeach
# The Jedi Academy tests given jka_test_timeout, most of which compile native
# fixtures, ran past Meson's 30 s default in a full run beside other builds;
# fntech3_jka_pose_host took 85 s there and 24 s alone.
jka_test_timeout = 180
test('fntech3_jka_cgame_host', python, args: files('tests/jka_cgame_host_tests.py'))
test('fntech3_jka_cgame_ghoul2', python, args: files('tests/jka_cgame_ghoul2_tests.py'))
test('fntech3_jka_cgame_render', python, args: files('tests/jka_cgame_render_tests.py'),
  timeout: jka_test_timeout)
test('fntech3_jka_cgame_attachment', python, args: files('tests/jka_cgame_attachment_tests.py'),
  timeout: jka_test_timeout)
test('fntech3_jka_server_attachment', python, args: files('tests/jka_server_attachment_tests.py'))
test('fntech3_jka_default_skeleton', python, args: files('tests/jka_default_skeleton_tests.py'),
  timeout: jka_test_timeout)
test('fntech3_jka_cgame_tag', python, args: files('tests/jka_cgame_tag_tests.py'),
  timeout: jka_test_timeout)
jka_skin_tests = executable('fntech3_jka_skin_tests',
  'tests/jka_skin_tests.cpp', cpp_args: common_cpp_args)
test('fntech3_jka_skin', jka_skin_tests)
jka_lightstyle_tests = executable('fntech3_jka_lightstyle_tests',
  'tests/jka_lightstyle_tests.cpp', cpp_args: common_cpp_args)
test('fntech3_jka_lightstyle', jka_lightstyle_tests)
test('fntech3_jka_music_host', python, args: files('tests/jka_music_host_tests.py'))
jka_music_preload_tests = executable('fntech3_jka_music_preload_tests',
  'tests/jka_music_preload_tests.cpp', cpp_args: common_cpp_args)
test('fntech3_jka_music_preload', jka_music_preload_tests)
jka_ambient_tests = executable('fntech3_jka_ambient_sets_tests',
  'tests/jka_ambient_sets_tests.cpp', cpp_args: common_cpp_args)
test('fntech3_jka_ambient_sets', jka_ambient_tests)
test('fntech3_jka_cgame_ambient', python, args: files('tests/jka_cgame_ambient_tests.py'))
jka_effects_tests = executable('fntech3_jka_effects_tests',
  'tests/jka_effects_tests.cpp', include_directories: include_directories('.'), cpp_args: common_cpp_args)
test('fntech3_jka_effects', jka_effects_tests)
test('fntech3_jka_cgame_effects', python, args: files('tests/jka_cgame_effects_tests.py'))
test('fntech3_jka_effect_play', python, args: files('tests/jka_effect_play_tests.py'),
  timeout: jka_test_timeout)
test('fntech3_audio_codec_profile', python, args: files('tests/audio_codec_profile_tests.py'))
test('fntech3_jka_render_host', python, args: files('tests/jka_render_host_tests.py'))
test('fntech3_jka_ui_binding', python, args: files('tests/jka_ui_binding_tests.py'))
test('fntech3_jka_ui_metadata', python, args: files('tests/jka_ui_metadata_tests.py'))
test('fntech3_jka_ui_ghoul2', python, args: files('tests/jka_ui_ghoul2_tests.py'))
test('fntech3_jka_sp_vm_export', python, args: files('tests/jka_sp_vm_export_tests.py'))
test('fntech3_common_recursive_error', python, args: files('tests/common_recursive_error_tests.py'))
test('fntech3_jka_ui_lifecycle', python, args: files('tests/jka_ui_lifecycle_tests.py'))
test('fntech3_jka_shader_files', python, args: files('tests/jka_shader_files_tests.py'))
test('fntech3_jka_shader_notc', python, args: files('tests/jka_shader_notc_tests.py'),
  timeout: jka_test_timeout)
test('fntech3_jka_tga_origin', python, args: files('tests/jka_tga_origin_tests.py'))
test('fntech3_jka_cinematic_path', python, args: files('tests/jka_cinematic_path_tests.py'))
test('fntech3_jka_ghoul2_cleanup', python, args: files('tests/jka_ghoul2_cleanup_tests.py'),
  timeout: jka_test_timeout)
test('fntech3_jka_pose_host', python, args: files('tests/jka_pose_tests.py'),
  timeout: jka_test_timeout)
test('fntech3_jka_geometry_host', python, args: files('tests/jka_geometry_tests.py'),
  timeout: jka_test_timeout)
test('fntech3_jka_mdr_host', python, args: files('tests/jka_mdr_tests.py'),
  timeout: jka_test_timeout)
test('fntech3_jka_ui_render', python, args: files('tests/jka_ui_render_tests.py'))
test('fntech3_jka_ui_cvar', python, args: files('tests/jka_ui_cvar_tests.py'))
test('fntech3_jka_handshake', python, args: files('tests/jka_handshake_tests.py'))
test('fntech3_jka_cgame_lifecycle', python, args: files('tests/jka_cgame_lifecycle_tests.py'))
test('fntech3_runtime_mdr', python, args: files('tests/runtime_mdr_tests.py'),
  timeout: jka_test_timeout)
test('fntech3_jka_parser_global', python, args: files('tests/jka_parser_global_tests.py'),
  timeout: jka_test_timeout)
test('fntech3_probe_jka_openjk_peer', python, args: files('tests/probe_jka_openjk_peer_tests.py'),
  timeout: jka_test_timeout)
test('fntech3_probe_jka_openjk_demo', python, args: files('tests/probe_jka_openjk_demo_tests.py'),
  timeout: jka_test_timeout)
test('fntech3_probe_jka_client', python, args: files('tests/probe_jka_client_tests.py'))
test('fntech3_jka_framing', python, args: files('tests/jka_framing_tests.py'))
test('fntech3_jka_demo_snapshot', python, args: files('tests/jka_demo_snapshot_tests.py'),
  timeout: jka_test_timeout)
demo_wire_profile_tests = executable('fntech3_demo_wire_profile_tests',
  'tests/demo_wire_profile_tests.cpp', cpp_args: common_cpp_args)
test('fntech3_demo_wire_profile', demo_wire_profile_tests)
jka_sp_ghoul2_tests = executable('fntech3_jka_sp_ghoul2_tests',
  'tests/jka_sp_ghoul2_tests.cpp', cpp_args: common_cpp_args)
test('fntech3_jka_sp_ghoul2', jka_sp_ghoul2_tests)
jka_pose_tests = executable('fntech3_jka_pose_tests',
  'tests/jka_pose_tests.cpp', cpp_args: common_cpp_args)
test('fntech3_jka_pose', jka_pose_tests)
jka_geometry_tests = executable('fntech3_jka_geometry_tests',
  'tests/jka_geometry_tests.cpp', cpp_args: common_cpp_args)
test('fntech3_jka_geometry', jka_geometry_tests)
jka_mdr_tests = executable('fntech3_jka_mdr_tests',
  'tests/jka_mdr_tests.cpp', cpp_args: common_cpp_args)
test('fntech3_jka_mdr', jka_mdr_tests)
raven_bsp_render_tests = executable('fntech3_raven_bsp_render_tests',
  'tests/raven_bsp_render_tests.cpp', cpp_args: common_cpp_args)
test('fntech3_raven_bsp_render', raven_bsp_render_tests)
test('fntech3_jka_distance_cull', python, args: files('tests/jka_distance_cull_tests.py'))
test('fntech3_raven_bsp_renderer_dispatch', python, args: files('tests/raven_bsp_renderer_dispatch_tests.py'))
jka_netchan_tests = executable('fntech3_jka_netchan_tests',
  ['tests/jka_netchan_tests.cpp', 'code/qcommon/net_chan.cpp',
   'code/qcommon/msg.cpp', 'code/qcommon/huffman_static.c'],
  include_directories: include_directories('code/qcommon'),
  c_args: common_c_args + ['-DDEDICATED'],
  cpp_args: common_cpp_args + ['-DDEDICATED'])
test('fntech3_jka_netchan', jka_netchan_tests)
test('fntech3_jka_actions', python, args: files('tests/jka_actions_tests.py'))

jka_roff_tests = executable('fntech3_jka_roff_tests',
  'tests/jka_roff_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_jka_roff', jka_roff_tests)

jka_icarus_tests = executable('fntech3_jka_icarus_tests',
  'tests/jka_icarus_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_jka_icarus', jka_icarus_tests)

raven_roff_server_tests = executable('fntech3_raven_roff_server_tests',
  'tests/raven_roff_server_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_raven_roff_server', raven_roff_server_tests)

jk2_abi_layout_tests = executable('fntech3_jk2_abi_layout_tests',
  'tests/jk2_abi_layout_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_jk2_abi_layouts', jk2_abi_layout_tests)

foreach jk2_test : ['handshake', 'client_adapter', 'font', 'message_codec']
  jk2_test_exe = executable('fntech3_jk2_' + jk2_test + '_tests',
    'tests/jk2_' + jk2_test + '_tests.cpp',
    cpp_args: common_cpp_args,
  )
  test('fntech3_jk2_' + jk2_test, jk2_test_exe)
endforeach

test('jk2-sp-host', python, args: files('tests/jk2_sp_host_tests.py'))

nav_graph_tests = executable('fntech3_nav_graph_tests',
  'tests/nav_graph_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_nav_graph', nav_graph_tests)

raven_ghoul2_server_tests = executable('fntech3_raven_ghoul2_server_tests',
  'tests/raven_ghoul2_server_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_raven_ghoul2_server', raven_ghoul2_server_tests)

jka_icarus_callback_tests = executable('fntech3_jka_icarus_callback_tests',
  'tests/jka_icarus_callback_tests.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_jka_icarus_callbacks', jka_icarus_callback_tests)

jka_icarus_runtime_tests = executable('fntech3_jka_icarus_runtime_tests',
  'tests/jka_icarus_runtime_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_jka_icarus_runtime', jka_icarus_runtime_tests)

jka_icarus_id_tests = executable('fntech3_jka_icarus_id_tests',
  'tests/jka_icarus_id_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_jka_icarus_ids', jka_icarus_id_tests)

jka_icarus_value_tests = executable('fntech3_jka_icarus_value_tests',
  'tests/jka_icarus_value_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_jka_icarus_values', jka_icarus_value_tests)

jka_model_tests = executable('fntech3_jka_model_tests',
  'tests/jka_model_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_jka_models', jka_model_tests)

jka_module_abi_tests = executable('fntech3_jka_module_abi_tests',
  ['tests/jka_module_abi_tests.cpp', 'code/qcommon/game_profile.cpp'],
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_jka_module_abi', jka_module_abi_tests)

jka_bsp_tests = executable('fntech3_jka_bsp_tests',
  'tests/jka_bsp_tests.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_jka_bsp', jka_bsp_tests)

alice_bsp_tests = executable('fntech3_alice_bsp_tests',
  'tests/alice_bsp_tests.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_alice_bsp', alice_bsp_tests)

alice_module_abi_tests = executable('fntech3_alice_module_abi_tests',
  ['tests/alice_module_abi_tests.cpp', 'code/qcommon/game_profile.cpp'],
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_alice_module_abi', alice_module_abi_tests)

alice_message_codec_tests = executable('fntech3_alice_message_codec_tests',
  ['tests/alice_message_codec_tests.cpp'],
  include_directories: include_directories('code/qcommon', 'code/compat'),
  cpp_args: common_cpp_args,
)
test('fntech3_alice_message_codec', alice_message_codec_tests)

fakk2_message_codec_tests = executable('fntech3_fakk2_message_codec_tests',
  ['tests/fakk2_message_codec_tests.cpp'],
  include_directories: include_directories('code/qcommon', 'code/compat'),
  cpp_args: common_cpp_args,
)
test('fntech3_fakk2_message_codec', fakk2_message_codec_tests)

alice_tan_tests = executable('fntech3_alice_tan_tests',
  'tests/alice_tan_tests.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_alice_tan', alice_tan_tests)

alice_skb_tests = executable('fntech3_alice_skb_tests',
  'tests/alice_skb_tests.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_alice_skb', alice_skb_tests)

alice_tiki_tests = executable('fntech3_alice_tiki_tests',
  'tests/alice_tiki_tests.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_alice_tiki', alice_tiki_tests)

alice_alias_tests = executable('fntech3_alice_alias_tests',
  ['tests/alice_alias_tests.cpp', 'code/qcommon/alice_alias.cpp'],
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_alice_alias', alice_alias_tests)

alice_ska_tests = executable('fntech3_alice_ska_tests',
  'tests/alice_ska_tests.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_alice_ska', alice_ska_tests)

alice_tiki_pose_tests = executable('fntech3_alice_tiki_pose_tests',
  'tests/alice_tiki_pose_tests.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_alice_tiki_pose', alice_tiki_pose_tests)

alice_tiki_lod_tests = executable('fntech3_alice_tiki_lod_tests',
  'tests/alice_tiki_lod_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_alice_tiki_lod', alice_tiki_lod_tests)

# The renderers' shared TIKI code, linked as each renderer links it: surface
# lookup and retail's animation and frame corrections.
alice_tiki_render_tests = executable('fntech3_alice_tiki_render_tests',
  [
    'tests/alice_tiki_render_tests.cpp',
    'code/renderercommon/tr_alice_tiki.cpp',
    'code/qcommon/q_math.c',
    'code/qcommon/puff.c',
  ],
  include_directories: include_directories('code/qcommon'),
  c_args: common_c_args,
  cpp_args: common_cpp_args,
  dependencies: libm.found() ? [libm] : [],
)
test('fntech3_alice_tiki_render', alice_tiki_render_tests)

alice_input_tests = executable('fntech3_alice_input_tests',
  'tests/alice_input_tests.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_alice_input', alice_input_tests)

alice_music_tests = executable('fntech3_alice_music_tests',
  'tests/alice_music_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_alice_music', alice_music_tests)

alice_sound_tests = executable('fntech3_alice_sound_tests',
  'tests/alice_sound_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_alice_sound', alice_sound_tests)

alice_ritualfont_tests = executable('fntech3_alice_ritualfont_tests',
  ['tests/alice_ritualfont_tests.cpp', 'code/qcommon/puff.c'],
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_alice_ritualfont', alice_ritualfont_tests)

alice_urc_tests = executable('fntech3_alice_urc_tests',
  ['tests/alice_urc_tests.cpp', 'code/qcommon/puff.c'],
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_alice_urc', alice_urc_tests)

alice_menu_tests = executable('fntech3_alice_menu_tests',
  ['tests/alice_menu_tests.cpp', 'code/qcommon/puff.c'],
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_alice_menu', alice_menu_tests)

alice_screen_tests = executable('fntech3_alice_screen_tests',
  ['tests/alice_screen_tests.cpp', 'code/qcommon/puff.c'],
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_alice_screen', alice_screen_tests)

alice_hud_tests = executable('fntech3_alice_hud_tests',
  ['tests/alice_hud_tests.cpp', 'code/qcommon/puff.c'],
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_alice_hud', alice_hud_tests)

alice_noise_tests = executable('fntech3_alice_noise_tests',
  'tests/alice_noise_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_alice_noise', alice_noise_tests)

alice_savegame_tests = executable('fntech3_alice_savegame_tests',
  'tests/alice_savegame_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_alice_savegame', alice_savegame_tests)

alice_snapshot_tests = executable('fntech3_alice_snapshot_tests',
  'tests/alice_snapshot_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_alice_snapshot', alice_snapshot_tests)

fakk2_module_abi_tests = executable('fntech3_fakk2_module_abi_tests',
  'tests/fakk2_module_abi_tests.cpp',
  include_directories: include_directories('code/qcommon', 'code/compat'),
  cpp_args: common_cpp_args,
)
test('fntech3_fakk2_module_abi', fakk2_module_abi_tests)

fakk2_hud_tests = executable('fntech3_fakk2_hud_tests',
  ['tests/fakk2_hud_tests.cpp', 'code/qcommon/puff.c'],
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_fakk2_hud', fakk2_hud_tests)

mohaa_module_abi_tests = executable('fntech3_mohaa_module_abi_tests',
  'tests/mohaa_module_abi_tests.cpp',
  include_directories: include_directories('code/compat'),
  cpp_args: common_cpp_args,
)
test('fntech3_mohaa_module_abi', mohaa_module_abi_tests)

mohaa_savegame_tests = executable('fntech3_mohaa_savegame_tests',
  'tests/mohaa_savegame_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_mohaa_savegame', mohaa_savegame_tests)

mohaa_load_record_tests = executable('fntech3_mohaa_load_record_tests',
  'tests/mohaa_load_record_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_mohaa_load_record', mohaa_load_record_tests)

jka_identity_tests = executable('fntech3_jka_identity_tests',
  ['tests/jka_identity_tests.cpp', 'code/qcommon/game_profile.cpp'],
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_jka_identity', jka_identity_tests)

game_profile_tests = executable('fntech3_game_profile_tests',
  [
    'tests/game_profile_tests.cpp',
    'code/qcommon/game_profile.cpp',
  ],
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_game_profiles', game_profile_tests)

steam_path_tests = executable('fntech3_steam_path_tests',
  ['tests/steam_path_tests.cpp', 'code/qcommon/game_profile.cpp'],
  cpp_args: common_cpp_args,
)
test('fntech3_steam_paths', steam_path_tests)

game_abi_layout_tests = executable('fntech3_game_abi_layout_tests',
  'tests/game_abi_layout_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_game_abi_layouts', game_abi_layout_tests)

game_abi_adapter_tests = executable('fntech3_game_abi_adapter_tests',
  [
    'tests/game_abi_adapter_tests.cpp',
    'code/compat/game_abi_adapter.c',
  ],
  include_directories: include_directories('code/qcommon', 'code/compat'),
  c_args: common_c_args,
  cpp_args: common_cpp_args,
)
test('fntech3_game_abi_adapters', game_abi_adapter_tests)

server_game_data_tests = executable('fntech3_server_game_data_tests',
  [
    'tests/server_game_data_tests.cpp',
    'code/server/server_game_data.cpp',
    'code/compat/game_abi_adapter.c',
  ],
  include_directories: include_directories('code/qcommon', 'code/compat', 'code/server'),
  c_args: common_c_args,
  cpp_args: common_cpp_args,
)
test('fntech3_server_game_data', server_game_data_tests)

q3_ihv_game_data_tests = executable('fntech3_q3_ihv_game_data_tests',
  ['tests/q3_ihv_game_data_tests.cpp', 'code/server/server_game_data.cpp',
   'code/compat/game_abi_adapter.c'],
  include_directories: include_directories('code/qcommon', 'code/compat', 'code/server'),
  c_args: common_c_args,
  cpp_args: common_cpp_args,
)
test('fntech3_q3_ihv_game_data', q3_ihv_game_data_tests)

q3_ihv_game_bridge_tests = executable('fntech3_q3_ihv_game_bridge_tests',
  ['tests/q3_ihv_game_bridge_tests.cpp', 'code/server/server_game_data.cpp',
   'code/compat/game_abi_adapter.c'],
  include_directories: include_directories('code/qcommon', 'code/compat', 'code/server'),
  c_args: common_c_args,
  cpp_args: common_cpp_args,
)
test('fntech3_q3_ihv_game_bridge', q3_ihv_game_bridge_tests)

wolfet_contract_tests = executable('fntech3_wolfet_contract_tests',
  ['tests/wolfet_contract_tests.cpp', 'code/server/server_game_data.cpp',
   'code/compat/game_abi_adapter.c'],
  include_directories: include_directories('code/qcommon', 'code/compat', 'code/server'),
  c_args: common_c_args,
  cpp_args: common_cpp_args,
)
test('fntech3_wolfet_contracts', wolfet_contract_tests)

stvef_mp3_frames_tests = executable('fntech3_stvef_mp3_frames_tests',
  'tests/stvef_mp3_frames_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_stvef_mp3_frames', stvef_mp3_frames_tests)

stvef_abi_layout_tests = executable('fntech3_stvef_abi_layout_tests',
  'tests/stvef_abi_layout_tests.cpp',
  include_directories: include_directories('code/qcommon', 'code/compat'),
  cpp_args: common_cpp_args,
)
test('fntech3_stvef_abi_layouts', stvef_abi_layout_tests)

stvef_vm_rounding_tests = executable('fntech3_stvef_vm_rounding_tests',
  'tests/stvef_vm_rounding_tests.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_stvef_vm_rounding', stvef_vm_rounding_tests)

stvef_client_wire_tests = executable('fntech3_stvef_client_wire_tests',
  'tests/stvef_client_wire_tests.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_stvef_client_wire', stvef_client_wire_tests)

stvef_render_record_tests = executable('fntech3_stvef_render_record_tests',
  'tests/stvef_render_record_tests.cpp',
  include_directories: include_directories('code/qcommon', 'code/compat', 'code/client'),
  cpp_args: common_cpp_args,
)
test('fntech3_stvef_render_records', stvef_render_record_tests)

wolfet_client_boundary_tests = executable('fntech3_wolfet_client_boundary_tests',
  'tests/wolfet_client_boundary_tests.cpp',
  include_directories: include_directories('code/qcommon', 'code/compat', 'code/client'),
  cpp_args: common_cpp_args,
)
test('fntech3_wolfet_client_boundary', wolfet_client_boundary_tests)

netchan_profile_tests = executable('fnql_netchan_profile_tests',
  'tests/netchan_profile_tests.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fnql_netchan_profiles', netchan_profile_tests)

netchan_codec_tests = executable('fnql_netchan_codec_tests',
  'tests/netchan_codec_tests.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fnql_netchan_codec', netchan_codec_tests)

ql_font_text_tests = executable('fnql_ql_font_text_tests',
  'tests/ql_font_text_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fnql_ql_font_text', ql_font_text_tests)

window_placement_tests = executable('fnql_window_placement_tests',
  'tests/window_placement_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fnql_window_placement', window_placement_tests)

# The generated frames compile on every platform so the test can check them
# against the committed PNGs; Windows also links the client's icon resources.
icon_test_sources = ['tests/application_icon_tests.cpp', 'code/qcommon/puff.c',
  'code/platform/game_icon_data.cpp']
icon_test_deps = []
icon_test_args = common_cpp_args
if win32
  icon_test_sources += win_client_resource
  icon_test_args += '-DNOMINMAX'
  foreach lib : ['shell32', 'user32', 'gdi32']
    icon_test_deps += cpp.find_library(lib)
  endforeach
endif
if use_sdl
  icon_test_deps += sdl_dep
  icon_test_args += '-DFNTECH3_ICON_TEST_SDL'
  if darwin
    icon_test_sources += 'code/sdl/sdl_macos_window.m'
    icon_test_deps += dependency('appleframeworks', modules: 'AppKit')
  endif
endif
application_icon_tests = executable('fntech3_application_icon_tests',
  icon_test_sources + ['code/qcommon/game_profile.cpp'],
  include_directories: include_directories('code/qcommon'),
  dependencies: icon_test_deps,
  cpp_args: icon_test_args,
)
test('fntech3_application_icons', application_icon_tests,
  args: [meson.current_build_dir(), meson.current_source_dir() / 'assets/icons/fntech3-128.png',
    meson.current_source_dir() / 'assets/icons/games'],
)

test('fnql_ql_font_source', python,
  args: files('tests/ql_font_source_tests.py'),
)
test('fnql_map_overbright_cap_source', python,
  args: files('tests/map_overbright_cap_source_tests.py'),
)
test('fnql_window_management_source', python,
  args: files('tests/window_management_source_tests.py'),
)
test('fnql_macos_support_source', python,
  args: files('tests/macos_support_source_tests.py'),
)

global_fog_tests = executable('fnql_global_fog_tests',
  'tests/global_fog_tests.cpp',
  'code/qcommon/q_shared.c',
  c_args: common_c_args,
  cpp_args: common_cpp_args,
)
test('fnql_global_fog', global_fog_tests,
  args: [join_paths(meson.current_source_dir(), 'pkg', 'games')],
)
test('fnql_global_fog_source', python,
  args: files('tests/global_fog_source_tests.py'),
)

menu_blur_tests = executable('fnql_menu_blur_tests',
  'tests/menu_blur_tests.cpp',
  'code/qcommon/q_shared.c',
  c_args: common_c_args,
  cpp_args: common_cpp_args,
)
test('fnql_menu_blur', menu_blur_tests)
test('fnql_menu_blur_source', python,
  args: files('tests/menu_blur_source_tests.py'),
)

levelshot_validation_tests = executable('fnql_levelshot_validation_tests',
  'tests/levelshot_validation_tests.cpp',
  'code/qcommon/q_shared.c',
  c_args: common_c_args,
  cpp_args: common_cpp_args,
)
test('fnql_levelshot_validation', levelshot_validation_tests)
test('fnql_renderer_port_safety_source', python,
  args: files('tests/renderer_port_safety_tests.py'),
)
test('fnql_untrusted_asset_bounds_source', python,
  args: files('tests/untrusted_asset_bounds_tests.py'),
)
test('fnql_surface_geometry_limits_source', python,
  args: files('tests/surface_geometry_limits_source_tests.py'),
)

bmp_loader_tests = executable('fnql_bmp_loader_tests',
  [
    'tests/bmp_loader_tests.c',
    'code/renderercommon/tr_image_bmp.c',
  ],
  include_directories: include_directories('code/qcommon', 'code/renderercommon'),
  c_args: common_c_args,
)
test('fnql_bmp_loader', bmp_loader_tests)

huffman_decoder_tests = executable('fnql_huffman_decoder_tests',
  [
    'tests/huffman_decoder_tests.c',
    'code/qcommon/huffman.c',
  ],
  include_directories: include_directories('code/qcommon'),
  c_args: common_c_args,
)
test('fnql_huffman_decoder', huffman_decoder_tests)

qvm_header_tests = executable('fnql_qvm_header_tests',
  'tests/qvm_header_tests.c',
  include_directories: include_directories('code/qcommon'),
  c_args: common_c_args,
)
test('fnql_qvm_header_bounds', qvm_header_tests)

filesystem_write_bounds_tests = executable('fnql_filesystem_write_bounds_tests',
  'tests/filesystem_write_bounds_tests.c',
  include_directories: include_directories('code/qcommon'),
  c_args: common_c_args,
)
test('fnql_filesystem_write_bounds', filesystem_write_bounds_tests)

png_loader_tests = executable('fnql_png_loader_tests',
  [
    'tests/png_loader_tests.c',
    'code/renderercommon/tr_image_png.c',
  ],
  dependencies: zlib_dep,
  include_directories: include_directories('code/qcommon', 'code/renderercommon'),
  c_args: common_c_args,
)
test('fnql_png_loader', png_loader_tests)

unzip_tests = executable('fntech3_unzip_tests',
  ['tests/unzip_tests.c', 'code/qcommon/unzip.c'],
  dependencies: zlib_dep,
  c_args: common_c_args,
)
test('fntech3_unzip', unzip_tests,
  args: [join_paths(meson.current_build_dir(), 'unzip-test.pk3')],
)

native_seh_tests = executable('fntech3_native_seh_tests',
  ['tests/native_seh_tests.c', 'code/qcommon/native_seh.c'],
  c_args: common_c_args,
)
test('fntech3_native_seh', native_seh_tests)

netchan_safety_tests = executable('fnql_netchan_safety_tests',
  'tests/netchan_safety_tests.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fnql_netchan_safety', netchan_safety_tests)

input_compat_tests = executable('fnql_input_compat_tests',
  'tests/input_compat_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fnql_input_compat', input_compat_tests)

client_cvar_compat_tests = executable('fnql_client_cvar_compat_tests',
  'tests/client_cvar_compat_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fnql_client_cvar_compat', client_cvar_compat_tests)
test('fnql_client_cvar_compat_source', python,
  args: files('tests/client_cvar_compat_source_tests.py'),
)

server_cvar_compat_tests = executable('fnql_server_cvar_compat_tests',
  'tests/server_cvar_compat_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fnql_server_cvar_compat', server_cvar_compat_tests)
test('fnql_server_cvar_compat_source', python,
  args: files('tests/server_cvar_compat_source_tests.py'),
)
test('fnql_spawn_view_angle_source', python,
  args: files('tests/spawn_view_angle_source_tests.py'),
)

message_codec_tests = executable('fnql_message_codec_tests',
  'tests/message_codec_tests.cpp',
  'tests/message_drop_trap.c',
  'code/qcommon/msg.cpp',
  'code/qcommon/huffman_static.c',
  include_directories: include_directories('code/qcommon'),
  c_args: common_c_args + ['-DDEDICATED'],
  cpp_args: common_cpp_args + ['-DDEDICATED'],
)
test('fnql_message_codec', message_codec_tests)

# Not a test: decodes retail Elite Force demos from a user installation
# through scripts/probe_stvef_demos.py.
# Reads a legitimate Jedi Academy installation's maps and models through the
# engine's own readers; driven by scripts/probe_jka_assets.py.
jka_asset_probe = executable('fntech3_jka_asset_probe',
  'tests/fixtures/jka_asset_probe.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)

# A headless Q3Test client for server interoperability probes
# (scripts/probe_q3test.py protocol-client).
q3test_client_probe = executable('fntech3_q3test_client_probe',
  'tests/fixtures/q3test_client_probe.cpp',
  cpp_args: common_cpp_args,
  dependencies: host_machine.system() == 'windows' ? [cc.find_library('ws2_32')] : [],
)

stvef_demo_probe = executable('fntech3_stvef_demo_probe',
  'tests/fixtures/stvef_demo_probe.cpp',
  'code/qcommon/msg.cpp',
  'code/qcommon/huffman_static.c',
  include_directories: include_directories('code/qcommon'),
  c_args: common_c_args + ['-DDEDICATED'],
  cpp_args: common_cpp_args + ['-DDEDICATED'],
)

protocol_layout_tests = executable('fnql_protocol_layout_tests',
  'tests/protocol_layout_tests.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fnql_protocol_layout', protocol_layout_tests)

protocol_contract_tests = executable('fnql_protocol_contract_tests',
  'tests/protocol_contract_tests.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fnql_protocol_contract', protocol_contract_tests)

retail_auth_challenge_tests = executable('fnql_retail_auth_challenge_tests',
  'tests/retail_auth_challenge_tests.cpp',
  include_directories: include_directories('code/server'),
  cpp_args: common_cpp_args,
)
test('fnql_retail_auth_challenge', retail_auth_challenge_tests)

factory_rotation_tests = executable('fnql_factory_rotation_tests',
  'tests/factory_rotation_tests.cpp',
  'code/server/factory_rotation.cpp',
  include_directories: include_directories('code/server'),
  cpp_args: common_cpp_args,
)
test('fnql_factory_rotation', factory_rotation_tests)

stats_session_tests = executable('fnql_stats_session_tests',
  'tests/stats_session_tests.cpp',
  include_directories: include_directories('code/server'),
  cpp_args: common_cpp_args,
)
test('fnql_stats_session', stats_session_tests)

json_document_tests = executable('fnql_json_document_tests',
  'tests/json_document_tests.cpp',
  include_directories: include_directories('code/server'),
  cpp_args: common_cpp_args,
)
test('fnql_json_document', json_document_tests)

factory_catalog_tests = executable('fnql_factory_catalog_tests',
  [
    'tests/factory_catalog_tests.cpp',
    'code/server/factory_catalog.cpp',
  ],
  include_directories: include_directories('code/server'),
  cpp_args: common_cpp_args,
)
test('fnql_factory_catalog', factory_catalog_tests)

stats_event_tests = executable('fnql_stats_event_tests',
  'tests/stats_event_tests.cpp',
  include_directories: include_directories('code/server'),
  cpp_args: common_cpp_args,
)
test('fnql_stats_event', stats_event_tests)

stats_report_tests = executable('fnql_stats_report_tests',
  'tests/stats_report_tests.cpp',
  include_directories: include_directories('code/server'),
  cpp_args: common_cpp_args,
)
test('fnql_stats_report', stats_report_tests)

demo_stream_tests = executable('fnql_demo_stream_tests',
  'tests/demo_stream_tests.cpp',
  include_directories: include_directories('code/client'),
  cpp_args: common_cpp_args,
)
test('fnql_demo_stream', demo_stream_tests)

test('fnql_ql_demo_source', python,
  args: files('tests/ql_demo_source_tests.py'),
)

ql_bsp_contract_tests = executable('fnql_ql_bsp_contract_tests',
  'tests/ql_bsp_contract_tests.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fnql_ql_bsp_contract', ql_bsp_contract_tests)

bsp43_contract_tests = executable('fntech3_bsp43_contract_tests',
  'tests/bsp43_contract_tests.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_bsp43_contract', bsp43_contract_tests)

q3_ihv_netchan_tests = executable('fntech3_q3_ihv_netchan_tests',
  'tests/q3_ihv_netchan_tests.cpp',
  include_directories: include_directories('code/qcommon', 'code/compat'),
  c_args: common_c_args,
  cpp_args: common_cpp_args,
)
test('fntech3_q3_ihv_netchan', q3_ihv_netchan_tests)

q3_ihv_native_boundary_tests = executable('fntech3_q3_ihv_native_boundary_tests',
  'tests/q3_ihv_native_boundary_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_q3_ihv_native_boundary', q3_ihv_native_boundary_tests)

q3_ihv_wire_tests = executable('fntech3_q3_ihv_wire_tests',
  'tests/q3_ihv_wire_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_q3_ihv_wire', q3_ihv_wire_tests)

q3_ihv_message_tests = executable('fntech3_q3_ihv_message_tests',
  'tests/q3_ihv_message_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_q3_ihv_message', q3_ihv_message_tests)

q3_ihv_configstrings_tests = executable('fntech3_q3_ihv_configstrings_tests',
  'tests/q3_ihv_configstrings_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_q3_ihv_configstrings', q3_ihv_configstrings_tests)

q3_ihv_client_data_tests = executable('fntech3_q3_ihv_client_data_tests',
  'tests/q3_ihv_client_data_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_q3_ihv_client_data', q3_ihv_client_data_tests)

q3_ihv_model_tags_tests = executable('fntech3_q3_ihv_model_tags_tests',
  'tests/q3_ihv_model_tags_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_q3_ihv_model_tags', q3_ihv_model_tags_tests)

q3_ihv_scene_tests = executable('fntech3_q3_ihv_scene_tests',
  'tests/q3_ihv_scene_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_q3_ihv_scene', q3_ihv_scene_tests)

q3_ihv_identity_tests = executable('fntech3_q3_ihv_identity_tests',
  'tests/q3_ihv_identity_tests.cpp', 'code/qcommon/game_profile.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_q3_ihv_identity', q3_ihv_identity_tests)

q3test_identity_tests = executable('fntech3_q3test_identity_tests',
  'tests/q3test_identity_tests.cpp', 'code/qcommon/game_profile.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_q3test_identity', q3test_identity_tests)

q3test_wire_tests = executable('fntech3_q3test_wire_tests',
  'tests/q3test_wire_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_q3test_wire', q3test_wire_tests)

q3test_bsp_tests = executable('fntech3_q3test_bsp_tests',
  'tests/q3test_bsp_tests.cpp',
  include_directories: include_directories('code/qcommon'),
  cpp_args: common_cpp_args,
)
test('fntech3_q3test_bsp', q3test_bsp_tests)

q3_ihv_collision_tests = executable('fntech3_q3_ihv_collision_tests',
  'tests/q3_ihv_collision_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_q3_ihv_collision', q3_ihv_collision_tests)

collision_model_parity_tests = executable('fnql_collision_model_parity_tests',
  'tests/collision_model_parity_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fnql_collision_model_parity', collision_model_parity_tests)

collision_game_contract_tests = executable('fntech3_collision_game_contract_tests',
  'tests/collision_game_contract_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fntech3_collision_game_contract', collision_game_contract_tests)

# Links the real trace path against a stub collision model so the capsule
# geometry and the debug trace-plane invariant are exercised, not just
# described.
# q_math.c calls atan2/sincos/logf/powf/acos, which i686 emits as real libm
# calls instead of inlining, so this target needs the math library explicitly.
collision_capsule_trace_deps = []
if libm.found()
  collision_capsule_trace_deps += libm
endif

collision_capsule_trace_tests = executable('fnql_collision_capsule_trace_tests',
  [
    'tests/collision_capsule_trace_tests.c',
    'code/qcommon/cm_trace.c',
    'code/qcommon/q_math.c',
  ],
  include_directories: include_directories('code/qcommon'),
  c_args: common_c_args,
  dependencies: collision_capsule_trace_deps,
)
test('fnql_collision_capsule_trace', collision_capsule_trace_tests)

stvef_server_policy_tests = executable('fntech3_stvef_server_policy_tests',
  [
    'tests/stvef_server_policy_tests.cpp',
    'code/qcommon/q_math.c',
  ],
  include_directories: include_directories('code/qcommon'),
  c_args: common_c_args,
  cpp_args: common_cpp_args,
  dependencies: collision_capsule_trace_deps,
)
test('fntech3_stvef_server_policy', stvef_server_policy_tests)
test('fnql_collision_model_parity_source', python,
  args: files('tests/collision_model_parity_source_tests.py'),
)

test('fnql_ql_services_source', python,
  args: files('tests/ql_services_source_tests.py'),
)
test('fnql_ql_cvar_configstring_source', python,
  args: files('tests/ql_cvar_configstring_source_tests.py'),
)
test('fnql_join_menu_source', python,
  args: files('tests/join_menu_source_tests.py'),
)
test('fnql_windowed_mouse_source', python,
  args: files('tests/windowed_mouse_source_tests.py'),
)
test('fnql_input_system_source', python,
  args: files('tests/input_system_source_tests.py'),
)
test('fnql_frame_clock_source', python,
  args: files('tests/frame_clock_source_tests.py'),
)
test('fnql_platform_port_safety_source', python,
  args: files('tests/platform_port_safety_tests.py'),
)
test('fnql_renderer_contract_source', python,
  args: files('tests/renderer_contract_source_tests.py'),
)
test('fnql_lightmap_parity_source', python,
  args: files('tests/lightmap_parity_source_tests.py'),
)
test('fnql_renderer_cvar_compat_source', python,
  args: files('tests/renderer_cvar_compat_source_tests.py'),
)
test('fnql_fnq3_bloom_parity_source', python,
  args: files('tests/fnq3_bloom_parity_source_tests.py'),
)
test('fnql_rtx_renderer_contract_source', python,
  args: files('tests/rtx_renderer_contract_source_tests.py'),
)
test('fnql_rtx_raster_feature_parity_source', python,
  args: files('tests/rtx_raster_feature_parity_source_tests.py'),
)
test('fnql_rtx_vulkan_safety_source', python,
  args: files('tests/rtx_vulkan_safety_source_tests.py'),
)
test('fnql_rtx_runtime_smoke_source', python,
  args: files('tests/rtx_runtime_smoke_tests.py'),
)
test('fntech3_rtx_raster_execution', python,
  args: files('tests/rtx_raster_execution_tests.py'),
)
test('fntech3_rtx_lightmap_execution', python,
  args: files('tests/rtx_lightmap_execution_tests.py'),
)
test('fntech3_rtx_transport', python,
  args: files('tests/rtx_transport_tests.py'),
)
test('fntech3_rtx_lifecycle', python,
  args: files('tests/rtx_lifecycle_tests.py'),
)
rtx_mdr_geometry_tests = executable('fntech3_rtx_mdr_geometry_tests',
  files('tests/rtx_mdr_geometry_tests.c'),
  c_args: common_c_args,
  dependencies: [cc.find_library('m', required: false)],
)
test('fntech3_rtx_mdr_geometry', rtx_mdr_geometry_tests)
test('fnql_cvar_factory_snapshot_source', python,
  args: files('tests/cvar_factory_snapshot_tests.py'),
)
test('fnql_factory_runtime_source', python,
  args: files('tests/factory_runtime_source_tests.py'),
)
test('fnql_retail_factory_probe', python,
  args: [files('tests/retail_factory_probe_tests.py'), '--meson'],
)
test('fnql_steam_provider_source', python,
  args: files('tests/steam_provider_source_tests.py'),
)

steam_identity_tests = executable('fnql_steam_identity_tests',
  'tests/steam_identity_tests.cpp',
  cpp_args: common_cpp_args,
)
test('fnql_steam_identity', steam_identity_tests)
test('fnql_steam_identity_source', python,
  args: files('tests/steam_identity_source_tests.py'),
)

test('fnql_workshop_source', python,
  args: files('tests/workshop_source_tests.py'),
)

webpak_format_tests = executable('fnql_webpak_format_tests',
  'tests/webpak_format_tests.cpp',
  include_directories: include_directories('code/client'),
  cpp_args: common_cpp_args,
)
test('fnql_webpak_format', webpak_format_tests)
test('fnql_webpak_builder_source', python,
  args: files('tests/webpak_builder_tests.py'),
)

webui_backend_tests = executable('fnql_webui_backend_tests',
  'tests/webui_backend_tests.cpp',
  include_directories: include_directories('code/client'),
  cpp_args: common_cpp_args,
)
test('fnql_webui_backend', webui_backend_tests)

webui_request_schedule_tests = executable('fnql_webui_request_schedule_tests',
  'tests/webui_request_schedule_tests.cpp',
  include_directories: include_directories('code/client'),
  cpp_args: common_cpp_args,
)
test('fnql_webui_request_schedule', webui_request_schedule_tests)

webui_resource_queue_tests = executable('fnql_webui_resource_queue_tests',
  'tests/webui_resource_queue_tests.cpp',
  include_directories: include_directories('code/client'),
  cpp_args: common_cpp_args,
)
test('fnql_webui_resource_queue', webui_resource_queue_tests)

zmq_endpoint_tests = executable('fnql_zmq_endpoint_tests',
  'tests/zmq_endpoint_tests.cpp',
  include_directories: include_directories('code/server'),
  cpp_args: common_cpp_args,
)
test('fnql_zmq_endpoint', zmq_endpoint_tests)

if get_option('glx-tests')
  glx_logic_tests = executable('fnql_glx_logic_tests',
    'tests/glx/glx_logic_tests.cpp',
    'code/rendererglx/glx_executor.cpp',
    include_directories: include_directories('code/rendererglx', 'code/qcommon', 'code/renderercommon'),
    cpp_args: common_cpp_args,
  )
  test('fnql_glx_logic', glx_logic_tests)
  test('fnql_glx_header_boundary', python,
    args: files('tests/glx/glx_header_boundary_tests.py'),
  )
  test('fnql_glx_spot_shadow_source', python,
    args: files('tests/glx/glx_spot_shadow_source_tests.py'),
  )
  test('fnql_static_map_lights_source', python,
    args: files('tests/static_map_lights_source_tests.py'),
  )
  test('fnql_surface_light_proxy_source', python,
    args: files('tests/surface_light_proxy_source_tests.py'),
  )
  test('fnql_csm_shadow_cache_source', python,
    args: files('tests/csm_shadow_cache_source_tests.py'),
  )
  test('fnql_csm_plan_source', python,
    args: files('tests/csm_plan_source_tests.py'),
  )
  test('fnql_shadow_manager_source', python,
    args: files('tests/shadow_manager_source_tests.py'),
  )
  test('fnql_vid_restart_fast_source', python,
    args: files('tests/vid_restart_fast_source_tests.py'),
  )
  test('fnql_cgame_native_bridge_source', python,
    args: files('tests/cgame_native_bridge_source_tests.py'),
  )
  test('fnql_ui_native_bridge_source', python,
    args: files('tests/ui_native_bridge_source_tests.py'),
  )
endif

# Keep the recursive unittest-compatible source suite visible to the default
# Meson test workflow. Individual self-running modules above remain part of the
# complete Meson gate, while discovery catches compatibility tests that are
# intentionally independent of renderer/audio build options.
test('fnql_python_source_regressions', python,
  args: [
    '-m', 'unittest', 'discover',
    '-s', join_paths(meson.current_source_dir(), 'tests'),
    '-p', '*_tests.py',
  ],
  workdir: meson.current_source_dir(),
  # Discovery covers every profile and compiles hundreds of native fixtures.
  # The complete Windows x86 run exceeds thirty minutes; keep a bounded
  # one-hour gate so it can finish and report every failure.
  timeout: 3600,
)

curl_summary = 'disabled'
if use_curl
  curl_summary = '@0@ (@1@)'.format(use_curl_dlopen ? 'dlopen' : 'linked', curl_source)
endif

summary({
  'client executable': build_client ? client_binary_name : 'disabled',
  'dedicated server': build_server ? server_binary_name : 'disabled',
  'renderer-dlopen': build_client ? renderer_dlopen : false,
  'renderer modules': build_client and renderer_dlopen ? ', '.join(enabled_renderers) : 'disabled',
  'static renderer': build_client and not renderer_dlopen ? renderer_default : 'disabled',
  'renderer default': renderer_default,
  'renderer arch': renderer_arch,
  'SDL3': use_sdl ? sdl_source : 'disabled',
  'OpenAL': openal_source,
  'cURL': curl_summary,
  'JPEG': jpeg_source,
  'Ogg Vorbis': ogg_vorbis_source,
  'MP3': mp3_source,
}, section: 'fnTech3 build')
