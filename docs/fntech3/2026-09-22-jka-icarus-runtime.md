# Jedi Academy scripts: running them

## Scope

The [script record](2026-09-21-jka-icarus.md) established what a compiled
ICARUS script *is*. This one is about running it: the interpreter in
`code/icarus`, the boundary it hands commands across, and the nineteen imports
the game module uses to drive it.

Where a number comes from is separated throughout. **What** a block number
means is Raven's, frozen in `code/compat/jka_icarus_ids.def` and cited by file
and line. **How** each one behaves is fnTech3's own design, and the sections
below say which behaviours the scripts themselves fix and which this engine
chose. No implementation text is incorporated from any source.

## Where the boundary is

ICARUS is an engine subsystem, not a game one. Raven's own split is visible in
the retail module's export table and is not a matter of interpretation:

- **The engine** owns the script cache, the sequencers, the task manager, the
  variables, the clock and every piece of flow control.
- **The game** owns every command that touches an entity. The engine hands one
  over by writing a record into the buffer the module lent it at startup and
  calling the module's export **with no arguments at all**.

That is what `G_SET_SHARED_BUFFER` is for, and it is why it is the third thing
the module asks for. The seventeen records are frozen in
`code/compat/jka_icarus_callbacks.h`, sizes and offsets asserted, with the
largest at 4,104 bytes. Raven did not order them uniformly -- most begin with
the task identifier and then the entity, and the two lerp endpoints begin with
the entity -- and that is preserved exactly.

## What the engine runs

A sequencer per entity, each a stack of frames over spans of statements. A
sequencer blocks on three things and nothing else:

| It waits for | Until |
| --- | --- |
| a time | the level clock reaches it |
| a task group | the game reports every command in it finished |
| a signal | something raises it |

Each opener is matched to its close once, when the script loads, so running one
never searches for the end of anything. A script whose bodies do not nest is
refused at load rather than part-way through.

Flow the engine carries out itself: `affect`, `if`/`else`, `loop`, `task`,
`do`, `wait`, `signal`, `waitsignal`, `declare`, `free`, `print`, `rem`,
`flush`, `run`, and the `get`, `random` and `tag` groups that appear inside
other commands' values.

Commands handed to the game: `set` (when it is not a declared variable),
`sound`, `move`, `rotate`, `use`, `kill`, `remove`, `play`.

## What the scripts decided, and what this engine decided

Some of the behaviour below is fixed by the corpus. The rest is a choice, and
it is marked as one.

**Fixed by the scripts.**

- *A `set` is either the engine's or the game's, decided by whether the name is
  a declared variable.* The corpus separates these cleanly on its own: 6,850 of
  block 26's leading strings begin `SET_` and the remaining ~115 are lowercase
  names like `fUsedBeam`, which are exactly the names `declare` introduces. The
  engine keeps the declared ones and passes the rest on.
- *A `wait` on a name waits for a task.* Every one of block 23's 926 string
  values is declared by a `task` in the same script, without exception.
- *A `signal` can be waited on from another script.* Blocks 46 and 47 name each
  other across scripts of one level, which the task namespace never does.
- *A `loop` of `-1` runs forever.* It is one of only four values block 27 ever
  takes, and the other three are plain counts.

**Chosen by this engine.**

- *A `waitsignal` consumes the signal it was waiting for.* Nothing in the files
  says whether a raised signal stays raised for a second waiter.
- *An `affect` that names no entity stops the script.* Silently skipping would
  be the friendlier choice and may well be the original one, but it is also the
  failure this whole boundary exists to avoid, so the stop is named instead.
- *A `flush` clears the sequencer it runs on, and the commands the game still
  owes it stop counting against a wait.*
- *`random` uses the engine's own generator, seeded per level*, so a probe of
  the same level repeats exactly.
- *A script that never pauses is stopped* after a bounded number of statements,
  by name, rather than holding the frame.

Anything else -- `camera`, the `tag` group, the lerp endpoints, a `move` in a
shape this does not read -- stops the script **by name**. A command is never
skipped, and a script never half-runs.

## What it does against the retail module

A developer server with a legitimate 1.01 installation now gets the retail
`jampgamex86.dll` through its whole ICARUS boundary. The module initializes
ICARUS, and the engine no longer stops there:

    Jedi Academy game import G_NAV_LOAD (202) is not served yet

`G_ICARUS_INIT` was the frontier before this slice. It is now navigation --
Raven's own NPC pathfinding -- which is the next engine subsystem on this
title's critical path.

Forty-six of multiplayer's imports are served. Nineteen of them are the ICARUS
surface: init, shutdown and the initialized check; the four entity calls,
including the one that tells the engine what a script's names mean; run and
register; the per-entity tick and the running check; the three task-identifier
calls; and the five variable calls.

## What this does not show

- No script has been run against the retail module. The interpreter is exercised
  by `tests/jka_icarus_runtime_tests.cpp` against a host of the test's own
  making; the module reaches ICARUS but stops at navigation before a level's
  scripts start.
- Nothing has been rendered, no entity has moved, and no client exists.
- The behaviours marked as this engine's choices are not claims about Raven's.
  When a script's observable behaviour later contradicts one, the choice is
  what should change.
- Jedi Outcast ships the same format and the same interpreter would run it, but
  nothing has driven its modules; its multiplayer is QVMs and its boundary has
  not been opened.
