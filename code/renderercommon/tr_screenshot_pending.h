/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is licensed under the GNU General Public
License, version 2 or (at your option) any later version.

Screenshot requests that cannot be taken. A `screenshot` command only
schedules its capture: the file is written at the end of the next rendered
frame, and that swap reports it with "Wrote <name>". Each renderer keeps one
waiting capture per file type in its backEnd (screenshotMask and the name
beside it). Two paths used to lose a request without a word:

  - a second request for a type whose capture is still waiting, for example
    two `screenshotPNG` in one frame, or one issued a few milliseconds after
    another under Alice, whose `wait` counts milliseconds; and
  - a capture still waiting when the renderer shuts down, which a map load
    (CL_ShutdownAll), a video restart and `quit` all do, or when the window is
    minimized with no framebuffer to capture from.

These helpers name every such request and the reason it was not written, so a
request either writes its file or says why it did not. A request naming the
file that is already waiting is covered by that capture and says nothing.

Each renderer includes this after its tr_local.h: it uses ri.Printf and the
SCREENSHOT_* bits, which are the same in every renderer.
===========================================================================
*/

#ifndef TR_SCREENSHOT_PENDING_H
#define TR_SCREENSHOT_PENDING_H

/* Why a waiting capture is dropped. */
#define R_SCREENSHOT_LOST_TO_SHUTDOWN "the renderer shut down before a frame was captured"
#define R_SCREENSHOT_LOST_TO_MINIMIZE "the window is minimized and cannot be captured"

static ID_INLINE void R_ReportUnwrittenScreenshot( const char *what, const char *reason )
{
	ri.Printf( PRINT_WARNING, "WARNING: %s was not written: %s.\n", what, reason );
}

/*
Names every capture still waiting, of those the arguments describe, with the
reason it will not be written. `mask` is backEnd.screenshotMask; the names
are the backEnd's per-type file names; `levelshot` and `cubemap` say whether
those captures are waiting. The AVI frame bit is not a request and is not
reported. Returns the number of captures named.
*/
static ID_INLINE int R_ReportUnwrittenScreenshots( int mask, const char *png,
	const char *tga, const char *jpg, const char *bmp, qboolean levelshot,
	qboolean cubemap, const char *reason )
{
	int reported = 0;

	if ( ( mask & SCREENSHOT_PNG ) && png && png[0] ) {
		R_ReportUnwrittenScreenshot( png, reason );
		reported++;
	}
	if ( ( mask & SCREENSHOT_TGA ) && tga && tga[0] ) {
		R_ReportUnwrittenScreenshot( tga, reason );
		reported++;
	}
	if ( ( mask & SCREENSHOT_JPG ) && jpg && jpg[0] ) {
		R_ReportUnwrittenScreenshot( jpg, reason );
		reported++;
	}
	if ( mask & SCREENSHOT_BMP ) {
		if ( bmp && bmp[0] ) {
			R_ReportUnwrittenScreenshot( bmp, reason );
			reported++;
		} else if ( mask & SCREENSHOT_BMP_CLIPBOARD ) {
			R_ReportUnwrittenScreenshot( "the clipboard screenshot", reason );
			reported++;
		}
	}
	if ( levelshot ) {
		R_ReportUnwrittenScreenshot( "the levelshot", reason );
		reported++;
	}
	if ( cubemap ) {
		R_ReportUnwrittenScreenshot( "the screenshot cubemap", reason );
		reported++;
	}
	return reported;
}

/*
A request for a file type whose capture is still waiting for the next frame.
`command` and `argument` are the request as typed (argument may be empty),
`ext` its file type's extension, and `pending` the name already waiting (empty
for a clipboard capture). A request that named the waiting file is covered by
that capture and prints nothing; any other request is refused aloud. Returns
qtrue when the request was covered.
*/
static ID_INLINE qboolean R_ReportScreenshotAlreadyPending( const char *command,
	const char *argument, const char *ext, const char *pending )
{
	char requested[MAX_OSPATH];

	requested[0] = '\0';
	if ( argument && argument[0] && Q_stricmp( argument, "silent" )
		&& Q_stricmp( argument, "clipboard" ) ) {
		Com_sprintf( requested, sizeof( requested ), "screenshots/%s.%s",
			argument, ext );
	}
	if ( requested[0] && pending && !Q_stricmp( requested, pending ) ) {
		return qtrue;
	}
	ri.Printf( PRINT_WARNING,
		"WARNING: %s%s%s not taken: %s is already waiting for the next frame.\n",
		command, ( argument && argument[0] ) ? " " : "",
		argument ? argument : "",
		( pending && pending[0] ) ? pending : "a clipboard screenshot" );
	return qfalse;
}

#endif /* TR_SCREENSHOT_PENDING_H */
