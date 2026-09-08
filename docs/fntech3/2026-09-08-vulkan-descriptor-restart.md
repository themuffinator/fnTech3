# Reallocate Vulkan attachment sets before updating them

The native Linux i686 Vulkan client initializes under private Xvfb with Mesa
lavapipe, but a pure Q3 server join triggers a renderer resource rebuild and
SIGSEGV. GDB traces the fault through `vk_update_descriptor_sets`,
`vk_update_attachment_descriptors`, and `vk_init_descriptors` during
`CL_FlushMemory`. The failing write targets the depth-fade descriptor.

`vk_release_resources` resets the descriptor pool, freeing its descriptor sets
while engine fields retain their old handles. `vk_init_descriptors` previously
updated all attachment descriptors after each category's allocation. Its first
update could therefore use depth-fade or shadow sets before their replacements
were allocated. Successful initial startup did not exercise this condition.

Vulkan now allocates all attachment sets before issuing one attachment update,
matching the lifecycle ordering already established in the RTX renderer. This
is an independently authored correction to repository code; it changes neither
profile's wire/module contract nor the attachment feature selection.

The existing executable descriptor lifecycle harness now covers both Vulkan
and RTX production functions. It models pool reset while deliberately retaining
old engine handles, checks that every write targets a live set, and requires
one update per allocated set. Cases include color/bloom options, partially
populated menu blur images, depth fade, and combinations of shadow attachments.
The pre-fix Vulkan implementation fails this regression; both corrected
renderer paths pass the focused Windows test run. The complete configured
Linux suite passes 107 checks with zero failures and two unavailable
OpenAL/retail-factory fixture skips; its aggregate source suite passes 1,256
tests with 15 skips.

Runtime probes use windowed mode, isolated writable homes, disabled input
grabbing, engine-command screenshots and private displays/desktops. The Linux
Q3 probe now passes pure admission, snapshots, `vid_restart`, before/after
screenshots and normal client/server exits. Windows x86 checks exercise the
same Vulkan correction with both Q3 QVMs and QL native modules: both reach
active network state, restart, write before/after screenshots and exit normally
along with their servers. QL remains at the native join UI; this does not claim
an entered-match test. Exact results,
source/artifact identities and log hashes are in the
[evidence record](evidence/2026-09-08-vulkan-descriptor-restart.json).

The Linux build is the earlier `b5776c9` checkout with the recorded Unix
exit/X11/console fixes and this Vulkan correction; it is not a fresh clean
checkout claim. Screenshots and retail inputs stay local. These bounded probes
do not prove full visual parity, validation-layer cleanliness, native ray
tracing, driver coverage or sustained lifecycle stress. Native Linux QL client
modules are unavailable in the local legitimate installation. Broader merger
gates, existing image/font warnings and hosted CI's billing/spending blocker
remain open.
