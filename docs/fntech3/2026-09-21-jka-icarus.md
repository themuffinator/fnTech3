# Jedi Academy scripts: reading the ICARUS container

## Scope

ICARUS is Raven's scripting system, and a `.ibi` file is one compiled script:
what BehavEd produces from a designer's behaviour graph. It matters to fnTech3
earlier than a scripting system usually would, because a Jedi Academy
multiplayer game module asks the engine to initialize ICARUS inside
`GAME_INIT`, before a single entity exists. A server for this title cannot
start a level without it.

This slice reads the container, and then the values inside it, and stops there.
Nothing is interpreted: no block number is given a meaning, no command runs,
and no entity is affected. What follows is what the files themselves say.

## What the installation ships

A legitimate retail 1.01 installation stores **1,018 compiled scripts**,
1,122,906 bytes in all, from 8 bytes to 27,083. All 1,011 distinct paths are in
`assets0.pk3`; the 1.01 patch replaces seven of them, six from `assets2.pk3`
and one from `assets3.pk3`. A running engine therefore sees **1,011 scripts**,
with seven superseded copies underneath. Both numbers are used below --
1,018 where every stored script was read, 1,011 where the engine's own
filesystem chose which copy to open.

## The container

Every one of the 1,018 begins with the same eight bytes:

| Bytes | Field | Observed |
| --- | --- | --- |
| 0-3 | identifier | `IBI\0` in all 1,018 |
| 4-7 | version, written as a 32-bit float | `1.57` in all 1,018 (`0x3FC8F5C3`) |

After that the file is a run of blocks, to the end:

| Bytes | Field |
| --- | --- |
| 0-3 | block number |
| 4-7 | member count |
| 8 | one byte, `0` in every block of every installed script |
| then | that many members |

and each member is

| Bytes | Field |
| --- | --- |
| 0-3 | member number |
| 4-7 | payload size |
| then | exactly that many bytes, with no padding and no alignment |

**Read that way, all 1,018 stored scripts parse exactly**: 26,512 blocks and
52,699 members carrying 454,562 bytes of payload, with no count reaching past
its file and not one byte left over at the end of any of them. Of the 1,011 a
running engine would open, the reader in this repository reports 25,987 blocks
and 51,567 members over 444,454 payload bytes; the difference is the seven
superseded copies. Ten of the scripts are eight bytes long -- a header and no
blocks at all -- which the reader accepts as the whole script it is. The
longest single script holds 648 blocks, and the most varied uses 16 different
block numbers.

## What the numbers look like

These are counts over the corpus, and nothing below them was known when they
were taken. Raven's own names for these numbers appear further down, but they
were read only after everything between here and there had been derived.

The 24 block numbers that occur: 19-23, 25-27, 29-35, 38-39, 41-44, 46-48.

The 11 member numbers, and what their payloads look like:

| Member | Count | Payload |
| --- | --- | --- |
| 4 | 23,722 | terminated text, 2 to 242 bytes |
| 6 | 23,184 | 4 bytes, 2,551 distinct values as a float |
| 7 | 1,130 | terminated text, 10 to 18 bytes, nine distinct uppercase names such as `CHAN_VOICE` |
| 14 | 2,832 | 4 bytes, always the float `14` |
| 15 | 48 | 4 bytes, always the float `0` |
| 16 | 3 | 4 bytes, always the float `0` |
| 17 | 146 | 4 bytes, always the float `0` |
| 18 | 8 | 4 bytes, always the float `0` |
| 36 | 326 | 4 bytes, always the float `36` |
| 37 | 169 | 4 bytes, always the float `16777216` |
| 49 | 1,131 | 4 bytes, always the float `49` |

Three of those -- 14, 36 and 49 -- carry their own member number as a float, and
four more carry zero. A member whose payload never varies is carrying no value,
so the number itself is the whole of what it says. That started as a guess from
this table; the next section turns it into something the whole corpus checks.

## A member's payload is not always its value

Some members introduce the ones after them. Reading each block as a sequence of
value groups, with these rules and no others:

| Member | Is | Consumes |
| --- | --- | --- |
| 4, 6, 7 | a value | itself: text, a float, a name |
| 14 | a value | itself and three member-6 floats |
| 36 | a value | itself, a member-6 float and member-4 text |
| 37 | a value | itself and two member-6 floats |
| 49 | a value | itself, one whole value group, and a member-6 float |
| 15, 16, 17, 18 | an operator | itself and one whole value group |

**every one of the 26,512 blocks is consumed exactly, and none is refused.**
Every one of the 205 operators ends its block, 205 of 205. This was arrived at
by taking the members that never vary their payload -- there is no point
storing a value that is always the same -- and asking what follows each of
them; the runs are fixed, so the rules fell out, and then the whole corpus was
parsed to check that nothing else was hiding.

That explains the constant payloads in the table above: a member that
introduces the ones after it needs no value of its own, and Raven's compiler
wrote the number itself, or zero, into the space where one would go.

Counted in value groups rather than members, **22 of the 24 block numbers have
one fixed shape**: block 26 always takes two values, block 23 always one, block
25 always none. Only two vary -- block 21 takes two or three, and block 35
takes one, three, four or six, so something there is genuinely variadic.

Exactly one block number contains an operator: block 38, one value against
another, in all 205 of its occurrences, and it is one of the five that open a
body. A block that compares two things and then opens a body is a conditional
in any language; the record goes that far and no further, because which
comparison each of the four operators stands for is not established here.

Two of those groups say a little more about themselves.

**Member 36 states a type, and states it in the same numbers the member stream
uses.** Its float is drawn from exactly three values across all 326 occurrences
-- `4`, `6` and `14`, appearing 149, 175 and 2 times -- which are precisely the
member numbers for text, for a float and for a three-float group. So there is
one number space here, not two: the same `4` that marks a member as text marks
a named thing as being text. That is why the member numbers and the small
integers in block 19, block 21, block 26 and block 35 look like they belong
together. They do.

**Member 37's two floats are ordered.** In all 169 groups the first is strictly
below the second, never equal and never above: `(500, 1500)`, `(1500, 3500)`.
A pair that is always strictly ordered is a range, and a script asking for a
range of numbers is asking for one from inside it. The record stops there.

Member 49 takes its name either as text, 1,083 times, or as a member-36 group,
48 times: literally, or through something named.

These rules are now a reader of their own. `code/compat/jka_icarus_values.hpp`
sits over the container and groups each block's members into the values it was
given, refusing anything the rules do not cover: a member number this corpus
never used, a group that runs off the end of its block, a type number that
names no kind of value, a comparison anywhere but last. It accounts for every
member of every block rather than skipping one it has no rule for, so a script
it reads is a script wholly read.

Over the installation it reads **all 1,011 scripts with no refusals**: 25,987
statements, 39,854 values grouped out of 51,567 members, 200 comparisons and
169 ranges -- and the engine's own reader confirms that every one of those 169
has its first bound strictly below its second.

The container reader underneath still knows none of this. It reads blocks and
members, and it is the layer a mod's unfamiliar script would still get through.

## The scripts are a tree, and they prove it themselves

The blocks are not a flat list. Five of the 24 numbers open a body that block
25 -- the one with no members -- closes, and that reading holds over the whole
corpus without a single exception:

> For openers `19`, `27`, `38`, `39` and `41` and closer `25`, **all 1,011
> scripts nest exactly**: the depth never goes negative, it returns to zero at
> the end of every script, and it reaches 12 at the deepest.

That is a fact about the files, not a reading of them. Ask which block numbers
have counts that add up to block 25's in every script, and the answer is
forced: the 1,011-by-23 matrix of per-script counts has full column rank, so
those five with a coefficient of one and every other number with zero is the
only solution there is -- over all real coefficients, not merely the first
subset someone tried. The ordered walk above then confirms it holds in
sequence and not only in total. It means a later interpreter reads a tree, and
it means block 25 carries no members because it needs none.

Note that block 41 opens a body and block 42 does not, although both carry a
single name from the same vocabulary -- 1,072 against 1,083 of them, with 525
and 515 distinct names. A named thing and a reference to it look alike in the
payload and differ in the structure.

## Some blocks say what they are

No block number is given a name in this repository. Four of them, though,
describe themselves through their own payloads, and each reading was checked
against the installation rather than assumed:

| Block | Shape | What the payload resolves to |
| --- | --- | --- |
| 20 | a member-7 name, then text | 1,117 of its 1,130 payloads are installed sound files, allowing the engine to choose between `.mp3` and `.wav`; the other 13 name nine paths the installation does not ship. One payload carries no extension at all, so choosing one is the engine's job |
| 48 | two strings, the first always `PLAY_ROFF` | **all 109** name an installed motion path, under `scripts/` with a `.rof` or `.roff` extension |
| 32 | one string | **all 61** name an installed script |
| 26 | two members, usually two strings | 6,850 of its 6,965 leading strings begin `SET_`; the rest are lowercase names like `fUsedBeam`, so the same block sets either a named property or a script's own variable |

Block 48 also pins a path convention the engine will need: a script names a
motion path without its directory or its extension, and `scripts/` and `.rof`
resolve every one of them.

Member 7's vocabulary is a closed set of nine, which is why this record calls
it a name rather than a string: `CHAN_AMBIENT`, `CHAN_ANNOUNCER`, `CHAN_AUTO`,
`CHAN_ITEM`, `CHAN_LESS_ATTEN`, `CHAN_VOICE`, `CHAN_VOICE_ATTEN`,
`CHAN_VOICE_GLOBAL` and `CHAN_WEAPON`. Four of those are channels Quake III
does not have, so this title's sound system will need them whether or not it
ever runs a script.

## The names in the scripts fall into three spaces

Scripts live in `scripts/<folder>/`, and for most folders there is a map of the
same name. Comparing each block's leading string against that map's own
`targetname` and `NPC_targetname` values separates the blocks cleanly, and
nothing but the installation was consulted to do it. Folders with no map of
their name are left out of every count below.

**Blocks 19, 30 and 34 name things in the map.**

| Block | Names a map entity |
| --- | --- |
| 34 | 498 of 527 (94.5%) |
| 30 | 1,128 of 1,197 (94.2%) |
| 19 | 1,114 of 1,457 (76.5%), with `player` set aside -- it appears 125 more times and is never in a map |

Those rates are floors rather than measurements: an entity a script addresses
may have been created by another script rather than placed in the map, and
`player` is the plain case of that.

**Blocks 41, 42 and 23 name something the script itself declares.** Block 41
is the one that opens a body, and it is the only one of the three that does:

> Every name blocks 42 and 23 carry -- **1,071 and 926 of them, without a
> single exception** -- is declared by a block 41 **in the same script**. Not
> one reaches into another script, even one beside it in the same folder.

**Blocks 46 and 47 name each other, and they do reach across scripts.** 509 of
510 block-47 names occur as a block-46 name: 483 in the same script and 26 in
another script in the same folder. Whatever those two are, they are a matched
pair, and unlike the first space they are not confined to one file.

Three spaces, then: one the map owns, one a single script owns, and one shared
across the scripts of a level. That is a fact about the corpus, and it is the
shape any interpreter will have to keep. No block is named here on the strength
of it.

## Raven's own numbers, read afterwards, agree throughout

Everything above was derived from the files. Raven's released sources, as
carried by the reference port this project pins, name the same numbers, and
they were read only after the derivation was finished. The two agree on every
one.

The numbers are three enumerations that run on from one another with no gap:
the base tokens, then eleven more, then the block ids. That is why a member
number and a block number share a space, and why a type tag is spelled with the
number of the kind it names. `code/compat/jka_icarus_ids.def` records them.

What was derived, beside the name the number turned out to carry:

| Derived from the scripts | Number | Raven's name |
| --- | --- | --- |
| Terminated text | 4 | `TK_STRING` |
| One float | 6 | `TK_FLOAT` |
| A name from a closed set of nine `CHAN_...` | 7 | `TK_IDENTIFIER` |
| Introduces three floats | 14 | `TK_VECTOR` |
| Four operators, each ending its block | 15-18 | `TK_GREATER_THAN`, `TK_LESS_THAN`, `TK_EQUALS`, `TK_NOT` |
| Introduces a type and a name | 36 | `ID_GET` |
| Introduces two floats, the first always below the second | 37 | `ID_RANDOM` |
| Introduces a name and a float | 49 | `ID_TAG` |
| The memberless block that closes the other five | 25 | `ID_BLOCK_END` |
| The five that open a body | 19, 27, 38, 39, 41 | `ID_AFFECT`, `ID_LOOP`, `ID_IF`, `ID_ELSE`, `ID_TASK` |
| The only block with an operator | 38 | `ID_IF` |
| Second string is an installed sound | 20 | `ID_SOUND` |
| First string is always `PLAY_ROFF` | 48 | `ID_PLAY` |
| String names an installed script | 32 | `ID_RUN` |
| Leading string almost always begins `SET_` | 26 | `ID_SET` |
| Strings name map entities | 30, 34 | `ID_USE`, `ID_REMOVE` |
| Declares the names two other blocks refer to, within one script | 41, 42, 23 | `ID_TASK`, `ID_DO`, `ID_WAIT` |
| The pair that names each other across a level's scripts | 46, 47 | `ID_SIGNAL`, `ID_WAITSIGNAL` |
| Variadic: one, three, four or six values | 35 | `ID_CAMERA` |
| One value, `-1` as readily as a count | 27 | `ID_LOOP` |

`TK_EQUALS` being the commonest of the four operators, 146 of 205, and `ID_LOOP`
taking `-1`, are the kind of thing that reads as a coincidence until the name
arrives. Both were counted before it did.

Five numbers the table names never appear in a compiled script: `ID_BLOCK_START`
and `ID_LOOPEND`, which the shared `ID_BLOCK_END` serves; `ID_REM`, a remark the
compiler drops; and `ID_DOWAIT` and `ID_EOF`. They are named anyway, so a script
that does use one is recognized rather than refused as nameless.

One number was **checked** against Raven's sources rather than derived: the file
version. Every installed script carries `1.57` and Raven's own reader compares
it for equality, refusing anything else, which is why this one does too.

A fourth enumeration continues the same space: the types, 51 to 70, which a
block is given when it has a kind to choose. Those were found as positions
where only a handful of values ever occur, before any name was available, and
every one lands inside its own group:

| Block, position | Values found | Raven's names |
| --- | --- | --- |
| `ID_AFFECT`, second | 1,503 and 201 of two values | `TYPE_FLUSH`, `TYPE_INSERT` -- its two affect types |
| `ID_MOVE`, third | 328 of one | `TYPE_ORIGIN` |
| `ID_SET`, sixth | 2 and 2 | `TYPE_ORIGIN`, `TYPE_ANGLES` -- its two set types |
| `ID_CAMERA`, first | 2,760 across nine | `TYPE_MOVE`, `TYPE_PAN`, `TYPE_ZOOM`, `TYPE_FADE`, `TYPE_ENABLE`, `TYPE_DISABLE`, `TYPE_SHAKE`, `TYPE_FOLLOW`, `TYPE_PATH` -- nine of its eleven camera types and nothing else |
| `ID_DECLARE`, first | 45, 10 and 5 | `TK_FLOAT`, `TK_STRING`, `TK_VECTOR` -- the three kinds a variable can be |

Nine different values in one position, all nine of them camera types, is not a
coincidence a reader could arrange for itself.

The names are facts and nothing more. No block has behaviour here, and the two
readers work from the shapes the scripts fix, not from the names.

## What the reader guarantees

`code/compat/jka_icarus_format.hpp` walks a script without allocating and
without reading a byte outside the buffer it was given:

- the identifier must match, and the version must match **to the bit**: Raven
  wrote a float there, and a script from a different compiler is a different
  contract, not a nearby one;
- a member count larger than the remaining bytes could hold a header apiece is
  refused as a count, not discovered as an overrun;
- a negative count or payload size is refused before any arithmetic uses it,
  because the unchecked conversion is what turns one into a very large one;
- a payload must lie inside the file;
- the file must end exactly where its last member does, so trailing bytes are a
  malformed script rather than something to ignore;
- a block with no members is still offered to the caller, so an empty block
  cannot be silently skipped;
- a member is only handed over as text when it ends in exactly one terminator
  and holds none before it, so a four-byte value is never read as a string.

`tests/jka_icarus_tests.cpp` writes its own scripts for all of this, including
the container's valid shapes; no retail file is used in the test.

The installation sweep then runs the real reader over the real files. All
1,011 scripts the engine would open read through it with **no refusals**,
alongside the 342 maps, meshes, skeletons and motion paths already covered:
1,353 assets in one pass, none rejected.

## What this does not show

- No block or member number is named or acted on. The structure is established
  -- which blocks open a body, how members group into values, which names come
  from the map and which from the script -- but a structure is not a meaning.
  Nothing is interpreted: no command runs, no variable is set and no entity is
  touched.
- The container reader knows none of it, and the value reader above it knows
  only which members introduce which others. Neither decides what a block is
  for, and neither enforces the nesting: that is recorded here because naming
  those five numbers would state as engine behaviour what is so far only true
  of one installation's scripts.
- A grammar with no exceptions over 52,699 members is strong evidence and not a
  specification. Another installation, a mod's own scripts or a different
  compiler version could carry a member number this corpus never uses, and the
  reader would accept it while none of these rules would cover it.
- The engine still refuses `G_ICARUS_INIT` and every other ICARUS import. This
  slice is a format boundary, not an interpreter.
- Single player's scripts were not examined separately: the corpus is one
  installation's `base/` paks, which both modes share.
- The one byte after each member count is `0` everywhere in this corpus, so
  nothing is known about what another value would mean.
