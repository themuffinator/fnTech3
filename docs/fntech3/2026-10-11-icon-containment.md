# Installation icon containment

The installation icon resolver now uses the shared handle-based canonical path
resolver on Windows. MinGW GCC 16.2's `std::filesystem::canonical` retains NTFS
junction spelling. A synthetic `Quake3.app` junction to an external directory
therefore passed the old parent-directory check. An installation reached through
a junction also returned an unresolved path. Both observations are reproduced by
`tests/game_icon_containment_tests.py` against the previous header.

The corrected resolver resolves both the installation and candidate before
comparing their immediate parent. Missing paths, wrong object types and resolver
errors still return no icon, preserving the built-in fallback. No retail assets
are loaded by these fixtures.

Validation on Windows x86 with MinGW GCC 16.2 and Clang/MSVC passed the regular,
external-junction and installation-alias cases. File symlinks were explicitly
skipped on this Windows account because it lacks symlink privileges. Linux x86
passed regular paths, rejected an external file symlink and accepted an internal
file symlink; Windows junction cases were skipped there. This verifies the shared
resolver boundary, not a demonstrated Windows executable-icon file-symlink attack
or any new game compatibility gate.
