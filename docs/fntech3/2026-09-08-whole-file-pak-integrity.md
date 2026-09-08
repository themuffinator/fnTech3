# Reject incomplete or corrupt whole-file pak payloads

Ordinary FS_ReadFile ignored the read byte count and returned buffers from
corrupt ZIP entries. A synthetic stored pak demonstrated the consequence in both
profiles: a config whose local and central CRC declarations were deliberately
wrong still executed. A following valid config in the same pak also executed,
and both dedicated processes exited zero.

The first attempted correction checked ZIP close status. Runtime still accepted
the bad config because this inherited unzip reader has its CRC accumulation and
close check disabled. The final correction uses the existing crc32_buffer on
complete payloads and compares both length and CRC against entry metadata.
It applies to ordinary whole-file pak reads and engine-owned root-archive reads.
No streaming reader or external unzip implementation is replaced.

A failed ordinary whole-file read now closes the handle, frees the temporary
buffer, clears the caller output and returns -1 before incrementing load counters.
Journaled config failures write the existing missing-file representation rather
than unverified content. The root-archive reader likewise rejects the failed
payload and retains its existing allowed fallback behavior. Length-only queries
remain metadata queries, and automatic profile identity remains a bounded metadata
heuristic; neither is now claimed to authenticate archive contents.

With the final correction, both profiles reject the corrupt config, execute the
valid following config from the same pak and exit zero without forced termination.
The [evidence record](2026-09-08-whole-file-pak-integrity.json) preserves both earlier
accepted-corruption outcomes and the final results, source/binary hashes, commands
and log hashes. Fixtures contain only independently authored echo commands.

The executable regression covers short/error reads, incorrect CRC, empty entries,
output/resource/counter preservation, config-journal failure records, a valid load
after failures and unchanged length-only behavior. The original reader fails the
regression. The strengthened test runs the production CRC and validator functions
and passes on Windows and Linux. The full Windows suite passes 110 configured
targets, zero failures and one skip; its nested Python count is in the record.
The focused strengthened test was rerun after that suite without an engine change.

Q3 and QL self-pairs using the corrected Windows x86 client/dedicated binaries
also complete exact synthetic HTTP downloads, pass pure admission and exit normally
on all four processes. Engine screenshots show Q3 world/weapon/HUD and QL world
with its native join panel. These 35.782/35.375-second probes use legitimate
read-only installations, isolated writable homes, disabled input/services,
windowed private unswitched desktops and engine screenshots. No user input or OS
capture occurs; retail inputs and screenshots remain ignored. No independent
retail-engine interoperability or active QL match claim is made here.

CRC is an integrity check, not authentication. Streaming payloads, comprehensive
archive validation, wider malformed ZIP cases, performance characterization and
full compatibility remain open. Existing image/font and temporary-hunk diagnostics
remain outside this correction. No external implementation was incorporated.
