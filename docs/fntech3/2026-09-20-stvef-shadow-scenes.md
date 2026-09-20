# Elite Force visible shadow modes

**Live-evidence correction (2026-09-21):** These probes used `r_renderer`,
which does not select fnTech3's backend. All fnTech3 live runs in this record
used GLx; Vulkan/RTX labels in the historical manifest identify the requested
test cases only. Source, native-vector and compiled-fixture evidence is
unchanged. See the [renderer audit](2026-09-21-stvef-renderer-verification.md).

A wider view of the saved `tutorial` scene now establishes positive live
examples of blob, stencil and projection shadows. The previous camera cut off
the NPC's feet and projected shadow. This follow-up changes only the probe:
`cg_fov 110` includes the receiving floor, and the settled Tuvok NPC retains
the frozen AI and held animation used in the earlier shadow checks.

The retail engine completes one windowed run and fnTech3 three GLx runs,
capturing all four `cg_shadows` values through their own `screenshot` commands.
All sixteen captures are retained locally. Every fnTech3 image is identical
across its three GLx runs. There is no keyboard/mouse control or operating-
system screen capture, and writable homes stay under `E:\Games\fnTech3-tests`.

Comparisons subtract each engine's shadow-off image to separate the shadow
contribution from background differences:

| Mode | Retail changed pixels | fnTech3 changed pixels | Shadow-mask disagreement | Mean absolute shadow difference per 8-bit channel |
| --- | ---: | ---: | ---: | ---: |
| Blob | 8,244 | 8,344 | 192 | 0.00618 |
| Stencil | 3,787 | 3,735 | 258 | 0.00936 |
| Projection | 3,098 | 3,100 | 80 | 0.01846 |

These are measurements of this scene, not universal tolerances. Background
images differ by 0.607 or less per channel on average, and retail images are
not byte-identical to fnTech3. The native mathematical fixtures remain the
evidence for the exact projection/extrusion contract; these observations add
visible coverage for all three legacy modes. Broader scenes and complete image
parity remain open.

The [evidence manifest](evidence/2026-09-20-stvef-shadow-scenes.json) records
binary, pak, save and probe identities, every command, capture hash and image
measurement. Local scripts and images are in `.tmp/stvef-gaps/shadow-wide-*`.
