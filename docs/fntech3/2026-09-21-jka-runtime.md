# Jedi Academy: what a developer server does with a real installation

## Scope

This record is the first runtime evidence for the Jedi Academy profiles: a
developer dedicated server, built with `-Djka-development=true`, started
against a legitimate retail 1.01 installation. It establishes where the engine
gets to today and where it stops. **No gameplay, module, wire or rendering
claim follows from it**; the module boundary is reached and refused by name.

The installation was read, never written: every run used an isolated
`fs_homepath` in a temporary directory, and the installation's own files are
unchanged. No client was started, no window opened and no input injected.

## What the engine does today

    fntech3.ded.exe +set fs_basepath "<installation>" +set fs_homepath <temp>
                    +set com_gameProfile jka-mp +set dedicated 1
                    +set logfile 2 +map mp/ffa1 +quit

1. **The profile is selected and activated.** Both `jka-sp` and `jka-mp` start
   this way; automatic detection asks for a mode, because one installation
   carries both.
2. **The filesystem starts on the retail paks**, in Raven's own order:

        <homepath>\base
        <installation>\base\assets3.pk3 (16 files)
        <installation>\base\assets2.pk3 (62 files)
        <installation>\base\assets1.pk3 (8320 files)
        <installation>\base\assets0.pk3 (15346 files)
        <installation>\base

   23,744 files in four paks, with the 1.01 patch on top.
3. **Raven's own `default.cfg` is executed** out of `assets2.pk3`, and each
   mode writes its own configuration into the home path when it shuts down:
   `base/jampconfig_server.cfg` for multiplayer and `base/jaconfig_server.cfg`
   for single player.
4. **A retail map's collision loads**, in both modes: multiplayer's
   `CM_LoadMap( 'maps/mp/ffa1.bsp' )` and single player's
   `CM_LoadMap( 'maps/t1_sour.bsp' )` both complete through the RBSP
   translation in [the map record](2026-09-21-jka-maps.md), building brushes
   and patch collision from Raven's own maps. The patch-bevel warnings the
   loader prints are Quake III's own and appear for its maps too.
5. **The retail game module loads and initializes**, out of the pak: the
   engine unpacks `jampgamex86.dll` into its own cache, finds Raven's `vmMain`
   and calls `GAME_INIT`. The module then prints its own banner through the
   engine's console and writes its own log:

        VM_LoadDll(qagame) found legacy vmMain at 20092A10
        ------- Game Initialization -------
        gamename: basejka
        gamedate: Nov  6 2003
        InitGame: \dmflags\0\fraglimit\20\g_gametype\0\mapname\mp/ffa1
                  \protocol\26 ... \g_saberLocking\1 \gamename\basejka

   `gamedate` is Raven's own build date, printed by the shipped binary. Getting
   this far means the module registered its cvars, opened `base/games.log`
   through the engine's filesystem and wrote its first line into it, and read
   the serverinfo back out of the engine. It runs on through its whole ICARUS
   boundary, is told the engine loaded no navigation graph, hands over its cull
   distance, publishes its entity and client arrays, has the server load a
   Ghoul2 model for it, registers a skin and puts it on that model, and stops
   at the first import the engine has not built:

        Jedi Academy game import G_UNLINKENTITY (35) is not served yet: its
        module boundary is still open. Its arrays are bound.

   Getting to an entity being unlinked means the level is spawning: the module
   is past its own initialization and into building a world.

   The server then shuts down. Single player still refuses before loading
   anything: its module hands the engine a function table that nothing reads
   yet.

## An ordinary build refuses the profile outright

The same command against a build without `-Djka-development=true` never reaches
any of this. It stops at detection:

    fnTech3 game profile detection failed (the game is recognized, but its
    engine adapters are not yet complete; see docs/fntech3/COMPATIBILITY.md,
    evidence: command line)

and writes nothing at all, not even a configuration. The gate is the default.

## Why the frontier is named rather than guessed

The first time the server got this far it died with an access violation: the
module called back through Raven's import numbers, nothing served them, and
whatever the engine returned was nonsense to it.

The engine now serves fifty-one imports and hands Quake III's shared
arithmetic block of fourteen through unchanged, because Jedi Academy kept its
numbers. Every other import stops the server with its own name and number.
Returning a made-up answer to an import nobody has implemented is how a
compatibility claim becomes a crash report; naming it turns the boundary into a
list that can be worked through, one round of this at a time.

Three of them deserve their names spelled out, because each is a whole answer
rather than a small one:

- **`G_SET_SHARED_BUFFER`** lends the engine one scratch buffer that certain
  later imports pass their bulky arguments through. Holding the pointer is the
  entire call, so it is served completely; nothing is copied through it while
  every import that would read or write it is refused.
- **`G_G2_CLEANENTATTACHMENTS`** asks the engine to forget every entity-to-bone
  attachment. fnTech3 holds none and cannot acquire one, because a Ghoul2
  instance can only be made through imports that are still refused, so doing
  nothing is the correct and complete answer. `jka_module_refusal_tests.py`
  fails the moment any other Ghoul2 import is served while this one still does
  nothing, which is what keeps that sentence true.
- **`G_LOCATE_GAME_DATA`** binds the module's own entity and client arrays to
  the engine's checked view at Raven's strides. The retail module now reaches
  it and the bind holds: asked later how far it got, the engine answers that
  its arrays are bound, which is the engine's own account rather than an
  inference from the order of Raven's source.

Two of the forty-eight are answered with a truthful no rather than a result.
The engine tells the game it did not load a navigation graph, because it did
not: it has no navigation at all. A legitimate installation does ship them, 24
of them, one per multiplayer map -- so that answer is a statement about this
engine and not about the installation, and a test fails the moment any other
navigation import is served while it still declines.

Server-side Ghoul2 began here, because both Raven titles stop until it exists.
It is not a renderer concern: the module makes a model for an entity while the
level is still spawning, long before anything is drawn. What the server holds
is the model and not the pose -- a `.glm`, the `.gla` it names, and the bone
count of each -- read through the same bounded readers that swept both
installations, so a model that would not read there does not load here. A model
that does not load is reported as not loaded, which is what the call returns
for one.

The instance pointer the module keeps in its own entity record is owned by the
engine and checked on the way back in. A pointer this store never handed out is
refused rather than followed, which `tests/raven_ghoul2_server_tests.cpp`
exercises with a stale one, a freed one and an address off the stack.

Skins are held the same way and for the same reason. Raven's own engine asks
its renderer for a *server* skin, separately from the one a client registers,
because a server needs the handle and not the shaders: the game hands that
handle back when it sets a skin on a model. So the file is read, the surfaces
it names are remembered, and nothing about how any of them are drawn is.

### The level now spawns

The entity-state adapter is written, and with it the boundary moved a long way
in one step. Academy's 532-byte entity state converts to and from the engine's
own state field by field: twenty-nine of its fields are Quake III's and keep
Quake III's meanings, and the other sixty-five are Academy's and live in an
engine-owned extension. The accounting is not asserted, it is checked -- a
record with a distinct value in every one of its 532 bytes goes out to the
engine's state and comes back byte for byte, so a field left unmapped would
come back zero and fail the test.

The trajectory inside that state needed its own measurement, and it is the kind
that would have gone unnoticed. **Academy's sine and gravity are 5 and 6, not
Quake III's 4 and 5**: Raven inserted a type of its own at 4. Converted on the
assumption that the first six coincide, every falling object in the game would
have oscillated instead. See `2026-09-22-jka-trace.md`.

With the state converting, the world answers: linking and unlinking, brush
models, point contents, both potentially-visible-set tests, area portals and
area connection, the entities in a box, and entity contact. Linking and
unlinking are served **by name and never handed through**, because Quake III's
34 and 35 are `G_BOT_ALLOCATE_CLIENT` and `G_BOT_FREE_CLIENT`: handing Raven's
numbers to the shared dispatcher would free a bot client instead of unlinking
an entity.

Tracing needed a measurement of the same kind and with the same stakes.
**Academy's `trace_t` is 48 bytes, not Quake III's 56**: the two solid flags and
the entity number are packed into the first four bytes, so the fraction sits
where Quake III keeps a flag and everything after it is four bytes early. This
was read off 203 of the module's own 233 call sites, not assumed. The trap also
passes two arguments Quake III's does not, a zero and a ten, which steer Ghoul2
collision and which the world trace records and does not act on.

### What the module asks for, exactly

The module reaches the engine through one function pointer, and every import it
can use has a small wrapper that pushes that import's number. Counting those
wrappers gives the whole of what this title will ever ask for: **150 of the 796
import numbers, and no others**. That is not an estimate of the work left; it
is its boundary.

It also settles what kind of bots this title has. **Not one of the 150 is in
the area-awareness block at 300-318**, and the installation ships no `.aas`
file for any map. Nor does it load a character or allocate a chat state, and
the installation ships none of Quake III's `items.c`, `weapons.c`, `syn.c`,
`rnd.c` or `match.c` -- only Raven's own `botfiles/*.jkb` and `bots.txt`. What
it does ask for is the state machinery and the elementary-action block, which
is what is left of Quake III's bot library once the configuration is taken
away. fnTech3's profile for this title said `COM_BOT_AI_QUAKE`, which the
installation contradicts; it now says what was measured. The engine had been
reporting `couldn't load weapons.c` truthfully, for a file this game was never
going to have.

### It no longer stops

The retail multiplayer module now runs `mp/ffa1` from `GAME_INIT` through the
whole of level spawning and into its frame loop, and **refuses nothing**. A
developer dedicated server holds the level open for as long as it is left to,
and the log carries no unserved import at all.

That is a milestone and not a compatibility claim. No client connects, no
snapshot is built and no packet is encoded; what has been shown is that every
import this title's module reaches during a map's life is answered, and that
none of the answers is a guess.

The last thing the module says is worth quoting, because it is the engine's own
honesty coming back through the game:

    Unable to save navigations data for map "mp/ffa1" (checksum:-1695889585)

`G_NAV_SAVE` answers that it wrote nothing, because this engine will not write
a `.nav` file into an installation it was asked only to read, and the module
reports that truthfully. The checksum it prints is `0x9AEAC74F` -- which is
exactly the second four bytes of the retail `maps/mp/ffa1.nav` in `assets0.pk3`.
The module and the shipped file agree, which identifies that field of the
`.nav` container from two directions at once and says that a loader, when one
is written, has something to match against.

### A bot connects, and where that stops

`addbot Alora 3` on a running server is the next thing after a level, because
it walks the whole client path without a network client. It gets a long way.

The module reads its own `botfiles/bots.txt`, finds the bot, and asks the
engine for a client slot. The slot is allocated, the bot's userinfo is written,
it connects, its goal, move and weapon states are allocated out of the bot
library, and its body is built: a Ghoul2 instance is duplicated for it, the
instance is attached to its entity number, the skeleton its model names is
read back, and bolts are registered by name against that model.

Four things were built for that and are real rather than answered empty:

- **An instance can be copied.** The copy owns its models; changing or
  releasing either leaves the other alone.
- **Attachments are held.** The engine remembers which instance belongs to
  which entity number, releasing an instance takes its attachments with it,
  and `G_G2_CLEANENTATTACHMENTS` now really clears what is held. That call
  used to be answered by doing nothing, which was honest only while nothing
  could make an attachment; the invariant that guarded it has been replaced by
  one that checks the clearing is real.
- **Models carry their names.** Both readers already proved every name field
  is terminated inside itself; the names are now read out when a model loads,
  so a bolt is a lookup rather than an invention.
- **Bolts are registered by name**, a surface first and then a bone, because
  the two are numbered separately. A name the model does not carry is reported
  as no bolt.

### The bot enters the game

It no longer stops at an import at all. The module announces the bot to the
server -- `broadcast: print "Alora^7 @@@PLENTER"` -- and calls `ClientBegin`
for client 0, with **no unserved import in the log**. Seven more things were
built to get there, and each is answered from evidence rather than resemblance:

- **A bone can be set playing, and a bone can be bent.** Retail `jamp.exe`'s
  own handlers fix both argument lists: the animation call reads ten arguments
  and converts exactly one, which places the bone's name as the only pointer;
  the angle call reads eleven and converts three, which places the name, the
  angles and the model list. The engine holds what it is told and interprets
  none of it -- which frame a bone is on at a given moment is arithmetic that
  has not been measured, and nothing here computes one.
- **A client's input frame converts both ways.** Academy's record is
  twenty-seven bytes: Quake III's first twenty-one, then a force power, an
  inventory item and a general command where Quake III keeps a second weapon
  and a field of view, so the three movement bytes sit one byte later. The
  same adapter serves the read and the write, and a frame that does not
  convert is refused rather than moved by.
- **A bot presses its keys.** Seventeen elementary actions, served by name,
  because Raven inserted an alternate attack and a force power into the middle
  of Quake III's block and everything after them sits two higher. The two
  Raven inserted have no counterpart in the library this engine carries: they
  are answered without doing anything, and that is stated rather than hidden.
  A bot here will move, turn, jump, crouch, fire and change weapon, and will
  not alt-fire or use a force power.
- **The input frame reads back.** That one carries a record, so the record was
  measured: the module's single call site reads its action flags at offset 32,
  once as a byte and once as a word it clears bits in, which is where Quake
  III keeps them and is not where they would be had Raven moved anything
  before them.

One of those turned up a mistake of this engine's own making. The guard that
keeps a vector that is not finite out of the collision model was refusing a
*missing* box as well, and a missing box is how this lineage has always asked
for a point trace -- the server's own trace substitutes the origin for one. The
guard now allows the two a trace may leave out, refuses a missing vector
anywhere else, and names the call and the numbers when it refuses, because its
old message printed zeroes for a null pointer and read as though the game had
handed in bad data.

### The bot plays, and then a fix moves the boundary earlier

The player state is written, and with it the bot stopped being a client that
exists and started being one that plays. A developer dedicated server left
running showed it moving about the level and taking things:

    ClientBegin: 0
    Item: 0 weapon_disruptor
    Item: 0 weapon_thermal
    Item: 0 ammo_powercell

**That state did not survive the next fix, and it should not have.** It held
only while every lightsaber in the game was quietly failing to load, which kept
the game off the whole branch that asks where a bolt on a saber is. Repairing
the loader -- see below -- is plainly right, and the consequence is that the
game now reaches that branch as soon as the bot has a saber. It connects,
enters and begins, and is then refused a bolt matrix:

    broadcast: print "Alora^7 @@@PLCONNECT"
    broadcast: print "Alora^7 @@@PLENTER"
    ClientBegin: 0
    ERROR: ... G_G2_GETBOLT (587) is not served yet

So what the client path can do is settled and what the Ghoul2 path still needs
is named. Running longer on a model that failed to load was not the better
state, and it is recorded here rather than quietly left as the headline.

Academy's player state is the larger of its two records: 168 fields, of which
43 are Quake III's at Quake III's widths and stay in the canonical record,
while 124 are Raven's and are carried in an engine-owned extension. Ammunition
is the one field that is neither, because this title has nineteen slots where
the canonical record holds sixteen: the first sixteen keep their canonical
meaning and the last three are carried.

None of those 168 members was typed out by hand. They were generated from the
measured table, so the names, the widths and the kinds are the ones that were
measured, and the same byte-for-byte test the entity state has covers this one:
a record with a distinct value in every one of its 1,552 bytes comes back
unchanged, so a field left unmapped would come back zero and fail.

The force block is carried whole and unread. The engine has no business knowing
what a force power is -- it moves the bytes between a module and a packet, and
the game decides what they mean.

### A truthful negative that turned out to matter

Through all of this the Ghoul2 store had been reporting, of every lightsaber in
the game:

    models/weapons2/saber_w.glm: the skeleton it names is not there

It was right that it had failed and wrong about why. The mesh names `*default`,
and a name beginning with a star is not a file at all: the mesh carries its own
bones and there is nothing to open. Measured across both installations,
`*default` is the *only* such name there is -- 41 of Jedi Academy's 127 meshes
and 23 of Jedi Outcast's 71 carry it -- and every name that does not begin with
a star resolves to a `.gla` that is present, in both. So the star is the rule
and not an exception to chase.

Fixing it moved the frontier immediately, which is the best evidence the
negative was real: with the sabers loading, the game went on to ask for four
more Ghoul2 calls it had never been able to reach.

### It does not stop

`G_G2_GETBOLT` was the last import a bot match reached and the only one it was
refused. It is served, and the match runs through it.

Getting there took the whole of the Ghoul2 pose, measured first out of the
shipped skeletons and then out of the retail engine itself:

- The pool stride is fourteen bytes, confirmed against eighteen skeletons
  across both titles with no remainder in any of them, and the frame indices
  are three bytes. The engine agrees in one instruction pair at `0x497a23`:
  `and ecx, 0xffffff` then `imul ecx, ecx, 0xe`.
- A pose is seven sixteen-bit words. The first four are a rotation and the
  last three a displacement in the model's own space, and the two halves do
  not agree with each other about where zero is: the engine's decompressor
  reads a rotation component as `word / 16383 - 2` and a displacement as
  `word / 64 - 512`. The displacement had been measured correctly from the
  data; the rotation had not, and the data had been saying so.
- A pose composes as that transform applied to the bone's own rest matrix,
  with no walk up the hierarchy at all, which holds Academy's humanoid to a
  median bone-length error of 0.034 units where every other candidate rule is
  wrong by 99% or more.
- A bone's frame comes from `frame = startFrame + ((now - startTime) / 50) *
  animSpeed`, so a Ghoul2 frame is fifty milliseconds. Two frames are blended
  element by element across the whole matrix, which is what the engine does
  and not a quaternion slerp.

The bolts a game asks for turned out not to be bones. Academy's player meshes
carry forty-five star-named surfaces, and the sabre hangs off `*r_hand`, which
is surface 60 of the mesh. Such a bolt is a tag: three corners carried by the
bones they are weighted to. Each is skinned by first undoing where its bone
rests and then applying where it is now, because a vertex is modelled in the
whole model's space rather than relative to the bone that carries it. That
step is invisible by eye and was left out at first, which put the right hand's
tag 26.7 units away from the hand on a retail model; with it the tag lands
0.644 units from `rhang_tag_bone`, which is where a tag on a hand belongs.

The measurement, its method, and the four things it got wrong on the way are
in docs/fntech3/2026-09-22-jka-pose.md.

### The bot plays

    ClientConnect: 0
    broadcast: print "Alora^7 @@@PLCONNECT"
    broadcast: print "Alora^7 @@@PLENTER"
    ClientBegin: 0
    Item: 0 weapon_disruptor
    Item: 0 weapon_thermal

Twenty seconds of `mp/ffa1` with a sabre loaded and the bolt branch live, and
**no import refused anywhere in the path**. Jedi Outcast does the same on
`ffa_bespin` with two bots, which take items and talk to each other, out of
the same shared store.

### What is missing behind that



Raven's own navigation graph is held but never filled from a file. `G_NAV_LOAD` answers truthfully that it loaded nothing,
so the graph the engine holds stays empty and every query against it says there
is no node -- which is what an empty graph means. All twenty-three navigation
imports this title uses are answered out of that one graph, and the six that do
nothing say in their own source why. The module never calls `G_NAV_INIT` or
`G_NAV_FREE` at all, so the graph's lifetime is the engine's. Reading a `.nav`
file is the next thing that would put real nodes in it.

One truthful negative is worth naming while it stands: the server-side Ghoul2
store reports `models/weapons2/saber/saber_w.glm: the skeleton it names is not
there`. The model loads and names a `.gla`; that `.gla` is not found under the
name the mesh gives. Whether the name needs a prefix this reader is not adding,
or the file is elsewhere, is unresolved and recorded here rather than papered
over.

Jedi Outcast now reaches the same place by its own route: it registers its
items, parses its bots and arenas, and holds `ffa_bespin` open with nothing
refused. Every number it needed was read out of its own engine's dispatcher,
never taken from Academy's table -- and two of them show why that mattered.
Its world block is Quake III's numbering, which Academy's is not; its trace is
Quake III's 56-byte record, which Academy's is not; and its motion paths sit
five numbers lower than Academy's. See `2026-09-22-jk2-records.md`.

## What this does not show

- The game module runs only as far as its first unserved import: it has
  registered its cvars and printed its banner, and there is still no entity, no
  snapshot, no client and no gameplay.
- The entity and client binding is exercised against the retail module; the
  configstring calls and the entity-token reader may or may not be, and nothing
  here counts which of them the module used.
- No map is drawn: the collision loader reads a map, the renderer does not.
- No packet is encoded or decoded; protocol 26 remains a registered number.
- Only one installation, one edition and one platform were used.
- The single-player profile reaches the same boundary from its own map; its
  `GetGameAPI` table is unwritten.
