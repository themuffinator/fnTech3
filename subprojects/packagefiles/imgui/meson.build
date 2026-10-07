project('imgui', 'cpp', version: '1.92.9b', license: 'MIT',
  default_options: ['cpp_std=c++17', 'warning_level=0', 'werror=false'])

# Dear ImGui's core and its std::string helpers alone: the entity editor
# draws ImGui's triangles through the engine's renderers, so no platform or
# renderer backend is built. The engine supplies the clipboard and opens
# nothing in a shell.
imgui_args = [
  '-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS',
  '-DIMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS',
  '-DIMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS',
  '-DIMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS',
]

imgui_inc = include_directories('.', 'misc/cpp')

imgui_lib = static_library('imgui',
  'imgui.cpp',
  'imgui_draw.cpp',
  'imgui_tables.cpp',
  'imgui_widgets.cpp',
  'misc/cpp/imgui_stdlib.cpp',
  cpp_args: imgui_args,
  include_directories: imgui_inc,
  gnu_symbol_visibility: 'hidden',
  install: false,
)

imgui_dep = declare_dependency(
  include_directories: include_directories('.', 'misc/cpp', is_system: true),
  compile_args: imgui_args,
  link_with: imgui_lib,
)

meson.override_dependency('imgui', imgui_dep)
