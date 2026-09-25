/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */

/* American McGee's Alice's frame: what its engine draws each frame and in
 * what order. Retail's field draw (SCR_DrawScreenField 0x0042e570) begins the
 * frame and hands everything to its UI's update (UI_Update 0x0040c800), whose
 * desktop draws the in-game view, the menus, the console and the HUD in its
 * own z-order. This is that update, and the in-game view's own pass:
 *
 *   View3D::Draw         0x00420900  the 3D field, then the overlay
 *   SCR_DrawScreenField3D 0x0042e490 the client game's view or the cinematic
 *   View3D::DrawOverlay  0x0041fde0  fade, letterbox, frame counter, the
 *                                    client game's 2D pass, then the centre
 *                                    or located print
 *
 * The centre and located prints live on the view (SetCenterPrint 0x0041f560,
 * SetLocationPrint 0x0041f760) and count down only while it draws, so they
 * freeze under menus, while loading and in clean screenshots, as retail's do.
 *
 * Divergences, each named where it happens: the frame counter's second and
 * third lines (renderer counters this engine's renderers do not keep), the
 * additive fade (no additive untextured box in this engine's renderer API),
 * the debug and time graphs, and the development console, which is this
 * engine's own rather than retail's floating "Alice Console" window. */

#include "client_alice_ui.hpp"

#include <algorithm>
#include <cmath>

namespace {

namespace screen = fntech3::alice::screen;
namespace font = fntech3::alice::ritualfont;

/* The view's print state: View3D +0x1d0 (timer), +0x1d4 (alpha), +0x1d8
 * (text), +0x1dc (material), +0x1e4 (located), +0x1e8..+0x1f4 (x, y, w, h). */
struct ViewPrint {
	std::string text;
	std::string material;     /* the name after a leading '@' */
	float timer = 0.0f;
	float alpha = 1.0f;
	bool located = false;
	int x = 0, y = 0, w = 0, h = 0;
};

ViewPrint viewPrint;
/* View3D's font, created with the view: courier-14 in Alice (UI_Init
 * 0x00410840), facfont-20 in F.A.K.K.2 (fakk2.exe 1.02 0x0040ecb2). */
int viewFont = -1;
cvar_t *alice_fps;
/* One frame drawn with only the view (saveshot 0x0042d570). */
int cleanShotFrames;
/* Whether the last frame showed the game's view, which a full-screen menu
 * coming up over it in F.A.K.K.2 fades in over (CL_RitualMenuCarry). */
bool viewShownLast;
/* The frame rate the counter shows, averaged as retail keeps it (0x00bd1d50). */
int fpsTimes[16];
int fpsIndex;

float VidWidth() { return static_cast<float>( cls.glconfig.vidWidth ); }
float VidHeight() { return static_cast<float>( cls.glconfig.vidHeight ); }

const char *ViewFontName() {
	return Com_GameIsFAKK2() ? "facfont-20" : "courier-14";
}

int ViewFont() {
	if ( viewFont < 0 ) {
		viewFont = CL_AliceFontRegister( ViewFontName() );
	}
	return viewFont;
}

const int *Stats() { return cl.snap.ps.alice.block[0]; }

/* The rate over the last sixteen frames. */
float FramesPerSecond() {
	int total = 0, counted = 0;
	for ( int time : fpsTimes ) {
		if ( time > 0 ) {
			total += time;
			++counted;
		}
	}
	return total > 0 ? 1000.0f * counted / total : 0.0f;
}

void SetPrint( const char *text, float alpha ) {
	viewPrint.text = text ? text : "";
	viewPrint.material.clear();
	if ( viewPrint.text[0] == '@' ) {
		viewPrint.material = viewPrint.text.substr( 1 );
	}
	viewPrint.alpha = alpha;
	viewPrint.timer = screen::PrintTimeMs;
}

/* The screen fade (0x00420620): only in the game and with a blend alpha. The
 * colour is left set, as retail leaves it; the next draw sets its own.
 * F.A.K.K.2's game module writes only the first two kinds, 0 and 1
 * (STAT_ADDFADE, fakk/gamex86.dll 0x100cc3a4). */
void DrawFade() {
	if ( cls.state != CA_ACTIVE ) {
		return;
	}
	const float *blend = cl.snap.ps.alice.blend;
	if ( !( blend[3] > 0.0f ) ) {
		return;
	}
	const float W = VidWidth(), H = VidHeight();
	switch ( screen::FadeKind( Stats()[CL_RitualFadeStat()] ) ) {
	case screen::Fade::Blend:
		CL_AliceFill( 0, 0, W, H, blend );
		break;
	case screen::Fade::Additive: {
		/* Retail adds the colour with an untextured additive box (renderer
		 * +0x6c, GL state ONE/ONE). This engine's 2D path blends by alpha
		 * only, so the colour is laid over at the alpha that keeps its
		 * brightest channel: over black this is exact, over a lit scene
		 * slightly darker than retail. */
		const float peak = std::max( blend[0], std::max( blend[1], blend[2] ) );
		if ( peak > 0.0f ) {
			const float a = std::min( peak, 1.0f );
			const float rgba[4] = { blend[0] / peak, blend[1] / peak, blend[2] / peak, a };
			CL_AliceFill( 0, 0, W, H, rgba );
		}
		break;
	}
	case screen::Fade::Drug:
		CL_AliceDrawPic( 0, 0, W, H, blend, CL_AliceShader( "textures/special/drugfade" ) );
		break;
	case screen::Fade::Ice:
		CL_AliceDrawPic( 0, 0, W, H, blend, CL_AliceShader( "textures/special/icefade" ) );
		break;
	case screen::Fade::None:
		break;
	}
}

/* The letterbox (0x00420570): two black bars of the stat's fraction of the
 * view's height. Its state also hides the HUD. F.A.K.K.2's game module keeps
 * the stat, scaled alike, in its own slot (CL_RitualLetterboxStat). */
bool DrawLetterbox() {
	if ( cls.state != CA_ACTIVE ) {
		return false;
	}
	const float fraction = screen::LetterboxFraction( Stats()[CL_RitualLetterboxStat()] );
	if ( fraction <= 0.0f ) {
		return false;
	}
	static const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	const float W = VidWidth(), H = VidHeight();
	CL_AliceFill( 0, 0, W, fraction * H, black );
	CL_AliceFill( 0, H - fraction * H, W, fraction * H, black );
	return true;
}

/* The frame counter (0x0041fb20): at the view's bottom left, indented by one
 * font height, white above 23.94 fps, yellow above 18, red below. Retail
 * prints two more lines of backend counters (world and total triangles,
 * vertices, clusters and texture megabytes) that this engine's renderers do
 * not report; they are not drawn. */
void DrawFrameCounter() {
	const int fontHeight = CL_AliceFontLineHeight( ViewFont() );
	const float fps = FramesPerSecond();
	static const float white[4] = { 1, 1, 1, 1 }, yellow[4] = { 1, 1, 0, 1 }, red[4] = { 1, 0, 0, 1 };
	const float *color = fps > screen::FpsGoodAbove ? white : fps > screen::FpsFairAbove ? yellow : red;
	CL_AliceFontPrint( ViewFont(), color, true, static_cast<float>( fontHeight ),
		VidHeight() - fontHeight * 3.0f, va( "FPS %4.1f", fps ), -1, 0, 0, 0, 1, 1 );
}

/* The centre print (0x0041fe70) and the located print (0x00420180). */
void DrawPrint() {
	if ( viewPrint.timer <= 0.0f ) {
		viewPrint.located = false;
		return;
	}
	const float a = screen::PrintAlpha( viewPrint.timer, viewPrint.alpha );
	const float W = VidWidth(), H = VidHeight();
	const float shade[4] = { a, a, a, a };
	const float shadow[4] = { 0, 0, 0, a };
	const float white[4] = { 1, 1, 1, a };
	const int fontIndex = ViewFont();

	if ( viewPrint.located ) {
		if ( !viewPrint.material.empty() ) {
			/* 640x480 units with no flip; a zero handle draws nothing
			 * (0x004202b1). Retail's svc_locprint passes -1 for the size, so
			 * a server-sent picture draws inside out; kept. */
			const qhandle_t shader = CL_AliceShader( viewPrint.material.c_str() );
			if ( shader ) {
				const float sx = W / 640.0f, sy = H / 480.0f;
				CL_AliceDrawPic( viewPrint.x * sx, viewPrint.y * sy, viewPrint.w * sx, viewPrint.h * sy, shade,
					shader );
			}
		} else {
			const screen::LocatedText origin = screen::LocatedTextOrigin( viewPrint.x, viewPrint.y,
				CL_AliceFontLineHeight( fontIndex ), W, H );
			font::Rect rect{ origin.x + 1.0f, origin.y + 1.0f, W, H };
			CL_AliceFontPrintJustified( fontIndex, shadow, true, rect, origin.horizontal, origin.vertical,
				viewPrint.text.c_str(), 0, 0, 0, 1, 1 );
			rect.x -= 1.0f;
			rect.y -= 1.0f;
			CL_AliceFontPrintJustified( fontIndex, white, true, rect, origin.horizontal, origin.vertical,
				viewPrint.text.c_str(), 0, 0, 0, 1, 1 );
		}
	} else if ( !viewPrint.material.empty() ) {
		/* The picture's own pixel size, centred on the view (0x0041fedd). */
		const qhandle_t shader = CL_AliceShader( viewPrint.material.c_str() );
		if ( shader ) {
			int w = 0, h = 0;
			CL_AliceShaderSize( shader, &w, &h );
			CL_AliceDrawPic( ( W - w ) * 0.5f, ( H - h ) * 0.5f, static_cast<float>( w ), static_cast<float>( h ),
				shade, shader );
		}
	} else {
		font::Rect rect{ 1.0f, 1.0f, W, H };
		CL_AliceFontPrintJustified( fontIndex, shadow, true, rect, 0, 1, viewPrint.text.c_str(), 0, 0, 0, 1, 1 );
		rect = font::Rect{ 0.0f, 0.0f, W, H };
		CL_AliceFontPrintJustified( fontIndex, white, true, rect, 0, 1, viewPrint.text.c_str(), 0, 0, 0, 1, 1 );
	}

	viewPrint.timer = screen::AdvancePrintTimer( viewPrint.timer, cls.frametime );
	if ( viewPrint.timer <= 0.0f ) {
		viewPrint.located = false;
	}
}

/* SCR_DrawScreenField3D (0x0042e490). Retail blacks the screen first only
 * outside the game on a screen wider than 4:3; here it is blacked for every
 * state but the game, because this engine's cinematic may be drawn at its own
 * aspect and its frame does not begin cleared. At 4:3 the two are the same. */
void DrawField3D( stereoFrame_t stereo ) {
	if ( cls.state != CA_ACTIVE ) {
		CL_AliceClearBlack();
	}
	switch ( cls.state ) {
	case CA_ACTIVE:
		if ( cgvm ) {
			CL_CGameRendering( stereo );
		}
		break;
	case CA_CINEMATIC:
		SCR_DrawCinematic();
		break;
	default:
		break;
	}
}

/* View3D::Draw and its overlay. Returns whether the letterbox is up. */
bool DrawView( stereoFrame_t stereo, bool clean ) {
	if ( cls.state != CA_DISCONNECTED ) {
		DrawField3D( stereo );
	}
	if ( !clean ) {
		DrawFade();
	}
	const bool letterbox = DrawLetterbox();
	if ( !clean && alice_fps && alice_fps->integer ) {
		DrawFrameCounter();
	}
	if ( !clean && cgvm ) {
		CL_AliceCGameDraw2D();
	}
	if ( !clean ) {
		DrawPrint();
	}
	return letterbox;
}

/* saveshot (0x0042d570): one frame with only the view, then a screenshot of
 * it. The picture is taken at the end of the next frame drawn, so the clean
 * frame is the next one; cl_alice_saveshot.cpp draws it and keeps it. */
void CL_AliceSaveShot_f() {
	cleanShotFrames = 1;
	CL_AliceSaveshotCommand();
}

} // namespace

void CL_AliceSetCenterPrint( const char *text, float alpha ) {
	SetPrint( text, alpha );
	viewPrint.located = false;
}

void CL_AliceSetLocationPrint( int x, int y, const char *text, float alpha, int w, int h ) {
	SetPrint( text, alpha );
	viewPrint.x = x;
	viewPrint.y = y;
	viewPrint.w = w;
	viewPrint.h = h;
	viewPrint.located = true;
}

void CL_AliceClearPrints( void ) {
	viewPrint = ViewPrint();
}

void CL_AliceScreenInit( void ) {
	// `fps` is Com_Init's in retail (0x00439b05, archived, default 0).
	alice_fps = Cvar_Get( "fps", "0", CVAR_ARCHIVE );
	viewFont = CL_AliceFontRegister( ViewFontName() );
	Cmd_AddCommand( "saveshot", CL_AliceSaveShot_f );
}

void CL_AliceScreenShutdown( void ) {
	Cmd_RemoveCommand( "saveshot" );
}

/*
====================
CL_AliceUpdateScreenField

The frame after BeginFrame: retail's UI_Update (0x0040c800).
====================
*/
void CL_AliceUpdateScreenField( stereoFrame_t stereo ) {
	fpsTimes[fpsIndex++ & 15] = cls.realFrametime;
	CL_AliceUIFrame();

	// A clean shot draws the view alone.
	if ( cleanShotFrames > 0 && cls.state == CA_ACTIVE ) {
		--cleanShotFrames;
		DrawView( stereo, true );
		return;
	}
	cleanShotFrames = 0;

	// The loading screen owns the frame; nothing else is drawn.
	if ( CL_AliceLoadingActive() ) {
		CL_RitualEndMenuCarry();
		viewShownLast = false;
		CL_AliceClearBlack();
		CL_AliceDrawLoadingScreen();
		return;
	}

	bool showView = false;
	if ( CL_AliceUIFullscreenMenuUp() ) {
		// The menu hides the view. This engine's frame begins uncleared, and
		// retail's menus fill their own background, so black goes under them.
		// With ui_menuDissolve, F.A.K.K.2's keep what the screen showed as
		// they fade: the game's view, drawn here, or a menu (CL_RitualMenuCarry).
		CL_AliceClearBlack();
		if ( CL_RitualMenuCarry( viewShownLast ) ) {
			DrawView( stereo, false );
		}
	} else {
		CL_RitualEndMenuCarry();
		if ( cls.state < CA_ACTIVE ) {
			CL_AliceClearBlack();
			CL_AliceUIDrawIntroPlate();
		} else if ( cls.state == CA_ACTIVE || cls.state == CA_CINEMATIC ) {
			showView = true;
		} else {
			CL_AliceClearBlack();
		}
	}
	viewShownLast = showView;

	bool letterbox = false;
	if ( showView ) {
		letterbox = DrawView( stereo, false );
	}
	CL_AliceHudFrame( showView && cls.state == CA_ACTIVE && !letterbox && !CL_AliceUIMenuIsUp() );

	// The desktop's z-order (0x004bf4e0): the view, the menus, then the
	// console, the subtitles and the HUD, then the cursor.
	CL_AliceUIDrawMenus();
	if ( Key_GetCatcher() & KEYCATCH_CONSOLE ) {
		Con_DrawConsole();
	}
	CL_AliceDialogDraw();
	CL_AliceHudDraw();
	CL_AliceUIDrawCursor();
}
