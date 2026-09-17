# Profile-specific menu mouse input

The shared input path selected absolute pointer presentation for every UI and
cgame menu. It discarded relative motion whenever either module owned input,
then passed drawable coordinates directly to the module's mouse export. This
matches Quake Live's contract, but Q3 and RTCW exports consume relative deltas.
Feeding positive positions into those exports repeatedly drives their cursor
toward its bottom/right limits, preventing normal menu hit testing.

Source observations: the original [Q3 UI](https://github.com/id-Software/Quake-III-Arena/blob/master/code/q3_ui/ui_atoms.c)
`UI_MouseEvent` and [RTCW SP UI](https://github.com/id-Software/RTCW-SP/blob/master/src/ui/ui_main.c)
and [RTCW MP UI](https://github.com/id-Software/RTCW-MP/blob/master/src/ui/ui_main.c)
`_UI_MouseEvent` add their arguments to the module-owned cursor. These were
inspected in the corresponding local reference trees under `E:\_SOURCE\_CODE`.
No external implementation text was incorporated. QL retains the absolute
contract and supersampling projection already documented in the client.

SDL, native Win32 and native X11 now select the existing relative, hidden,
confined pointer mode for Q3/RTCW module menus. The client forwards signed
deltas to their UI/cgame exports and drops absolute events for those modules,
including positions queued alongside clicks. QL modules retain absolute input
and public-resolution projection. Console/browser precedence, focus-loss
release and gameplay accumulation remain intact. The browser uses absolute
input in every profile. Existing `in_mouse` and `in_nograb` controls apply to
the restored relative mode.

Validation on Windows, based on commit
`0b6af9ba7a17d73bf27c5c016c00772a1a93899a` plus this working-tree change:

- `tests/menu_mouse_contract_tests.py` compiles and executes the actual client
  dispatch and all three backend ownership functions with synthetic module
  endpoints. It covers Q3, QL, RTCW SP and RTCW MP; native/QVM presentation;
  signed motion; absolute-event rejection; supersampling; overlapping catchers;
  console/browser precedence; focus loss; missing modules; input suppression;
  and return to gameplay. It never accesses input devices or loads a game.
- The 16 windowed-mouse and 89 input-system source checks pass. Meson's
  `fnql_input_compat`, `fnql_windowed_mouse_source` and
  `fnql_input_system_source` targets pass (three successes, zero failures).
- The shared Windows x86 release client and RTCW-enabled x86 development
  client build successfully with all three renderer targets. Build commands,
  executable hashes and focused-test logs are retained under
  `.tmp/menu-mouse/`.

This establishes Source/Unit/Build evidence. Live menu hover/click testing and
native Win32/X11 platform runtime checks remain open. No game was launched,
no user input was captured or injected, and no rendering claim is made. The
complete compatibility suite was not rerun for this bounded input correction.
