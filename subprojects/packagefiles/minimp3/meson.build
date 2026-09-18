project('minimp3', 'c', license: 'CC0-1.0')

# Header-only MPEG audio decoder; fnTech3 compiles the implementation once in
# its own MP3 sound codec translation unit.
minimp3_dep = declare_dependency(
  include_directories: include_directories('.', is_system: true),
)

meson.override_dependency('minimp3', minimp3_dep)
