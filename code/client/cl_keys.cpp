/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
extern "C" {
#include "client.h"
}

#include "client_cpp.h"
#include "input_compat.hpp"
#include "../compat/q3_key_adapter.h"
#include "../compat/rtcw_key_adapter.h"
#include "../compat/rtcw_key_names.hpp"
#include "../compat/stvef_key_adapter.h"
#include "../compat/stvef_key_names.hpp"
#include "../compat/wolfet_key_adapter.h"
#include "client_rtcw_camera_input.hpp"

#include <algorithm>
#include <array>
#include <cstring>

using fnql::ScopedZoneMemory;

namespace {

fnql::input::Utf16Decoder textInputDecoder;

} // namespace

/*

key up events are sent even if in console mode

*/

extern "C" {
field_t		g_consoleField;
field_t		chatField;
qboolean	chat_team;

int			chat_playerNum;
}

static void Field_CharEvent( field_t *edit, int ch );

/*
=============================================================================

EDIT FIELDS

=============================================================================
*/


/*
===================
Field_Draw

Handles horizontal scrolling and cursor blinking
x, y, and width are in pixels
===================
*/
static void Field_VariableSizeDraw( field_t *edit, int x, int y, int width, int size, qboolean showCursor,
		qboolean noColorEscape ) {
	int		len;
	int		drawLen;
	int		prestep;
	int		cursorChar;
	std::array<char, MAX_STRING_CHARS> str;
	char	*s;
	int		i;
	int		curColor;

	drawLen = edit->widthInChars - 1; // - 1 so there is always a space for the cursor
	len = strlen( edit->buffer );

	// guarantee that cursor will be visible
	if ( len <= drawLen ) {
		prestep = 0;
	} else {
		if ( edit->scroll + drawLen > len ) {
			edit->scroll = len - drawLen;
			if ( edit->scroll < 0 ) {
				edit->scroll = 0;
			}
		}
		prestep = edit->scroll;
	}

	if ( prestep + drawLen > len ) {
		drawLen = len - prestep;
	}

	// extract <drawLen> characters from the field at <prestep>
	if ( drawLen >= MAX_STRING_CHARS ) {
		Com_Error( ERR_DROP, "drawLen >= MAX_STRING_CHARS" );
	}

	std::copy_n( edit->buffer + prestep, drawLen, str.data() );
	str[ drawLen ] = '\0';

	// color tracking
	curColor = COLOR_WHITE;

	if ( prestep > 0 ) {
		// we need to track last actual color because we cut some text before
		s = edit->buffer;
		for ( i = 0; i < prestep + 1; i++, s++ ) {
			if ( Q_IsColorString( s ) ) {
				curColor = *(s+1);
				s++;
			}
		}
		// scroll marker
		// FIXME: force white color?
		if ( str[0] ) {
			str[0] = '<';
		}
	}

	// draw it
	if ( size == smallchar_width ) {
		SCR_DrawSmallStringExt( x, y, str.data(), g_color_table[ ColorIndexFromChar( curColor ) ],
			qfalse, noColorEscape );
		if ( len > drawLen + prestep ) {
			SCR_DrawSmallChar( x + ( edit->widthInChars - 1 ) * size, y, '>' );
		}
	} else {
		if ( len > drawLen + prestep ) {
			SCR_DrawStringExt( x + ( edit->widthInChars - 1 ) * BIGCHAR_WIDTH, y, size, ">",
				g_color_table[ ColorIndex( COLOR_WHITE ) ], qfalse, noColorEscape );
		}
		// draw big string with drop shadow
		SCR_DrawStringExt( x, y, BIGCHAR_WIDTH, str.data(), g_color_table[ ColorIndexFromChar( curColor ) ],
			qfalse, noColorEscape );
	}

	// draw the cursor
	if ( showCursor ) {
		if ( cls.realtime & 256 ) {
			return;		// off blink
		}

		if ( key_overstrikeMode ) {
			cursorChar = 11;
		} else {
			cursorChar = 10;
		}

		i = drawLen - strlen( str.data() );

		if ( size == smallchar_width ) {
			SCR_DrawSmallChar( x + ( edit->cursor - prestep - i ) * size, y, cursorChar );
		} else {
			// Host TTF text has no glyphs for the charset's control-byte
			// cursors. Use the retail field cursor shapes explicitly.
			str[0] = key_overstrikeMode ? '_' : '|';
			str[1] = '\0';
			SCR_DrawBigString( x + ( edit->cursor - prestep - i ) * BIGCHAR_WIDTH, y, str.data(), 1.0, qfalse );
		}
	}
}


void Field_Draw( field_t *edit, int x, int y, int width, qboolean showCursor, qboolean noColorEscape )
{
	Field_VariableSizeDraw( edit, x, y, width, smallchar_width, showCursor, noColorEscape );
}


void Field_BigDraw( field_t *edit, int x, int y, int width, qboolean showCursor, qboolean noColorEscape )
{
	Field_VariableSizeDraw( edit, x, y, width, bigchar_width, showCursor, noColorEscape );
}


static bool Field_IsUtf8ContinuationByte( unsigned char ch ) {
	return ( ch & 0xc0 ) == 0x80;
}


/*
==================
Field_InsertUtf8Scalar

Inserts one encoded scalar as a transaction. A field that does not have room
for the complete scalar is left untouched, so it can never end in a truncated
UTF-8 sequence.
==================
*/
static bool Field_InsertUtf8Scalar( field_t *edit,
	const unsigned char *bytes, std::size_t byteCount ) {
	constexpr int fieldCapacity = MAX_EDIT_LINE - 2;

	if ( !edit || !bytes || byteCount == 0 || byteCount > 4 ) {
		return false;
	}

	const int len = static_cast<int>( strlen( edit->buffer ) );
	if ( edit->cursor < 0 || edit->cursor > len ) {
		return false;
	}

	const int insertLength = static_cast<int>( byteCount );
	int overwriteEnd = edit->cursor;
	if ( key_overstrikeMode && !Field_IsUtf8ContinuationByte( bytes[0] ) &&
		overwriteEnd < len ) {
		++overwriteEnd;
		while ( overwriteEnd < len && Field_IsUtf8ContinuationByte(
			static_cast<unsigned char>( edit->buffer[overwriteEnd] ) ) ) {
			++overwriteEnd;
		}
	}

	const int newLength = len - ( overwriteEnd - edit->cursor ) + insertLength;
	if ( newLength > fieldCapacity ) {
		return false;
	}

	// Move the suffix, including its terminator, once the full operation has
	// been proven to fit. memmove covers both insert and overstrike overlap.
	std::memmove( edit->buffer + edit->cursor + insertLength,
		edit->buffer + overwriteEnd,
		static_cast<std::size_t>( len - overwriteEnd + 1 ) );
	std::memcpy( edit->buffer + edit->cursor, bytes, byteCount );

	for ( int i = 0; i < insertLength; ++i ) {
		++edit->cursor;
		if ( edit->cursor >= edit->widthInChars ) {
			++edit->scroll;
		}
	}
	return true;
}


/*
================
Field_Paste
================
*/
static void Field_Paste( field_t *edit ) {
	ScopedZoneMemory clipboardText( Sys_GetClipboardData() );
	char *cbd = clipboardText.as<char>();

	if ( !cbd ) {
		return;
	}

	// Clipboard bytes are data, never editor commands: a pasted Ctrl-V byte
	// must not recursively paste, and Ctrl-C/A/E/H must not mutate field state.
	// Ignore single-line control bytes; insert printable ASCII, valid multi-byte
	// scalars, and replacement characters for malformed UTF-8 atomically.
	static constexpr unsigned char replacementCharacter[] = {
		0xefu, 0xbfu, 0xbdu
	};
	const std::size_t pasteLen = strlen( cbd );
	for ( std::size_t i = 0; i < pasteLen; ) {
		const auto *bytes = reinterpret_cast<const unsigned char *>( cbd + i );
		const fnql::input::Utf8DecodeResult decoded =
			fnql::input::DecodeUtf8( bytes, pasteLen - i );
		if ( decoded.valid && decoded.size > 1 ) {
			if ( !Field_InsertUtf8Scalar( edit, bytes, decoded.size ) ) {
				break;
			}
			i += decoded.size;
		} else if ( !decoded.valid ) {
			if ( !Field_InsertUtf8Scalar( edit, replacementCharacter,
					sizeof( replacementCharacter ) ) ) {
				break;
			}
			i += decoded.size;
		} else {
			const unsigned char byte =
				static_cast<unsigned char>( cbd[i] );
			if ( byte >= ' ' && byte != 0x7fu &&
				!Field_InsertUtf8Scalar( edit, &byte, 1 ) ) {
				break;
			}
			++i;
		}
	}
}


static void Field_DeletePreviousCharacter( field_t *edit ) {
	int len = static_cast<int>( strlen( edit->buffer ) );

	while ( edit->cursor > 0 ) {
		const unsigned char deletedByte = static_cast<unsigned char>( edit->buffer[edit->cursor - 1] );
		std::copy( edit->buffer + edit->cursor, edit->buffer + len + 1,
			edit->buffer + edit->cursor - 1 );
		--edit->cursor;
		--len;
		if ( edit->cursor < edit->scroll ) {
			--edit->scroll;
		}
		if ( !Field_IsUtf8ContinuationByte( deletedByte ) ) {
			break;
		}
	}
}


static void Field_DeleteCurrentCharacter( field_t *edit ) {
	int len = static_cast<int>( strlen( edit->buffer ) );

	while ( edit->cursor < len ) {
		std::copy( edit->buffer + edit->cursor + 1, edit->buffer + len + 1,
			edit->buffer + edit->cursor );
		--len;
		if ( edit->cursor >= len ||
			!Field_IsUtf8ContinuationByte( static_cast<unsigned char>( edit->buffer[edit->cursor] ) ) ) {
			break;
		}
	}
}


static void Field_AdvanceCursor( field_t *edit ) {
	const int len = static_cast<int>( strlen( edit->buffer ) );

	while ( edit->cursor < len ) {
		++edit->cursor;
		if ( edit->cursor >= edit->scroll + edit->widthInChars ) {
			++edit->scroll;
		}
		if ( edit->cursor >= len ||
			!Field_IsUtf8ContinuationByte( static_cast<unsigned char>( edit->buffer[edit->cursor] ) ) ) {
			break;
		}
	}
}


static void Field_RetreatCursor( field_t *edit ) {
	while ( edit->cursor > 0 ) {
		--edit->cursor;
		if ( edit->cursor < edit->scroll ) {
			--edit->scroll;
		}
		if ( !Field_IsUtf8ContinuationByte( static_cast<unsigned char>( edit->buffer[edit->cursor] ) ) ) {
			break;
		}
	}
}


/*
=================
Field_NextWord
=================
*/
static void Field_SeekWord( field_t *edit, int direction )
{
	if ( direction > 0 ) {
		while ( edit->buffer[ edit->cursor ] == ' ' )
			edit->cursor++;
		while ( edit->buffer[ edit->cursor ] != '\0' && edit->buffer[ edit->cursor ] != ' ' )
			edit->cursor++;
		while ( edit->buffer[ edit->cursor ] == ' ' )
			edit->cursor++;
	} else {
		while ( edit->cursor > 0 && edit->buffer[ edit->cursor-1 ] == ' ' )
			edit->cursor--;
		while ( edit->cursor > 0 && edit->buffer[ edit->cursor-1 ] != ' ' )
			edit->cursor--;
		if ( edit->cursor == 0 && ( edit->buffer[ 0 ] == '/' || edit->buffer[ 0 ] == '\\' ) )
			edit->cursor++;
	}
}


/*
=================
Field_KeyDownEvent

Performs the basic line editing functions for the console,
in-game talk, and menu fields

Key events are used for non-printable characters, others are gotten from char events.
=================
*/
static void Field_KeyDownEvent( field_t *edit, int key ) {
	int		len;

	// shift-insert is paste
	if ( ( ( key == K_INS ) || ( key == K_KP_INS ) ) && keys[K_SHIFT].down ) {
		Field_Paste( edit );
		return;
	}

	len = strlen( edit->buffer );

	switch ( key ) {
		case K_DEL:
		case K_KP_DEL:
			Field_DeleteCurrentCharacter( edit );
			break;

		case K_RIGHTARROW:
		case K_KP_RIGHTARROW:
			if ( edit->cursor < len ) {
				if ( keys[ K_CTRL ].down ) {
					Field_SeekWord( edit, 1 );
				} else {
					Field_AdvanceCursor( edit );
				}
			}
			break;

		case K_LEFTARROW:
		case K_KP_LEFTARROW:
			if ( edit->cursor > 0 ) {
				if ( keys[ K_CTRL ].down ) {
					Field_SeekWord( edit, -1 );
				} else {
					Field_RetreatCursor( edit );
				}
			}
			break;

		case K_HOME:
		case K_KP_HOME:
			edit->cursor = 0;
			break;

		case K_END:
		case K_KP_END:
			edit->cursor = len;
			break;

		case K_INS:
		case K_KP_INS:
			key_overstrikeMode = key_overstrikeMode ? qfalse : qtrue;
			break;

		default:
			break;
	}

	// Change scroll if cursor is no longer visible
	if ( edit->cursor < edit->scroll ) {
		edit->scroll = edit->cursor;
	} else if ( edit->cursor >= edit->scroll + edit->widthInChars && edit->cursor <= len ) {
		edit->scroll = edit->cursor - edit->widthInChars + 1;
	}
}


/*
==================
Field_CharEvent
==================
*/
static void Field_CharEvent( field_t *edit, int ch ) {
	int		len;

	if ( ch == 'v' - 'a' + 1 ) {	// ctrl-v is paste
		Field_Paste( edit );
		return;
	}

	if ( ch == 'c' - 'a' + 1 ) {	// ctrl-c clears the field
		Field_Clear( edit );
		return;
	}

	len = strlen( edit->buffer );

	if ( ch == 'h' - 'a' + 1 )	{	// ctrl-h is backspace
		Field_DeletePreviousCharacter( edit );
		return;
	}

	if ( ch == 'a' - 'a' + 1 ) {	// ctrl-a is home
		edit->cursor = 0;
		edit->scroll = 0;
		return;
	}

	if ( ch == 'e' - 'a' + 1 ) {	// ctrl-e is end
		edit->cursor = len;
		edit->scroll = ( std::max )( 0, edit->cursor - edit->widthInChars );
		return;
	}

	//
	// ignore any other non printable chars
	//
	if ( ch < ' ' ) {
		return;
	}

	const unsigned char byte = static_cast<unsigned char>( ch );
	Field_InsertUtf8Scalar( edit, &byte, 1 );
}


/*
=============================================================================

CONSOLE LINE EDITING

==============================================================================
*/

/*
====================
Console_Key

Handles history and console scrollback
====================
*/
static void Console_Key( int key ) {
	std::array<char, MAX_STRING_CHARS> rawSayBuffer;

	// ctrl-L clears screen
	if ( key == 'l' && keys[K_CTRL].down ) {
		Cbuf_AddText( "clear\n" );
		return;
	}

	if ( Con_InputKey( key ) ) {
		return;
	}

	// enter finishes the line
	if ( key == K_ENTER || key == K_KP_ENTER ) {
		const bool consoleChatAllowed =
			( cl_allowConsoleChat && cl_allowConsoleChat->integer ) ||
			Con_UseAutoSay();

		// Retail treats bare text as a command unless console chat is
		// explicitly enabled. FnQL's con_autoSay retains the same opt-in.
		if ( !consoleChatAllowed
			&& g_consoleField.buffer[0] != '\0'
			&& g_consoleField.buffer[0] != '\\'
			&& g_consoleField.buffer[0] != '/' ) {
			std::array<char, MAX_EDIT_LINE - 1> temp;

			Q_strncpyz( temp.data(), g_consoleField.buffer, static_cast<int>( temp.size() ) );
			Com_sprintf( g_consoleField.buffer, sizeof( g_consoleField.buffer ), "\\%s", temp.data() );
			g_consoleField.cursor++;
		}

		Com_Printf( "]%s\n", g_consoleField.buffer );

		// leading slash is an explicit command
		if ( g_consoleField.buffer[0] == '\\' || g_consoleField.buffer[0] == '/' ) {
			Cbuf_AddText( g_consoleField.buffer+1 );	// valid command
			Cbuf_AddText( "\n" );
		} else {
			if ( !g_consoleField.buffer[0] ) {
				return;	// empty lines just scroll the console without adding to history
			} else if ( consoleChatAllowed ) {
				// Retail owns cl_allowConsoleChat. Keep FnQL's con_autoSay as a
				// supported alias so existing configs retain their behavior.
				if ( Con_UseRawSay() ) {
					Com_sprintf( rawSayBuffer.data(), static_cast<int>( rawSayBuffer.size() ), "say \"%s\"\n", g_consoleField.buffer );
					CL_AddReliableCommand( rawSayBuffer.data(), qfalse );
				} else {
					Cbuf_AddText( "cmd say " );
					Cbuf_AddText( g_consoleField.buffer );
					Cbuf_AddText( "\n" );
				}
			} else {
				Cbuf_AddText( g_consoleField.buffer );
				Cbuf_AddText( "\n" );
			}
		}

		// copy line to history buffer
		Con_SaveField( &g_consoleField );

		Field_Clear( &g_consoleField );
		g_consoleField.widthInChars = g_console_field_width;

		if ( cls.state == CA_DISCONNECTED ) {
			SCR_UpdateScreen ();	// force an update, because the command
		}							// may take some time
		return;
	}

	// command history (ctrl-p ctrl-n for unix style)

	if ( (key == K_MWHEELUP && keys[K_SHIFT].down) || ( key == K_UPARROW ) || ( key == K_KP_UPARROW ) ||
		 ( ( tolower(key) == 'p' ) && keys[K_CTRL].down ) ) {
		Con_HistoryGetPrev( &g_consoleField );
		g_consoleField.widthInChars = g_console_field_width;
		return;
	}

	if ( (key == K_MWHEELDOWN && keys[K_SHIFT].down) || ( key == K_DOWNARROW ) || ( key == K_KP_DOWNARROW ) ||
		 ( ( tolower(key) == 'n' ) && keys[K_CTRL].down ) ) {
		Con_HistoryGetNext( &g_consoleField );
		g_consoleField.widthInChars = g_console_field_width;
		return;
	}

	// console scrolling
	if ( key == K_PGUP || key == K_MWHEELUP ) {
		if ( keys[K_CTRL].down ) {	// hold <ctrl> to accelerate scrolling
			Con_PageUp( 0 );		// by one visible page
		} else {
			Con_PageUp( -1 );
		}
		return;
	}

	if ( key == K_PGDN || key == K_MWHEELDOWN ) {
		if ( keys[K_CTRL].down ) {	// hold <ctrl> to accelerate scrolling
			Con_PageDown( 0 );		// by one visible page
		} else {
			Con_PageDown( -1 );
		}
		return;
	}

	// ctrl-home = top of console
	if ( key == K_HOME && keys[K_CTRL].down ) {
		Con_Top();
		return;
	}

	// ctrl-end = bottom of console
	if ( key == K_END && keys[K_CTRL].down ) {
		Con_Bottom();
		return;
	}

	// pass to the normal editline routine
	Field_KeyDownEvent( &g_consoleField, key );
}

//============================================================================


/*
================
Message_Key

In game talk message
================
*/
static void Message_Key( int key ) {

	std::array<char, MAX_STRING_CHARS> buffer;

	if (key == K_ESCAPE) {
		Key_SetCatcher( Key_GetCatcher( ) & ~KEYCATCH_MESSAGE );
		if ( Com_GameIsQuakeLive() && cgvm && cgvm->dllExports ) {
			VM_Call( cgvm, 0, CG_CHAT_UP );
		}
		Field_Clear( &chatField );
		return;
	}

	if ( key == K_ENTER || key == K_KP_ENTER )
	{
		if ( chatField.buffer[0] && cls.state == CA_ACTIVE ) {
			if (chat_playerNum != -1 )

				Com_sprintf( buffer.data(), static_cast<int>( buffer.size() ), "tell %i \"%s\"\n", chat_playerNum, chatField.buffer );

			else if (chat_team)

				Com_sprintf( buffer.data(), static_cast<int>( buffer.size() ), "say_team \"%s\"\n", chatField.buffer );
			else if ( Com_GameIsRTCW() && cl.rtcwLimboMode )
				Com_sprintf( buffer.data(), static_cast<int>( buffer.size() ), "say_limbo \"%s\"\n", chatField.buffer );
			else
				Com_sprintf( buffer.data(), static_cast<int>( buffer.size() ), "say \"%s\"\n", chatField.buffer );

			CL_AddReliableCommand( buffer.data(), qfalse );
		}
		Key_SetCatcher( Key_GetCatcher( ) & ~KEYCATCH_MESSAGE );
		if ( Com_GameIsQuakeLive() && cgvm && cgvm->dllExports ) {
			VM_Call( cgvm, 0, CG_CHAT_UP );
		}
		Field_Clear( &chatField );
		return;
	}

	Field_KeyDownEvent( &chatField, key );
}

//============================================================================

/*
=============
CL_DispatchBrowserKeyEvent

Routes captured key events through the retained browser bridge only when the
browser keycatcher owns input.
=============
*/
static void CL_DispatchBrowserKeyEvent( int key, qboolean down ) {
	if ( key >= K_MOUSE1 && key <= K_MOUSE9 ) {
		CL_WebView_OnMouseButtonEvent( key, down );
	} else if ( key == K_MWHEELUP ) {
		if ( down ) {
			CL_WebView_OnMouseWheelEvent( 1 );
		}
	} else if ( key == K_MWHEELDOWN ) {
		if ( down ) {
			CL_WebView_OnMouseWheelEvent( -1 );
		}
	} else {
		CL_WebView_OnKeyEvent( key, down );
	}
}


/*
=============
CL_ActivateNativeMenu

Activates a menu owned by the retail UI module and transfers input to it.

Ownership is claimed unconditionally, as retail does.  Do not gate this on
UI_MENUS_ANY_VISIBLE: that recovered slot is only established for the
fullscreen main menu, and withholding KEYCATCH_UI here takes the in-game menu's
draw pass (SCR_DrawScreenField) and its absolute-pointer cursor and mouse grab
(IN_AbsolutePointerOwnerKind) down with it.
=============
*/
static void CL_ActivateNativeMenu( uiMenuCommand_t menu ) {
	if ( !uivm ) {
		return;
	}

	// Browser capture has priority over native UI input. Retail releases it
	// before opening an in-game menu so the UI receives absolute mouse motion
	// and can draw its own cursor.
	CL_WebHost_HideForGameTransition();

	VM_Call( uivm, 1, UI_SET_ACTIVE_MENU, menu );
	Key_SetCatcher( ( Key_GetCatcher() & ~( KEYCATCH_CGAME | KEYCATCH_BROWSER ) )
		| KEYCATCH_UI );
}


/*
=============
CL_ModuleKeyEvent

The number a UI or cgame is handed for a key event, or -1 for a key its game
has no number for. Quake III and Elite Force number keys as original Quake III
does, four below this engine from the mouse wheel upward, and q3_ui's and Elite
Force's menus take joystick buttons 1-4 as Enter: this engine's mouse wheel
numbers are those buttons there. Quake Live's modules take this engine's own
numbers; RTCW's and ET's are converted with the rest of their calls in vm.c.
=============
*/
static int CL_ModuleKeyEvent( int key ) {
	if ( Com_GameIsQuake3() ) {
		return FnTech3_Q3KeyEventFromInternal( key );
	}
	if ( Com_GameIsSTVEF() ) {
		return FnTech3_STVEFKeyEventFromInternal( key );
	}
	return key;
}


/*
=============
CL_UIKeyDownEvent

Hands the UI a key press, numbered for its game; a key that game has no
number for is not delivered.
=============
*/
static void CL_UIKeyDownEvent( int key, unsigned time ) {
	key = CL_ModuleKeyEvent( key );
	if ( key >= 0 ) {
		VM_Call( uivm, 3, UI_KEY_EVENT, key, qtrue, time );
	}
}


/*
=============
CL_UIKeyUpEvent

Hands the UI a key release, numbered as its press was.
=============
*/
static void CL_UIKeyUpEvent( int key, unsigned time ) {
	key = CL_ModuleKeyEvent( key );
	if ( key >= 0 ) {
		VM_Call( uivm, 3, UI_KEY_EVENT, key, qfalse, time );
	}
}


/*
=============
CL_CGameKeyEvent

Hands the cgame a key press or release, numbered as the UI's are.
=============
*/
static void CL_CGameKeyEvent( int key, qboolean down ) {
	key = CL_ModuleKeyEvent( key );
	if ( key >= 0 ) {
		VM_Call( cgvm, 2, CG_KEY_EVENT, key, down );
	}
}


/*
=============
CL_ToggleMenuInternal

Routes Quake Live-style menu toggles through the same path as the hard-coded
Escape key, optionally synthesizing a release event for command-triggered use.
=============
*/
static void CL_ToggleMenuInternal( int key, qboolean sendKeyUp, unsigned time ) {
#ifdef USE_CURL
	if ( Com_DL_InProgress( &download ) && download.mapAutoDownload ) {
		Com_DL_Cleanup( &download );
	}
#endif

	if ( Key_GetCatcher() & KEYCATCH_CONSOLE ) {
		// escape always closes console
		Con_ToggleConsole_f();
		Key_ClearStates();
		return;
	}

	if ( Key_GetCatcher() & KEYCATCH_BROWSER ) {
		// The disconnected WebUI owns Escape itself. Once a game is active,
		// retail transfers Escape to the native in-game/join menu. This also
		// recovers if a late WebUI callback re-arms browser input after the
		// connection transition has already hidden its surface.
		if ( CL_WebHost_HasDrawableSurface()
			&& ( cls.state != CA_ACTIVE || clc.demoplaying ) ) {
			return;
		}
		// Either play is active, or the input claim outlived the surface that
		// could have drawn a menu. A browser with nothing to present cannot
		// consume Escape, so release it rather than dropping the key.
		CL_WebHost_HideForGameTransition();
	}

	if ( Key_GetCatcher( ) & KEYCATCH_MESSAGE ) {
		// clear message mode
		Message_Key( K_ESCAPE );
		return;
	}

	// Retail event 5 closes the command overlay; legacy Q3 modules expose
	// only the ordinary CGAME_EVENT_NONE transition at this boundary.
	if ( Key_GetCatcher( ) & KEYCATCH_CGAME ) {
		if ( cgvm ) {
			VM_Call( cgvm, 1, CG_EVENT_HANDLING,
				cgvm->dllExports ? CGAME_EVENT_CLOSECOMMANDOVERLAY : CGAME_EVENT_NONE );
		}
		// A missing or older module must not leave stale cgame ownership behind.
		Key_SetCatcher( Key_GetCatcher( ) & ~KEYCATCH_CGAME );
	}

	Com_DPrintf( "CL_ToggleMenuInternal: catcher 0x%04x state %i\n",
		Key_GetCatcher(), (int)cls.state );

	if ( !( Key_GetCatcher( ) & KEYCATCH_UI ) ) {
		if ( !uivm ) {
			return;
		}

		if ( cls.state == CA_ACTIVE && !clc.demoplaying ) {
			CL_ActivateNativeMenu( UIMENU_INGAME );
		}
		else if ( cls.state != CA_DISCONNECTED ) {
#if 0
			CL_Disconnect_f();
			S_StopAllSounds();
#else
			Cmd_Clear();
			Cvar_Set( "com_errorMessage", "" );
			if ( cls.state == CA_CINEMATIC ) {
				SCR_StopCinematic();
				CL_ActivateNativeMenu( UIMENU_MAIN );
			} else if ( !CL_Disconnect( qtrue ) ) { // restart client if not done already
				CL_FlushMemory();
			}
#endif
		}
		return;
	}

	if ( uivm ) {
		CL_UIKeyDownEvent( key, time );
		// Elite Force's menus take every key event as a press.
		if ( sendKeyUp && !Com_GameIsSTVEF() ) {
			CL_UIKeyUpEvent( key, time );
		}
	}
}


/*
================
CL_ToggleMenu_f
================
*/
void CL_ToggleMenu_f( void ) {
	CL_ToggleMenuInternal( K_ESCAPE, qtrue, cls.realtime );
}


/*
=============
CL_HandleDemoPlaybackKeyEvent

Applies the retail Quake Live demo-playback shortcuts while normal input
catchers are clear apart from the recovered 0x10 mouse-pass bit.
=============
*/
static qboolean CL_HandleDemoPlaybackKeyEvent( int key ) {
	if ( !clc.demoplaying || ( Key_GetCatcher() & ~KEYCATCH_RETAIL_MOUSEPASS ) != 0 ) {
		return qfalse;
	}

	switch ( key ) {
	case K_SPACE:
		Cbuf_ExecuteText( EXEC_APPEND, "toggle cl_freezeDemo\n" );
		return qtrue;

	case K_DOWNARROW:
	case K_MOUSE3:
		Cbuf_ExecuteText( EXEC_APPEND, "timescale 1\n" );
		return qtrue;

	case K_LEFTARROW:
	case K_MWHEELDOWN:
		Cbuf_ExecuteText( EXEC_APPEND, "cvarAdd timescale -0.1\n" );
		return qtrue;

	case K_RIGHTARROW:
	case K_MWHEELUP:
		if ( cl_freezeDemo && cl_freezeDemo->integer ) {
			Cbuf_ExecuteText( EXEC_APPEND, "timescale 1; cl_freezeDemo 0; wait; wait; cl_freezeDemo 1\n" );
		} else {
			Cbuf_ExecuteText( EXEC_APPEND, "cvarAdd timescale 0.1\n" );
		}
		return qtrue;

	case K_DEL:
		Cbuf_ExecuteText( EXEC_APPEND, "toggle cg_drawDemoHUD\n" );
		return qtrue;
	}

	return qfalse;
}


/*
===================
CL_KeyDownEvent

Called by CL_KeyEvent to handle a keypress
===================
*/
static void CL_KeyDownEvent( int key, unsigned time )
{
	keys[key].down = qtrue;
	keys[key].bound = qfalse;
	keys[key].repeats++;

	if ( keys[key].repeats == 1 ) {
		anykeydown++;
	}

#ifndef _WIN32
	if ( keys[K_ALT].down && key == K_ENTER )
	{
		Cvar_SetValue( "r_fullscreen", !Cvar_VariableIntegerValue( "r_fullscreen" ) );
		Cbuf_ExecuteText( EXEC_APPEND, "vid_restart\n" );
		return;
	}
#endif

	// console key is hardcoded, so the user can never unbind it
	if ( key == K_CONSOLE || ( keys[K_SHIFT].down && key == K_ESCAPE ) ) {
		Con_ToggleConsole_f();
		Key_ClearStates();
		return;
	}

	const auto cameraAction = CL_RTCWCameraKeyAction(Com_GameIsRTCWSP(),
		cl.rtcwCameraMode, Key_GetCatcher(), key, true);
	if (cameraAction != rtcwCameraKeyAction::Pass) {
		if (cameraAction == rtcwCameraKeyAction::Interrupt) CL_AddReliableCommand("cameraInterrupt", qfalse);
		return;
	}

	// keys can still be used for bound actions
	if ( CL_STVEFSPCinematicKey( key ) ) return;
	if ( ( key < 128 || key == K_MOUSE1 ) && cls.state == CA_CINEMATIC && Key_GetCatcher() == 0 ) {
		if ( Cvar_VariableIntegerValue( "com_cameraMode" ) == 0 ) {
			Cvar_Set ("nextdemo","");
			key = K_ESCAPE;
		}
	}

	// escape is always handled special
	if ( key == K_ESCAPE ) {
		CL_ToggleMenuInternal( key, qfalse, time );
		return;
	}

	if ( CL_HandleDemoPlaybackKeyEvent( key ) ) {
		return;
	}

	// distribute the key down event to the appropriate handler
	if ( Key_GetCatcher( ) & KEYCATCH_CONSOLE ) {
		if ( Con_KeyEvent( key, qtrue ) ) {
			return;
		}
		Console_Key( key );
	} else if ( Key_GetCatcher( ) & KEYCATCH_BROWSER ) {
		CL_DispatchBrowserKeyEvent( key, qtrue );
	} else if ( Key_GetCatcher( ) & KEYCATCH_UI ) {
		if ( uivm ) {
			CL_UIKeyDownEvent( key, time );
		}
	} else if ( Key_GetCatcher( ) & KEYCATCH_MESSAGE ) {
		Message_Key( key );
	} else if ( Key_GetCatcher( ) & KEYCATCH_CGAME ) {
		if ( cgvm ) {
			CL_CGameKeyEvent( key, qtrue );
		}
	} else if ( cls.state == CA_DISCONNECTED ) {
		Console_Key( key );
	} else {
		// send the bound action
		Key_ParseBinding( key, qtrue, time );
	}
}


/*
===================
CL_KeyUpEvent

Called by CL_KeyEvent to handle a keyrelease
===================
*/
static void CL_KeyUpEvent( int key, unsigned time )
{
	const bool bound = keys[key].bound != qfalse;
	const bool wasDown = keys[key].down != qfalse;

	keys[key].repeats = 0;
	keys[key].down = qfalse;
	keys[key].bound = qfalse;

	if ( wasDown ) {
		if ( anykeydown > 0 ) {
			--anykeydown;
		} else {
			anykeydown = 0;
		}
	} else {
		// Duplicate or release-only platform messages have no state to unwind.
		// Ignoring them also prevents redundant UI/cgame/browser key-up calls.
		return;
	}

	// don't process key-up events for the console key
	if ( key == K_CONSOLE || ( key == K_ESCAPE && keys[K_SHIFT].down ) ) {
		return;
	}
	if (CL_RTCWCameraKeyAction(Com_GameIsRTCWSP(), cl.rtcwCameraMode,
		Key_GetCatcher(), key, false) != rtcwCameraKeyAction::Pass) return;

	//
	// key up events only perform actions if the game key binding is
	// a button command (leading + sign).  These will be processed even in
	// console mode and menu mode, to keep the character from continuing
	// an action started before a mode switch.
	//
	if ( cls.state != CA_DISCONNECTED ) {
		if ( bound || ( Key_GetCatcher() & KEYCATCH_CGAME ) ) {
			Key_ParseBinding( key, qfalse, time );
		}
	}

	if ( Key_GetCatcher() & KEYCATCH_CONSOLE ) {
		Con_KeyEvent( key, qfalse );
	}

	if ( ( Key_GetCatcher() & KEYCATCH_BROWSER ) && !( Key_GetCatcher() & KEYCATCH_CONSOLE ) ) {
		CL_DispatchBrowserKeyEvent( key, qfalse );
	} else if ( Key_GetCatcher() & KEYCATCH_UI ) {
		// Elite Force's menus take every key event as a press, so they are sent
		// none for a release (stvoyHM.exe 0x40bee6, stvoy.exe 0x407198 send
		// key-down events only).
		if ( uivm && !Com_GameIsSTVEF() ) {
			CL_UIKeyUpEvent( key, time );
		}
	} else if ( Key_GetCatcher() & KEYCATCH_CGAME ) {
		if ( cgvm ) {
			CL_CGameKeyEvent( key, qfalse );
		}
	}
}


/*
===================
CL_KeyEvent

Called by the system for both key up and key down events
===================
*/
void CL_KeyEvent( int key, qboolean down, unsigned time )
{
	// Zero is the platform translators' "unmapped key" sentinel.
	if ( key == 0 ) {
		return;
	}
	if ( key < 0 || key >= MAX_KEYS ) {
		Com_DPrintf( "CL_KeyEvent: ignoring invalid key %d\n", key );
		return;
	}

	if ( down )
		CL_KeyDownEvent( key, time );
	else
		CL_KeyUpEvent( key, time );
}


/*
===================
CL_CharEvent

Normal keyboard characters, already shifted / capslocked / etc
===================
*/
void CL_CharEvent( int key )
{
	if ( key < 0 ) {
		return;
	}

	const std::optional<std::uint32_t> codepoint =
		textInputDecoder.Consume( static_cast<std::uint32_t>( key ) );
	if ( !codepoint || *codepoint == 0u || *codepoint == 127u ) {
		// Delete is handled by Field_KeyDownEvent. A pending UTF-16 high
		// surrogate also deliberately produces no character yet.
		return;
	}

	// The browser takes composed characters, not the UTF-8 byte stream the
	// legacy consumers below want, so it is served before the encoding split.
	if ( !( Key_GetCatcher( ) & KEYCATCH_CONSOLE )
		&& ( Key_GetCatcher( ) & KEYCATCH_BROWSER ) ) {
		CL_WebView_OnCharEvent( static_cast<int>( *codepoint ) );
		return;
	}

	const fnql::input::Utf8Codepoint encoded = fnql::input::EncodeUtf8( *codepoint );
	const int catcher = Key_GetCatcher();

	// Keep the retail module byte-stream contract, but give engine-owned fields
	// the complete scalar so capacity checks can be atomic.
	if ( catcher & KEYCATCH_CONSOLE ) {
		if ( encoded.size == 1 ) {
			Con_CharEvent( encoded.bytes[0] );
		} else {
			Con_CharEventUtf8( encoded.bytes.data(),
				static_cast<int>( encoded.size ) );
		}
	} else if ( catcher & KEYCATCH_UI ) {
		if ( uivm ) {
			for ( std::size_t i = 0; i < encoded.size; ++i ) {
				const int utf8Byte = encoded.bytes[i];
				VM_Call( uivm, 3, UI_KEY_EVENT, utf8Byte | K_CHAR_FLAG,
					qtrue, cls.realtime );
			}
		}
	} else if ( catcher & KEYCATCH_MESSAGE ) {
		if ( encoded.size == 1 ) {
			Field_CharEvent( &chatField, encoded.bytes[0] );
		} else {
			Field_InsertUtf8Scalar( &chatField, encoded.bytes.data(), encoded.size );
		}
	} else if ( cls.state == CA_DISCONNECTED ) {
		if ( encoded.size == 1 ) {
			Field_CharEvent( &g_consoleField, encoded.bytes[0] );
		} else {
			Field_InsertUtf8Scalar( &g_consoleField, encoded.bytes.data(), encoded.size );
		}
	}
}


/*
===================
Key_ClearStates
===================
*/
void Key_ClearStates( void )
{
	int		i;

	textInputDecoder.Reset();
	anykeydown = 0;

	for ( i = 0 ; i < MAX_KEYS ; i++ )
	{
		if ( keys[i].down )
			CL_KeyEvent( i, qfalse, 0 );

		keys[i].down = qfalse;
		keys[i].bound = qfalse;
		keys[i].repeats = 0;
	}

	// Release commands above carry the old per-source generation. Invalidate
	// them only after every logical key has queued its release, then clear the
	// engine-owned state synchronously so wait or command-buffer pressure
	// cannot leave movement, mouselook, buttons, or generated voice held.
	CL_ClearKeyCommandInputState();
}


static int keyCatchers = 0;

/*
====================
Key_GetCatcher
====================
*/
int Key_GetCatcher( void )
{
	return keyCatchers;
}


/*
====================
Key_SetCatcher
====================
*/
void Key_SetCatcher( int catcher )
{
	// Preserve the anti-stuck release for real input-ownership changes, but
	// not when retail's gameplay-transparent HUD catcher is toggled.  Clearing
	// there releases movement keys that are already held when +scores begins.
	if ( fnql::input::GameplayCatcherStateChanged(
		keyCatchers, catcher, KEYCATCH_RETAIL_MOUSEPASS ) ) {
		Key_ClearStates();
	}

	keyCatchers = catcher;
}

// Quake III's modules number keys as original Quake III does
// (q3_key_adapter.h). A key this engine numbers outside that vocabulary is
// never reported, and a module number outside it never becomes an engine
// binding. Quake Live's modules take this engine's own numbers.
qboolean CL_Q3KeyIsDown( int key ) {
	const int internal = FnTech3_Q3KeyToInternal( key );
	return internal < 0 ? qfalse : Key_IsDown( internal );
}

void CL_Q3SetBinding( int key, const char *binding ) {
	const int internal = FnTech3_Q3KeyToInternal( key );
	if ( internal >= 0 && binding ) Key_SetBinding( internal, binding );
}

void CL_Q3KeyString( int key, char *buffer, int size, qboolean binding ) {
	if ( size <= 0 ) return;
	if ( !buffer ) {
		Com_Error( ERR_DROP, "NULL Quake III key string buffer" );
		return;
	}
	const int internal = FnTech3_Q3KeyToInternal( key );
	if ( binding ) {
		const char *value = Key_GetBinding( internal );
		Q_strncpyz( buffer, value ? value : "", size );
	} else if ( internal >= 0 || key == -1 ) {
		Q_strncpyz( buffer, Key_KeynumToString( internal ), size );
	} else if ( key < 0 || key > 255 ) {
		Q_strncpyz( buffer, "<OUT OF RANGE>", size );
	} else {
		// Quake III names a number it has no key for in hex.
		Com_sprintf( buffer, size, "0x%02x", key );
	}
}

int CL_Q3KeyForBinding( const char *binding ) {
	if ( !binding ) return -1;
	// Search in the module's own order, so an engine-only key holding the same
	// binding never displaces the key Quake III would have reported.
	for ( int key = 0; key <= FNTECH3_Q3_K_AUX16; ++key ) {
		const int internal = FnTech3_Q3KeyToInternal( key );
		const char *value = internal < 0 ? nullptr : Key_GetBinding( internal );
		if ( value && !Q_stricmp( binding, value ) ) return key;
	}
	return -1;
}

qboolean CL_RTCWKeyIsDown( int key ) {
	const int internal = FnTech3_RTCWKeyToInternal( key );
	return internal < 0 ? qfalse : Key_IsDown( internal );
}

void CL_RTCWSetBinding( int key, const char *binding ) {
	const int internal = FnTech3_RTCWKeyToInternal( key );
	if ( internal >= 0 && binding ) Key_SetBinding( internal, binding );
}

void CL_RTCWKeyString( int key, char *buffer, int size, qboolean binding ) {
	if ( size <= 0 ) return;
	if ( !buffer ) {
		Com_Error( ERR_DROP, "NULL RTCW key string buffer" );
		return;
	}
	if ( !binding ) {
		// Original RTCW hands its modules the translated name of the module's
		// own key number, in the player's cl_language, not the console's name.
		std::array<char, 5> tiny{};
		Q_strncpyz( buffer, fntech3::rtcw::keys::NameForModule( key,
			Cvar_VariableIntegerValue( "cl_language" ), Com_GameIsRTCWMP() != 0, tiny.data() ), size );
		return;
	}
	const char *value = Key_GetBinding( FnTech3_RTCWKeyToInternal( key ) );
	Q_strncpyz( buffer, value ? value : "", size );
}

int CL_RTCWKeyForBinding( const char *binding ) {
	if ( !binding ) return -1;
	// Search in the module's order, excluding engine-only keys even when they
	// hold the same binding as a later key that RTCW can represent.
	for ( int key = 0; key <= FNTECH3_RTCW_K_AUX16; ++key ) {
		const int internal = FnTech3_RTCWKeyToInternal( key );
		const char *value = internal < 0 ? nullptr : Key_GetBinding( internal );
		if ( value && !Q_stricmp( binding, value ) ) return key;
	}
	return -1;
}

int CL_RTCWGetKeyCatcher( void ) {
	const int internal = Key_GetCatcher();
	int external = 0;
#define RTCW_CATCHER(name) if ( internal & name ) external |= FNTECH3_RTCW_##name;
	RTCW_CATCHER(KEYCATCH_CONSOLE)
	RTCW_CATCHER(KEYCATCH_UI)
	RTCW_CATCHER(KEYCATCH_MESSAGE)
	RTCW_CATCHER(KEYCATCH_CGAME)
#undef RTCW_CATCHER
	return external;
}

void CL_RTCWSetKeyCatcher( int catcher ) {
	int internal = Key_GetCatcher() & KEYCATCH_BROWSER;
#define RTCW_CATCHER(name) if ( catcher & FNTECH3_RTCW_##name ) internal |= name;
	RTCW_CATCHER(KEYCATCH_CONSOLE)
	RTCW_CATCHER(KEYCATCH_UI)
	RTCW_CATCHER(KEYCATCH_MESSAGE)
	RTCW_CATCHER(KEYCATCH_CGAME)
#undef RTCW_CATCHER
	Key_SetCatcher( internal );
}

qboolean CL_STVEFKeyIsDown( int key ) {
	const int internal = FnTech3_STVEFKeyToInternal( key );
	return internal < 0 ? qfalse : Key_IsDown( internal );
}

void CL_STVEFSetBinding( int key, const char *binding ) {
	const int internal = FnTech3_STVEFKeyToInternal( key );
	if ( internal >= 0 && binding ) Key_SetBinding( internal, binding );
}

void CL_STVEFKeyString( int key, char *buffer, int size, qboolean binding ) {
	if ( size <= 0 ) return;
	if ( !buffer ) {
		Com_Error( ERR_DROP, "NULL Elite Force key string buffer" );
		return;
	}
	if ( !binding ) {
		// Retail hands its modules the translated name of the module's own key
		// number, in the player's g_language.
		std::array<char, 5> tiny{};
		Q_strncpyz( buffer, fntech3::stvef::keys::NameForModule( key,
			Cvar_VariableString( "g_language" ), Com_GameIsSTVEFMP() != 0, tiny.data() ), size );
		return;
	}
	const char *value = Key_GetBinding( FnTech3_STVEFKeyToInternal( key ) );
	Q_strncpyz( buffer, value ? value : "", size );
}

// Enemy Territory's key vocabulary is its own. It ends at K_AUX16, so the four
// keys this engine numbers above that have no ET number and are never reported;
// a module number above ET's range never becomes an engine binding either.
qboolean CL_WolfETKeyIsDown( int key ) {
	const int internal = FnTech3_WolfETKeyToInternal( key );
	return internal < 0 ? qfalse : Key_IsDown( internal );
}

void CL_WolfETSetBinding( int key, const char *binding ) {
	const int internal = FnTech3_WolfETKeyToInternal( key );
	if ( internal >= 0 && binding ) Key_SetBinding( internal, binding );
}

void CL_WolfETKeyString( int key, char *buffer, int size, qboolean binding ) {
	if ( size <= 0 ) return;
	if ( !buffer ) {
		Com_Error( ERR_DROP, "NULL ET key string buffer" );
		return;
	}
	const int internal = FnTech3_WolfETKeyToInternal( key );
	const char *value = binding ? Key_GetBinding( internal ) : Key_KeynumToString( internal );
	Q_strncpyz( buffer, value ? value : "", size );
}

int CL_WolfETKeyForBinding( const char *binding ) {
	if ( !binding ) return -1;
	// Search in the module's own order, so an engine-only key holding the same
	// binding never displaces the key ET would have reported.
	for ( int key = 0; key <= FNTECH3_WOLFET_K_AUX16; ++key ) {
		const int internal = FnTech3_WolfETKeyToInternal( key );
		const char *value = internal < 0 ? nullptr : Key_GetBinding( internal );
		if ( value && !Q_stricmp( binding, value ) ) return key;
	}
	return -1;
}

// ET asks for the two keys bound to a command and expects -1 in either slot
// that has none. Both are searched in ET's own key order.
void CL_WolfETKeysForBinding( const char *binding, int *first, int *second ) {
	if ( !first || !second ) Com_Error( ERR_DROP, "NULL ET binding key output" );
	*first = *second = -1;
	if ( !binding ) return;
	for ( int key = 0; key <= FNTECH3_WOLFET_K_AUX16; ++key ) {
		const int internal = FnTech3_WolfETKeyToInternal( key );
		const char *value = internal < 0 ? nullptr : Key_GetBinding( internal );
		if ( !value || Q_stricmp( binding, value ) ) continue;
		if ( *first < 0 ) *first = key;
		else if ( *second < 0 ) { *second = key; return; }
	}
}

int CL_WolfETGetKeyCatcher( void ) {
	const int internal = Key_GetCatcher();
	int external = 0;
#define WOLFET_CATCHER(name) if ( internal & name ) external |= name;
	WOLFET_CATCHER(KEYCATCH_CONSOLE)
	WOLFET_CATCHER(KEYCATCH_UI)
	WOLFET_CATCHER(KEYCATCH_MESSAGE)
	WOLFET_CATCHER(KEYCATCH_CGAME)
#undef WOLFET_CATCHER
	// This engine's own mouse-pass and browser catchers are outside ET's four
	// and are deliberately not reported.
	return external;
}

void CL_WolfETSetKeyCatcher( int catcher ) {
	int internal = Key_GetCatcher() & KEYCATCH_BROWSER;
#define WOLFET_CATCHER(name) if ( catcher & name ) internal |= name;
	WOLFET_CATCHER(KEYCATCH_CONSOLE)
	WOLFET_CATCHER(KEYCATCH_UI)
	WOLFET_CATCHER(KEYCATCH_MESSAGE)
	WOLFET_CATCHER(KEYCATCH_CGAME)
#undef WOLFET_CATCHER
	Key_SetCatcher( internal );
}
