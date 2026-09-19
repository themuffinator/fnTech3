# Elite Force single-player menus

## Scope

This record covers hosting the retail single-player menus, `efuix86.dll`,
in fnTech3's client: their function tables, how menus open, key events, the
connection screen and the order of each frame, plus the single-player render
entity and glconfig records they share with the cgame. The cgame half of
`efgamex86.dll`, savegames and their thumbnails, and Bink movies belong to
later slices. The profile stays behind `-Dstvef-development=true`.

The table layouts are in `code/compat/stvef_sp_ui_api.h`; the host is
`code/client/client_stvef_sp_ui.hpp`.

## The boundary

| Contract | Retail `stvoy.exe` 1.20 | fnTech3 |
| --- | --- | --- |
| Loading | Loads `efuix86.dll` and calls `GetUIAPI`, which takes nothing; a missing table is fatal (`Sys_GetUIAPI failed`). `CL_InitUI` (`0x40a4a0`) copies the 12 exports (0x30 bytes), fills 59 imports (0xec bytes) on its stack and calls `UI_Init(2, imports)` | `VM_CreateTable` finds the library and resolves `GetUIAPI`; the host fills the imports in retail's order and initializes the menus with version 2. The client drives them through `uivm` like any UI module |
| Scissor | The `R_ScissorPic` slot (+0x7c) is left empty | Left empty |
| Opening menus | By name through the export at +0x18: `main` when disconnected with no full-screen menu up (`0x40a1e6`); `ingame` for Escape in a level, with `HOLO` on a `_holo` map (`CL_KeyEvent` `0x407129`); `needcd`; `genericmenu` or, on a holodeck map, `genericholomenu`, and `endholomenu`, from the console commands of those names (`0x40a8b0`, `0x40a900`); none closes them | The engine's menu numbers map to these names, and the two commands are registered while the menus run |
| Whether a menu may open | The menus ask `SG_GameAllowedToSaveHere` with `inCamera` set before opening any menu. Retail (`0x4272e0`) then asks only the game, which refuses during its camera scenes, and allows it when no game runs; without `inCamera` it also requires a running level whose name does not start with `_` and a living player | The same logic on the server's game. Answering no, as a placeholder did at first, leaves every menu shut and the screen black |
| Keys | Presses only (`0x407198`); characters with 0x400 added; Escape (0x1b) to the export at +8 | Presses only, as for Holomatch, with Elite Force's own key numbers |
| Each frame | `UI_GetActiveMenu(0, &fullscreen)` (`0x40a1ae`). Without a full-screen menu: disconnected opens `main`; connecting, challenging and connected draw `UI_DrawConnect` (+0x20); loading and primed draw the cgame's information screen and then `UI_DrawConnectText` (+0x24); active draws the game; a cinematic plays. Then `UI_Refresh(realtime)` every frame (+0x10); the menus draw only while they hold the keys | The same order for single player: the connection screen first, then a refresh every frame, which keeps the menus' clock current while none is open |
| Connection text | `UI_UpdateConnectionString("(%i)")` with the attempt count at each retransmission (`0x407a60`, `0x407b31`), `""` when the gamestate arrives (`CL_ParseGamestate` `0x4097e4`), and `UI_UpdateConnectionMessageString` with a server's `print` (`0x407e7a`); the menus copy each and redraw | The same three calls; Quake III's menus read these from the client state instead |
| Client state | `GetClientState` (`0x40a740`) returns the single-player state, which has no authorizing step: active is 7, cinematic 8 | The engine's states mapped by name; authorizing reads as connecting |
| Glconfig | `GetGlconfig` (`0x40a750`) copies 0x144c bytes, with `maxActiveTextures` for the texture units and `clampToEdgeAvailable` after the anisotropy flag | Filled field by field from the engine's; every renderer fnTech3 loads filters anisotropically and clamps to edge |
| Looping sound | `S_StartLocalLoopingSound` (`0x420970`) loops a sound on the listener's entity with no position; the SDK menus never call it | A loop on entity 0, the single-player listener |
| Save imports | Read saves' thumbnails and comments, validate saves, store the comment, capture the screen | Until the savegame slice: no saves, no thumbnail |

## Render records

Single player's reference entity is Quake III 1.17's, 152 bytes: the eight
entity types with Quake III's numbers, Quake III's flags, then a lighting
direction and a sprite's radius and rotation in their own fields. Retail's
renderer adds the same fixed ambient light to every entity (`0x45b818`), so
none of Holomatch's lighting flags apply, and it works out each entity's
lighting direction itself (`R_SetupEntityLighting` `0x45b740`) without reading
the record's. The record is refused outright for an unknown type or a
non-boolean axis flag, and any bit the engine gives another meaning (its
crosshair flag, 0x10) is dropped. The view is Holomatch's.

`RF_CAP_FRAMES` (0x400) holds a one-shot animation on its last frame:
retail's MD3 path caps both frames at the last before it would wrap them
(`R_AddMD3Surfaces` `0x461f8b`). The engine has the same flag, with the same
number, in all three renderers; the cgame sets it on props that animate once
and on explosion models.

## Results

A developer client with the `stvef-sp` profile, 1,024 by 768 in a window,
loads the retail menus from the installation, shows the `ACCESSING VOYAGER
LCARS` splash and then the `ELITE FORCE : MAIN MENU` with its animated stellar
cartography, button highlights and `ST:V EF V1.20`, captured with the engine's
`screenshot` command. The installation was left unchanged.

## Tests

- `stvef_sp_menu_tests` pins both tables in retail's order, every import but
  the scissor filled, the menu names and the holodeck variant, the client
  states, the order of the checks in `SG_GameAllowedToSaveHere`, the menu
  commands, press-only keys, the connection text calls and the frame order.
- `fntech3_stvef_abi_layouts` pins the export offsets retail calls and the
  import offsets it fills.
- `fntech3_stvef_render_records` converts the single-player entity (every
  type, refused types and axis flags, every flag, the sprite fields), reads
  the shared view and fills the single-player glconfig without writing past
  it.

## Remaining

Savegames with their thumbnails; Bink movies. The cgame half of
`efgamex86.dll`, which draws the game and the loading screen, has
[its own record](2026-09-19-stvef-sp-cgame.md).
