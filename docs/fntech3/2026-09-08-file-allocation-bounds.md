# Whole-file length and temporary-allocation bounds

Whole-file loads reserve an extra NUL byte through an allocator taking int sizes.
A declared INT_MAX length could overflow that request; negative narrowed ZIP sizes
could also reach the allocation path. Root-archive, ordinary filesystem, profile
file and journal reads now reject lengths that cannot represent the NUL-inclusive
request. Valid open handles are closed before rejection, outputs are cleared, and
ordinary config journaling retains its missing-file representation.

The temporary allocator had a related independent overflow in alignment/header
arithmetic and in the combined bank-capacity sum. It now bounds the requested
size plus alignment and the selected allocator header before int arithmetic,
including the early zone-allocation path. Capacity addition uses uint64_t so it
cannot wrap on x86. Invalid requests fail before allocating or advancing a bank.
These guards do not promise that every representable request fits available memory.

Executable regressions exercise negative sizes, INT_MIN, INT_MAX and nearby
values, no-allocation/no-leak rejection, length-only boundaries, journal handling,
both hunk banks, initialized/uninitialized paths and a combined bank sum exceeding
32 bits. The previous functions fail those regressions. Windows and Linux focused
tests pass without attempting multi-gigabyte allocations. The final MSVC x86 build
and full Windows suite pass 110 configured targets with zero failures and one
skip. The [evidence record](2026-09-08-file-allocation-bounds.json) contains source
hashes and the nested Python count. An earlier filesystem-only suite also passed;
the final complete suite was rerun after the allocator correction.

Synthetic stored paks declare uncompressed lengths 2147483647 and 4294967295
without supplying such payloads. The final dedicated binary rejects each bad
config in both profiles, executes the following valid config in the same pak,
and exits zero without forced termination: four successful bounded rejection
probes. This is not a claim of successful large-file allocation.

The final Windows x86 client/server binaries also complete Q3 and QL self-pairs,
exact synthetic HTTP downloads and pure admission. All four processes exit
normally. Engine screenshots show Q3 world/weapon/HUD and QL world with its join
panel. These 35.672/35.860-second probes use legitimate read-only assets, isolated
writable homes, windowed private unswitched desktops and disabled input/services.
No keyboard/mouse control or OS capture is used; proprietary inputs and screenshot
pixels remain ignored.

This extends the [whole-file integrity correction](2026-09-08-whole-file-pak-integrity.md).
Broader allocator/asset audits, memory-pressure behavior, streaming integrity and
full profile compatibility remain open. Existing image/font and temporary-hunk
diagnostics remain outside this correction. No external implementation was added.
