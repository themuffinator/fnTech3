# Entity Editor Guide

fnTech3 can edit a map's entities -- its spawn points, items, lights,
triggers, targets and every other `{ "key" "value" }` block a level holds --
from inside the running game, and save the result as an **entity override**:
a `maps/<map>.ent` file that the server loads in place of the entities stored
in the map's BSP. The map itself is never changed. The editor works the same
way in every game fnTech3 runs; what each game does with the entities is the
game's own.

- [Opening the editor](#opening-the-editor)
- [The screen](#the-screen)
- [Moving around](#moving-around)
- [Selecting](#selecting)
- [Editing](#editing)
- [Saving and applying](#saving-and-applying)
- [Entity overrides](#entity-overrides)
- [Keys and mouse](#keys-and-mouse)
- [Console commands](#console-commands)
- [Settings](#settings)
- [Notes for each game](#notes-for-each-game)
- [Troubleshooting](#troubleshooting)

## Opening the editor

Load a map, then open the console and type:

```
entityeditor
```

The same command closes it again. Binding it to a key is the quickest way to
work:

```
bind F9 entityeditor
```

The editor opens on any map running on your own machine -- a map you started
with `map` or `devmap`, a single-player level, or a listen server you host --
and while you watch a demo. On someone else's server it opens only when that
server has `sv_cheats 1`, because the editor's free camera sees places the
player is not.

While the editor is open:

- the world is drawn from the editor's own camera, which starts where the
  player was looking, and the game's HUD and notify lines are hidden;
- the keyboard and mouse belong to the editor (the console key still opens the
  console);
- a game menu that was open waits, undrawn, until the editor closes;
- a single-player game is paused (`cl_entityEditorPause`).

Closing the editor gives the view and the controls back to the game. Unsaved
edits stay in the editor until you reopen it on the same map.

## The screen

The editor's interface is built with [Dear ImGui](https://github.com/ocornut/imgui)
and drawn by the game's own renderer, so it looks the same under GLx, Vulkan
and RTX.

- **Menu bar** (top): **File** (save, apply, reload the map, revert, start
  again from the BSP's own entities, close), **Edit** (undo and redo -- each
  named for what it undoes -- new entity, duplicate, copy, cut, paste,
  delete, drop, move, turn), **Select** (all listed, none, previous, next,
  frame the selection, filter the list), **View** (snapping, the grid, labels,
  target links, the panels, the player's view, flying, the camera's speed and
  field of view, and the panels' size) and **Help**. **Save** and **Apply** sit on the bar too,
  and at its end the map's name, marked `*` while it has unsaved changes;
  hover it for the file it saves to. Each item shows its key, and the less
  obvious ones -- and every button in the panels -- explain themselves when
  hovered.
- **Entities** (left): every entity, numbered in the order the map lists
  them, with its classname and its `targetname` in quotes. A coloured bar
  shows its kind; an entity that stands nowhere -- no origin, no brush model
  -- is greyed. Type in the filter box to narrow the list: each word must
  appear, in any case, within some key or value, and `key=value` needs one
  pair whose key holds the one and whose value the other. So `light _color`
  lists coloured lights, `team_` every team entity, and `model=*3` the
  owner of brush model 3 (with those of `*30` and up, where a map has
  them).
- **Keys** (right): the selected entity's keys and values, each in an edit
  box, with what a game would object to above them, and buttons that frame,
  move, drop, turn, duplicate and delete the selection. Below the keys are the
  entities it targets and those that target it; click one to select it. With
  several selected, a row above the keys sets one key on all of them.
- **View** (centre): every entity that stands somewhere, drawn over the world
  in the colour of its kind -- the nearest as boxes with their classnames
  above them, those farther off as dots:

  | Colour | Kind |
  |---|---|
  | green | spawn points (`info_player_*`, anything else with `spawn` in its name but a `func_` one) |
  | cyan | items, weapons, ammunition, powerups and flags |
  | yellow, or its own `_color` | lights |
  | orange | triggers |
  | violet | targets, `info_*`, paths and scripts |
  | blue | brush entities (`func_*`, anything with a `"model" "*N"`) |
  | white | `misc_*` |

  A brush entity's box is its brushes' bounds. Any other entity's box is the
  one its game gives it about its origin, so that it sits where the game
  puts the entity:

  | Game | Spawn points | Items | Others |
  |---|---|---|---|
  | Quake III, Quake Live, the Q3Test and IHV releases, Elite Force, the Jedi games | the player, 32 wide, from 24 below the origin to 32 above | 30 about the origin | 16 about it |
  | Return to Castle Wolfenstein, Enemy Territory | the player, 36 wide, from 24 below to 48 above | 20 wide and 10 high, standing on the origin; 20 about it when it hangs (`spawnflags` 1) | 16 about it |
  | Alice, F.A.K.K.2, Medal of Honor | 32 wide, standing on the origin, 96 high | 20 wide and 20 high, standing on it | 16 wide and high, standing on it; scripts' origins, triggers and `info_`, `target_`, `path_` and `func_` points 16 about it; path nodes their own boxes, on it |

  A light's box is 12 about its origin in every game. An arrow shows the way
  an entity with an `angle` or `angles` faces; an `angle` of -1 or -2 points
  up or down and has none. Lines run from each entity to the entities it
  targets (`target`, `target2` to `target9`, `killtarget` and every key
  ending in `target`), brighter for the selected entity's. Entities
  the camera cannot see from where it is are drawn faintly. Selected entities
  are drawn in gold, the primary one -- the one the keys panel shows --
  brightest.

  However crowded the view, the editor draws no more than a frame can hold
  beside the game and the console: the 48 nearest entities as boxes, the
  next 384 as dots, up to 32 labels, and 48 links, the selected entity's
  first. Selected entities are boxes wherever they are, drawn over the rest.
  `entityeditor stats` says what the last frame drew.
- **Status line** (bottom): what the last action did, whether the map runs on
  a local server, on a server reached by rcon, or neither (save only), the
  camera's position and flying speed.

The panels grow with the screen; **View > Panel size** or
`cl_entityEditorScale` sets their size yourself.

## Moving around

Hold the **right mouse button** over the view and the mouse turns it; while
it is held, fly with **W A S D**, go down and up with **Q** and **E** (or
**C** and **Space**), hold **Shift** to go faster and **Ctrl** to go slower,
and turn the **mouse wheel** to change the flying speed. Your own movement
bindings (`+forward`, `+moveleft` and so on) fly the camera too.

**Tab** flies without holding the button; a crosshair then marks the
centre of the view, and a left click selects what it points at. **Tab** or
**Escape** stops flying.

Without the right button, the mouse wheel moves the camera forward and back,
**F** frames the selected entity, and **Home** returns to the player's view.

The camera has a field of view of its own: 90 degrees across a 4:3 screen,
its height kept on a wider one as the games' own view is, so 106 degrees
across a 16:9 screen (**View > Field of view**, `cl_entityEditorFov`). A
scope's zoom or an underwater sway in the game's view does not carry over,
and the boxes are drawn with the same view the world is.

## Selecting

**Click** an entity in the view or in the list to select it alone. Clicking
empty view deselects; so does **Escape**.

Several entities can be selected at once, and every edit below then applies
to all of them:

- **Ctrl+click** adds an entity to the selection, or takes it out;
- **Shift+click** in the view adds one; in the list it selects every entity
  from the last one clicked to this one (**Ctrl+Shift+click** adds them);
- **drag a box** over empty view to select every entity drawn inside it
  (with **Ctrl** or **Shift** held, the box adds to the selection);
- **Ctrl+A** selects every entity the list shows -- filter first to select,
  say, every `item_health`.

The last entity chosen is the **primary** one: the keys panel shows its keys
and the axis arrows stand on it. **,** and **.** step to the previous and next
entity in the list; **Double-click** frames one.

## Editing

**Move** the selection by dragging one of its entities: they slide level,
together, at their own heights. (Pressing one of several selected entities
keeps the others selected so that they can be dragged; let go without
moving, and the click selects that one alone.) The primary entity's three
arrows -- red X, green Y, blue Z -- move them along one axis each; drag the
small square where they meet to slide them level. Moves snap to the grid
(**G** turns snapping off, **[** and **]** halve and double the grid). The
arrow keys nudge the selection a grid step across the floor along the map
axis nearest the way the camera faces, **Page Up** and **Page Down** up and
down, with **Shift** four steps at a time. **M** moves the primary entity to
the surface under the cursor and the rest with it, and **End** sets each
selected entity on the floor below it: its box comes down until some part
of it rests on the world.

An entity put on a surface -- moved there with **M**, made, or pasted --
rests its box against it: on a floor, a spawn point's box stands on the
floor and an item's sits on it, whichever way its game centres it. Along
the surface the entity goes on the grid; across it, to the grid step
nearest the surface on the near side, or, with snapping off, flush with
it.

**Turn** the selection with **Q** and **E** (15 degrees; 90 with **Shift**).
An entity with an `angles` key turns its yaw; otherwise its `angle` key
changes, and is added when it has none. An `angle` of -1 or -2, which sends
a door, a plat or a push straight up or down, stays as it is.

**Edit a key or value** of the primary entity by clicking its box, typing,
and pressing **Enter** or clicking elsewhere; **Escape** abandons the edit
and **Tab** moves to the next box. **Ctrl+A** selects all of a box's text,
and **Ctrl+C**, **Ctrl+X** and **Ctrl+V** copy, cut and paste it. Remove a
key with its **x**. Add one in the empty row below the keys: type the key,
then the value, and press **Enter**. To set a key on every selected entity,
use the row the keys panel shows above them while several are selected.

**Make** an entity with **N** (or **Edit > New entity**): type a classname --
the list under the box offers the map's own, the most used first -- and press
**Enter**. The new entity stands where the centre of the view meets the
world, its box resting on the surface there. **Ctrl+D** (**Duplicate**) copies each selected entity a grid step
aside, and the copies become the selection; **Delete** removes the
selection.

**Ctrl+C** puts the selected entities on the clipboard as entity text, the
`{ "key" "value" }` blocks an `.ent` file holds, and **Ctrl+X** cuts them.
**Ctrl+V** pastes entity text from the clipboard -- copied in this map,
another map or a text editor -- at the end of the list, and selects what it
pasted; a single pasted entity with an `origin` stands where the centre of
the view meets the world, and several keep their own origins. While an edit
box has the keyboard these keys copy, cut and paste its text instead.

**Undo** with **Ctrl+Z** and **redo** with **Ctrl+Y** (or **Ctrl+Shift+Z**).
Every change can be undone -- a whole drag, or an edit of the whole
selection, as one step -- and undo brings back the selection with it.

Some entities cannot be copied or deleted: the first entity, the
`worldspawn`, holds the map's own settings and every level needs it; and a
brush entity's model belongs to it alone, so a copy would have no model of
its own. A paste leaves out any `worldspawn` or brush entity it holds. A key
or value cannot contain a double quote, and cannot begin with `}`, because
the games would read the text differently.

## Saving and applying

**Ctrl+S** (or **Save**) writes the whole entity list to `maps/<map>.ent` in
your home directory -- in the game directory under `fs_homepath`, or the mod
directory `fs_game` names -- beside where the map's BSP would be. A map in a
subdirectory keeps it: Medal of Honor's `dm/mohdm1` saves to
`maps/dm/mohdm1.ent`. If the file cannot be written, the status line says so
and nothing counts as saved.

**F5** (or **Apply**) saves and restarts the level, so that the game spawns
the entities you edited:

- when you changed only keys of entities the level already has -- moved them,
  renamed their targets, set their values -- the Quake III family restarts
  the level in place with `map_restart 0`;
- when entities were added, removed or reordered, or a key that decides
  whether or as what one spawns changed (`classname`, `spawnflags`, and the
  game-type filters `gametype`, `notsingle`, `notteam`, `notfree` and the
  like), it loads the map again instead, as the server started it (`devmap`,
  `map`, or `spdevmap` and `spmap` for a single-player game). A client game
  keeps what it knows of each entity number across `map_restart`, and Return
  to Castle Wolfenstein's single player hangs when those numbers change;
- Alice, F.A.K.K.2 and Medal of Honor restart the level with `restart`, which
  loads it again; Elite Force's single player and the Q3Test releases before
  1.06, which have no `map_restart`, load the map again.

On a server you administer -- a dedicated server on this machine or another
-- set `rconPassword` to the server's password, and **Apply** and **Reload
map** save, then ask the server over rcon to load the map again (`devmap`,
or `restart` in Alice, F.A.K.K.2 and Medal of Honor). The server reads
`maps/<map>.ent` from its own home, so it uses your edits when it shares
this home or once you copy the file there; its reply tells the editor which,
and the status line says "the server loaded maps/<map>.ent" or that it did
not.

The editor stays open with its camera where it was, and the status line says
whether the server loaded your file. **Ctrl+F5** (or **File > Reload map**)
saves and loads the map again from the start; use it when the game reads
something only when a map first loads.

**Revert** goes back to the entities the level is running, and **BSP
entities** to the map's own entities, ignoring the override; both are one
change you can undo. Saving after **BSP entities** writes the original
entities as the override; delete the `.ent` file to stop overriding the map
at all.

If the map changes while edits are unsaved, the editor writes them to
`maps/<map>.ent.unsaved` and says so, then reads the new map.

## Entity overrides

When a map starts, the server looks for `maps/<map>.ent` and, if it finds
one, hands it to the game in place of the BSP's entities. The editor writes
these files, but any text editor can: an `.ent` file is a map's entity text,
one `{ }` block per entity, the first being the `worldspawn`. The files are
found like any other game file, so they can sit in your home directory or be
shipped inside a `.pk3`.

- `sv_entityOverrides 1` (the default) reads them; `0` always uses the BSP's
  own entities.
- The file is checked first, as the games' own readers would read it. If a
  game would refuse it -- an entity without a closing brace, a key with no
  value, a stray token, a first entity that is not the `worldspawn`, or a NUL
  byte -- the server prints why and which line, and uses the BSP's entities
  instead:

  ```
  Ignored maps/q3dm17.ent, line 1: the entity begun here has no closing brace. Using the BSP's entities.
  ```
- On success it prints `Loaded entity override maps/<map>.ent (<bytes>
  bytes, <count> entities).`
- Every game reads the override: the server's game module, its bots (which
  look for the items the level spawned), and -- when the server runs in the
  same process as your client -- the client game too, for the games whose
  client spawns entities of its own (Enemy Territory, Return to Castle
  Wolfenstein, Jedi Academy).
- Quake Live's own `sv_altEntDir` still works as it did in the retail game:
  when it is set and holds a file for the map, that file is used, unchecked,
  before `maps/<map>.ent`. `sv_dumpEntities` still writes `ents/<map>.ent`.
- `sv_saveentfile` writes the entities the running level was given to its
  `maps/<map>.ent` -- a start for a hand-written override on a dedicated
  server. It says so if the file cannot be written.

The map's BSP keeps the entity text its lighting and visibility were compiled
with. The renderer still reads a few worldspawn keys from the BSP alone, such
as the light grid's `gridsize`, which must match the lighting stored in the
map; changing them in an override has no effect.

## Keys and mouse

| Input | Action |
|---|---|
| Left click | select (view or list); empty view deselects |
| Ctrl+click, Shift+click | add to or take from the selection; Shift in the list: a range |
| Ctrl+Shift+click | in the list, add a range |
| Left drag | move the selection level; on an arrow, along that axis |
| Drag on empty view | select what a box holds (Ctrl or Shift: add) |
| Double click | frame the entity |
| Right button held | look; W A S D, Q E, Space, C fly; wheel sets the speed |
| Wheel | move forward and back (view), scroll (panels) |
| Tab | fly without holding the button |
| F | frame the selected entity |
| Arrows, Page Up, Page Down | nudge the selection a grid step (Shift: four) |
| Q, E | turn the selection 15 degrees (Shift: 90); while flying, down and up |
| M | move the selection to the point under the cursor |
| End | set the selection on the floor |
| N | make an entity |
| Ctrl+D | duplicate the selection a grid step aside |
| Ctrl+C, Ctrl+X, Ctrl+V | copy, cut and paste entities as text |
| Delete | delete the selection |
| Ctrl+Z, Ctrl+Y | undo, redo |
| Ctrl+A | select every entity the list shows |
| , and . | the previous and next entity in the list |
| G, [ and ] | snapping, smaller and larger grid |
| L, K | labels (off, near, all), links |
| Ctrl+F | the list's filter |
| Home | back to the player's view |
| Ctrl+S | save |
| F5, Ctrl+F5 | save and restart the level; save and reload the map |
| F1 | this list, on the screen |
| Escape | leave a box, stop flying, deselect, then close |

## Console commands

Everything the editor does can be typed or scripted, open or not:

| Command | Effect |
|---|---|
| `entityeditor` | open or close |
| `entityeditor open`, `close` | |
| `entityeditor save`, `apply`, `reload` | save; save and restart the level; save and load the map again (on another server, over rcon) |
| `entityeditor revert`, `bsp` | back to the level's entities, or the BSP's own |
| `entityeditor undo`, `redo` | |
| `entityeditor select <n or targetname>...` | select by list number or `targetname`, one or several, and print one |
| `entityeditor select all`, `select none` | every entity the list shows, or none |
| `entityeditor next`, `prev` | the next or previous entity in the list |
| `entityeditor set <key> <value>` | set a key on the selected entities |
| `entityeditor unset <key>` | remove a key from them |
| `entityeditor new <classname>` | make one where the view looks |
| `entityeditor duplicate` (or `copy`), `delete`, `drop`, `look` | |
| `entityeditor move <x> <y> <z>` | place the primary entity there, the rest about it |
| `entityeditor nudge <x> <y> <z>` | move the selection by that much |
| `entityeditor turn <degrees>` | |
| `entityeditor find <text>` | filter the list |
| `entityeditor print` | print the selected entities |
| `entityeditor list [text]` | print the entities, or those matching |
| `entityeditor camera <x> <y> <z> [pitch yaw]` | place the editor's camera |
| `entityeditor stats` | what the last frame drew: boxes, dots and renderer commands |
| `entityeditor help` | list these |
| `sv_saveentfile` | write the running level's entities to `maps/<map>.ent` |

## Settings

| Cvar | Default | Meaning |
|---|---|---|
| `sv_entityOverrides` | `1` | load `maps/<map>.ent` in place of the BSP's entities |
| `cl_entityEditorGrid` | `8` | the grid moves snap to, in units |
| `cl_entityEditorSnap` | `1` | snap moves to the grid |
| `cl_entityEditorLabels` | `1` | classnames over entities: 0 none, 1 near ones, 2 all |
| `cl_entityEditorLabelDistance` | `1536` | how near an entity is labelled, in units |
| `cl_entityEditorLinks` | `1` | draw every target link, not only the selected entity's |
| `cl_entityEditorPause` | `1` | pause a single-player game while the editor is open |
| `cl_entityEditorSpeed` | `400` | the camera's flying speed, in units a second |
| `cl_entityEditorFov` | `90` | the camera's field of view across a 4:3 screen; a wider one keeps its height |
| `cl_entityEditorScale` | `0` | panel and text size; 0 follows the screen height |

## Notes for each game

The editor reads and writes entity text the way the engine hands it to every
game's module, so it shows each game's entities as that game sees them. What
an entity does is the game's own: its classnames, its keys, and its limits.

- **Key limits.** Quake III-family game modules keep at most 64 keys an entity
  and a fixed number of bytes of keys and values -- 4,096 in Quake III, Quake
  Live and Jedi Academy multiplayer, 2,048 in Return to Castle Wolfenstein,
  Enemy Territory, Elite Force and the Jedi games' single player -- and every
  key or value at most 1,023 bytes. The keys panel says when an entity
  exceeds them. Alice, F.A.K.K.2 and Medal of Honor read the whole text
  themselves and set no such limits.
- **Quake III Arena** presents Quake Live's `advertisement` entities as
  `func_static`, in an override as in a BSP, and the editor shows them as the
  game receives them.
- **Quake Live**: `sv_altEntDir` comes first, as described above, and a map
  loaded again keeps the factory it was started with.
- **Return to Castle Wolfenstein's single player** loads the map again
  whenever the entities to spawn change (see
  [Saving and applying](#saving-and-applying)).
- **Alice and F.A.K.K.2** load a level again to restart it, so Apply takes a
  few seconds; a map started with a spawn target (`map garden1$start`) saves
  to `maps/garden1.ent`.
- **Medal of Honor** keeps its multiplayer maps in `maps/DM` and its
  objective maps in `maps/obj`; their overrides sit beside them, named as
  the map was loaded (`map dm/mohdm1` saves `maps/dm/mohdm1.ent`, which
  Windows finds however the map is spelled; on Linux, load the map with the
  same spelling each time).
- **Elite Force single player** restarts by loading the map again.
- **Jedi Academy multiplayer**'s client cannot yet host a level in the same
  process, so the editor works on a dedicated server you join with
  `sv_cheats 1`: it edits and saves `maps/mp/<map>.ent` in your home
  directory, and with `rconPassword` set Apply asks the server to load it.
  The client itself cannot yet follow a server's map change -- any change,
  not only the editor's -- so reconnect afterwards.
- **Q3Test** releases before 1.06 had no `map_restart`, so Apply loads the
  map again.
- **The Quake III IHV Test, Jedi Outcast, and Jedi Academy's single player**
  cannot yet play a level in fnTech3's client, so the editor does not open
  there. The IHV Test's and Jedi Outcast's multiplayer dedicated servers load
  `maps/<map>.ent` like any other: write it with a text editor, or start
  from what `sv_saveentfile` writes.
- Items in the Quake III family fall to the floor when they spawn unless their
  `spawnflags` hang them in the air, so an item placed above the floor lands
  on it.

## Troubleshooting

- **"The entity editor needs a local server, a demo or a server with sv_cheats
  1."** You are connected to another server without cheats; host the map
  yourself (`devmap <map>`) to edit it.
- **"applying needs the map running on this machine's server ... or
  rconPassword"**: you can edit and save while watching a demo or playing on
  another server; a local server restarts the level itself, and another
  server does when you set `rconPassword` to its password.
- **"the server did not say it loaded maps/<map>.ent"**: the server reloaded
  from its own home, which does not hold your file; copy the saved file
  into the server's home (the same path under its game directory) and Apply
  again.
- **"saved, but sv_entityOverrides is 0"**: set `sv_entityOverrides 1`.
- **"the server did not load maps/<map>.ent"**: the console says why -- most
  often a line a game would refuse, or Quake Live's `sv_altEntDir` holding a
  file for the map first.
- **Nothing changes after Apply** for something the game reads only when a map
  first loads (some games spawn client-side models or read worldspawn
  settings once): use **Reload map** instead.
- **An entity is not drawn by the game while the editor flies elsewhere**: the
  server sends the client only what the player could see, so the game draws
  entities near the player's own position; the editor's boxes show every
  entity wherever the camera is.
- **"This build has no entity editor"**: the engine was built without Dear
  ImGui; run `meson subprojects download imgui` before building with Make or
  CMake.
