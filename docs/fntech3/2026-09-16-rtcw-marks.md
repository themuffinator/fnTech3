# RTCW decal projection boundary, 2026-09-16

Approved startup retest 3 passed the initial SP autosave, scene-fog guard and
silent-backend voice query, then stopped at `CG_CM_MARKFRAGMENTS` (import 30).
The engine-generated late image in
`.tmp/rtcw-manager/startup-retest-3/runtime/` shows that error dialog. It does
not establish playable SP. Its two matching save files contain 218,225 bytes
and SHA-256 `bee8cf22636a167d90e4f855d86fbb68306fabfe11ae32db396f02cabbe8508f`.

The original SP/MP call takes an integer rotation, four input corners and a
projection vector. Its result uses twenty-byte XYZST vertices. A negative
fragment point count means the renderer supplied texture coordinates; negative
fragment capacity requests the older projection mapping. Q3's count argument
and twelve-byte vertex result cannot serve this contract.

Renderer API 24 adds a distinct `MarkRTCWFragments` entrypoint for GLx, Vulkan
and RTX. The client validates native/QVM spans, integer widths, finite input,
capacity arithmetic and disjoint output buffers. It copies inputs to owned
storage, validates the complete renderer result before publishing it, and
preserves unused bytes, including ST values in XYZ-only fragments. External
records are marshalled independently from renderer-owned types.

The shared renderer implementation independently follows observed SP/MP
face/curve selection, recent visibility, shader exclusions, rotation, clipping
and texture mapping. It reuses the repository's GPLv2-or-later Q3 polygon
clipper. Original RTCW's opposite face-facing cutoff, lack of triangle-soup
marks and face-before-curve plane-count behavior are preserved explicitly.
Each query owns duplicate suppression rather than modifying rendering stamps.
Degenerate geometry returns no fragments; overflowing arithmetic, malformed
indices, cyclic traversal and excessive recursive depth fail explicitly.

Five focused tests cover both mappings, rotation, curve triangles, visibility,
surface exclusions, shared leaves, clipping capacity, malformed input,
native/QVM unaligned spans, renderer failure and atomic output publication.
They pass on Windows x86 (3.671 seconds), Windows x64 (2.933 seconds), and Linux
x86 with address/undefined-behavior sanitizers and leak detection (22.420
seconds). Initial fixture/compiler failures and their corrections remain in
the task logs. All three development renderer builds pass.

Updated source probes pass against original and iortcw SP/MP on Windows x86
and x64: 2,215 assertions for each SP reference and 2,205 for each MP reference.
These checks establish structure, offset, scalar and ordinal agreement. They
do not establish original-renderer image or floating-point output parity.

The SP import audit also identifies remaining corona and camera calls, an
unused collision-model-loading ordinal, and diagnostic test-print imports.
Retest 4 reached the corona call after this correction. Retest 5, with corona
support, then reached the camera loader. The [latest evidence](2026-09-16-rtcw-coronas.md)
records the resulting error image and subsequent shared-renderer checks.
Renderer API 25 supersedes the initial decal-only API 24.
