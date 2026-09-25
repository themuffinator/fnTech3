/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_FAKK2_HUD_HPP
#define FNTECH3_COMPAT_FAKK2_HUD_HPP

/* Heavy Metal: F.A.K.K.2's HUD: four .urc menus -- healthhud, weaphud,
 * dualweaphud and bosshud -- built from UIFakkLabel, the class every `Label`
 * in a Ritual .urc becomes (the loader's remap 0x0040e240 over the table at
 * 0x0050ba68). Its nine keywords (responses 0x0050d820) make a label show a
 * player stat as a number or a bar, the name of an item in hand, or that
 * item's model. Alice keeps the class and uses none of them; F.A.K.K.2's HUD
 * is made of them.
 *
 * This is the pure part: the bar's fraction, its flash and its quad, the
 * model's camera, and which HUD menus show. code/client/cl_alice_ui.cpp reads
 * the snapshot, feeds it here and draws what comes back.
 *
 * Addresses are retail alice.exe's (image base 0x400000), F.A.K.K.2's
 * unpacked descendant, unless they are marked fakk2.exe 1.02: the official
 * patch's plain executable, readable where the retail one is encrypted. The
 * stat and item slots are F.A.K.K.2's, read in its retail game module
 * (fakk/gamex86.dll, image base 0x10000000) where it stores them. */

#include <cmath>
#include <cstdint>

namespace fntech3 {
namespace fakk2 {
namespace hud {

/* ---- the player state's slots -------------------------------------------- */

/* F.A.K.K.2's stats (playerstat_t). Those the engine itself reads are pinned
 * where the retail game module stores them through the client's player state
 * (stats at +0xb0): the cinematic flags at +0xe8 (0x100d1349..0x100d1376),
 * the additive fade at +0xec (0x100cc3a4), the letterbox at +0xf0
 * (0x100d1502, 0x100d158f) and the boss's health, at least 1 while he
 * lives, at +0xe4 (0x100d133c). */
enum Stat {
	StatHealth = 0,
	StatDeadYaw = 1,
	StatAmmoLeft = 2,
	StatAmmoRight = 3,
	StatClipAmmoLeft = 4,
	StatClipAmmoRight = 5,
	StatWaterLevel = 6,
	StatMaxAmmoLeft = 7,
	StatMaxAmmoRight = 8,
	StatMaxClipAmmoLeft = 9,
	StatMaxClipAmmoRight = 10,
	StatLastPain = 11,
	StatAccumulatedPain = 12,
	StatBossHealth = 13,
	StatCinematic = 14,
	StatAddFade = 15,
	StatLetterbox = 16,
};

/* The item slots of the player state's activeItems (+0x130): an item index,
 * an offset into the item configstrings, or -1 for an empty hand. alice.exe's
 * weapon-group builder reads 4, 2 and 3 as the two-handed, left and right
 * weapons (0x0040fed0: 0x00c30264, 0x00c3025c, 0x00c30260). */
enum ActiveItem {
	ItemAmmoLeft = 0,
	ItemAmmoRight = 1,
	ItemWeaponLeft = 2,
	ItemWeaponRight = 3,
	ItemWeaponDual = 4,
};

inline constexpr int StatCount = 32;

/* A stat, or 0 for a slot outside the 32. Retail indexes the array with
 * whatever the .urc gave (0x0041ef39) and so reads the player state's
 * neighbouring words for a bad one; this reads none. */
inline int ReadSlot( const int *slots, int index ) {
	return slots && index >= 0 && index < StatCount ? slots[index] : 0;
}

/* ---- the bar (DrawStatbar 0x0041eb90) ------------------------------------ */

enum class Orientation { None = 0, Horizontal = 1, Vertical = 2 };

/* The label's playerstat, maxplayerstat, itemstat and invmodelrender slots
 * (+0x1f0..+0x1fc, each -1 until set), the statbar's orientation and range
 * (+0x200..+0x208), and its two materials (+0x214, +0x218). Which of the
 * label's jobs it does goes by the first slot set: stat, then item, then
 * model (Draw 0x0041ef00..0x0041ef26). */
struct Label {
	int stat = -1;
	int maxStat = -1;
	int item = -1;
	int model = -1;
	Orientation orientation = Orientation::None;
	float min = -1.0f;   /* 0xbf800000 at 0x0041e117 */
	float max = -1.0f;   /* 0x0041e11d */
};

/* How far the bar is filled (0x0041ef40..0x0041efc5): the stat over the
 * max stat's value when one is named (maxplayerstat >= 0), else over max
 * less min -- the value itself is never offset by min. More than full is
 * full; less than empty, or no number at all (0/0), is empty. A range of
 * zero divides into infinity or nothing, which the same two rules settle. */
inline float StatFraction( const Label &label, const int *stats ) {
	const float value = static_cast<float>( ReadSlot( stats, label.stat ) );
	const float range = label.maxStat >= 0 ? static_cast<float>( ReadSlot( stats, label.maxStat ) )
		: label.max - label.min;
	float frac;
	if ( range == 0.0f ) {
		frac = value > 0.0f ? 1.0f : 0.0f;
	} else {
		frac = value / range;
	}
	if ( frac > 1.0f ) frac = 1.0f;          /* fcomp 1.0 at 0x0041ef79 */
	if ( !( frac >= 0.0f ) ) frac = 0.0f;    /* below zero or NaN, 0x0041efa4 */
	return frac;
}

/* The flash: any change of the fraction starts it (0x0041ebcb..0x0041ebec;
 * fakk2.exe 1.02 0x004182ab..0x004182cc), and its strength falls linearly
 * from 1 to 0 over 750 ms (0x004ff964; fakk2.exe 0x004d5608, both
 * 0x3aaec33e). Neither engine's constructor sets the two members (fakk2.exe
 * 0x004177f0), and its class allocator takes plain heap memory (0x00438eef),
 * so the bar's first appearance flashes; the sentinel keeps that. */
struct Flash {
	float lastFrac = -1.0f;   /* +0x20c */
	float start = 0.0f;       /* +0x210, the UI's time in ms */
};

inline constexpr float FlashRate = 0.00133333332f;

/* One draw of a non-empty bar: returns the flash's strength, 0 to 1. An
 * empty bar is not drawn and leaves the flash alone (0x0041eb93..0x0041ebc5). */
inline float StepFlash( Flash &flash, float frac, float nowMs ) {
	if ( frac != flash.lastFrac ) flash.start = nowMs;
	flash.lastFrac = frac;
	float a = 1.0f - ( nowMs - flash.start ) * FlashRate;
	if ( a < 0.0f ) a = 0.0f;
	if ( a > 1.0f ) a = 1.0f;
	return a;
}

/* A quad in the label's own units, (0, 0) its top left, with its texture
 * coordinates; `draw` false for nothing. */
struct Quad {
	bool draw = false;
	float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
	float s0 = 0.0f, t0 = 0.0f, s1 = 1.0f, t1 = 1.0f;
};

/* The bar's quad for a fraction above zero. Retail draws in a window of the
 * label's own rectangle, clipped to it, with the renderer's tile draw
 * (texture coordinates are position over the image's size, 0x00498de0..
 * 0x00498e7a) or its stretch draw (0..1); this returns what that leaves:
 *
 *   tiled vertical      DrawTilePic( 0, (1-f)h, w, h )    filled from the bottom,
 *                       the texture fixed (0x0041ec85..0x0041ecac)
 *   tiled horizontal    DrawTilePic( 0, 0, f w, h )       from the left
 *   stretched vertical  DrawStretchPic( 0, f h, w, h )    retail's own inverted
 *                       bar: a full stat shows nothing (0x0041eda1..0x0041edc8)
 *   stretched horizontal DrawStretchPic( 0, 0, f w, h )   the image squeezed
 *
 * Any other orientation draws nothing. The flash is drawn with the same quad. */
inline Quad BarQuad( Orientation orientation, bool tiled, float frac, float w, float h, float imageW, float imageH ) {
	Quad q;
	if ( !( frac > 0.0f ) || w <= 0.0f || h <= 0.0f ) return q;
	const float iw = imageW > 0.0f ? imageW : w;
	const float ih = imageH > 0.0f ? imageH : h;
	switch ( orientation ) {
	case Orientation::Vertical:
		if ( tiled ) {
			q.x = 0.0f; q.y = ( 1.0f - frac ) * h; q.w = w; q.h = frac * h;
			q.s0 = 0.0f; q.s1 = w / iw;
			q.t0 = ( 1.0f - frac ) * h / ih; q.t1 = h / ih;
		} else {
			q.x = 0.0f; q.y = frac * h; q.w = w; q.h = h - frac * h;
			q.s0 = 0.0f; q.s1 = 1.0f;
			q.t0 = 0.0f; q.t1 = 1.0f - frac;
		}
		break;
	case Orientation::Horizontal:
		q.x = 0.0f; q.y = 0.0f; q.w = frac * w; q.h = h;
		if ( tiled ) {
			q.s0 = 0.0f; q.s1 = frac * w / iw;
			q.t0 = 0.0f; q.t1 = h / ih;
		} else {
			q.s0 = 0.0f; q.s1 = 1.0f; q.t0 = 0.0f; q.t1 = 1.0f;
		}
		break;
	default:
		return q;
	}
	q.draw = q.w > 0.0f && q.h > 0.0f;
	return q;
}

/* Without a bar material the label fills a box in its foreground colour from
 * its left edge, whatever its orientation, and draws no flash
 * (0x0041eed1 -> DrawBox 0x004bd060). */
inline Quad FallbackBox( float frac, float w, float h ) {
	Quad q;
	if ( !( frac > 0.0f ) ) return q;
	q.w = frac * w;
	q.h = h;
	q.draw = q.w > 0.0f && q.h > 0.0f;
	return q;
}

/* ---- the model (0x0041f0c7..0x0041f29f) ---------------------------------- */

struct Vec3 {
	float x = 0.0f, y = 0.0f, z = 0.0f;
};

/* An item's view block: the inventory script's hud* keywords for the HUD
 * (item +0x48), its plain ones for the inventory's list (item +0x94); the
 * block's constructor (0x004035d0) gives a scale of 1 and deltas of 45 0 0. */
enum class Move { None = 0, Bob = 1, Spin = 2 };

struct View {
	float scale = 1.0f;
	Vec3 angles;
	Vec3 angleDeltas{ 45.0f, 0.0f, 0.0f };
	Vec3 rotateOffset;
	Vec3 offset;
	Move move = Move::None;
};

/* The camera-space origin (0x0041f1da..0x0041f278): the largest extent,
 * taking the later of y, z and x on a tie, times the scale times 125/67
 * (0x004ff45c, 1 over 2 tan 15) away -- so a larger scale makes the model
 * smaller -- centred on its bounds, with y added rather than subtracted as
 * retail does. */
inline Vec3 ModelOrigin( const float *mins, const float *maxs, float scale ) {
	float extent = maxs[1] - mins[1];
	if ( !( extent > maxs[2] - mins[2] ) ) extent = maxs[2] - mins[2];
	if ( !( extent > maxs[0] - mins[0] ) ) extent = maxs[0] - mins[0];
	Vec3 origin;
	origin.x = extent * scale * 1.86567163f;
	origin.y = 0.5f * ( mins[1] + maxs[1] );
	origin.z = -0.5f * ( mins[2] + maxs[2] );
	return origin;
}

/* The model's angles at a time: its angles, plus sin(t) times its deltas for
 * bob or t times them for spin, t being the UI's time in ms times 1/333
 * (0x004ff468, 0x3b44ce08). Retail keeps t in a float, which loses the
 * millisecond after four and a half hours; this keeps it in a double and
 * takes a spin modulo 360, which turns the model the same way. */
inline constexpr double MoveRate = 0.00300300308;

inline Vec3 MovedAngles( const View &view, double timeMs ) {
	Vec3 angles = view.angles;
	const double t = timeMs * MoveRate;
	if ( view.move == Move::Bob ) {
		const double s = std::sin( t );
		angles.x += static_cast<float>( s * view.angleDeltas.x );
		angles.y += static_cast<float>( s * view.angleDeltas.y );
		angles.z += static_cast<float>( s * view.angleDeltas.z );
	} else if ( view.move == Move::Spin ) {
		angles.x += static_cast<float>( std::fmod( t * view.angleDeltas.x, 360.0 ) );
		angles.y += static_cast<float>( std::fmod( t * view.angleDeltas.y, 360.0 ) );
		angles.z += static_cast<float>( std::fmod( t * view.angleDeltas.z, 360.0 ) );
	}
	return angles;
}

/* The entity's origin (UI_Render3DModel 0x00408254..0x0040832c): the camera
 * origin plus the offset, plus the rotate offset along the model's own axes. */
inline Vec3 EntityOrigin( const Vec3 &origin, const View &view, const float axis[3][3] ) {
	Vec3 out = origin;
	out.x += view.offset.x;
	out.y += view.offset.y;
	out.z += view.offset.z;
	const float r[3] = { view.rotateOffset.x, view.rotateOffset.y, view.rotateOffset.z };
	for ( int i = 0; i < 3; ++i ) {
		out.x += r[i] * axis[i][0];
		out.y += r[i] * axis[i][1];
		out.z += r[i] * axis[i][2];
	}
	return out;
}

/* The item a model label shows is looked up again on every frame in the
 * second half of each real-time second (cls.realtime % 1000 > 500,
 * 0x0041f0d5..0x0041f0e8), and kept between; so a weapon change shows within
 * half a second, and a HUD just shown draws no model until the first look. */
inline bool ItemLookupWindow( int realtimeMs ) {
	return realtimeMs % 1000 > 500;
}

/* ---- which HUD menus show ------------------------------------------------ */

/* The menus by the names their files declare -- two of which are crossed:
 * ui/weaphud.urc declares "dualweaphud", with one model for the two-handed
 * weapon, and ui/dualweaphud.urc declares "weaphud", with one for each hand. */
inline constexpr const char *HealthHud = "healthhud";
inline constexpr const char *WeaponHud = "weaphud";
inline constexpr const char *DualWeaponHud = "dualweaphud";
inline constexpr const char *BossHud = "bosshud";

struct Selection {
	bool health = false;
	bool weapon = false;
	bool dualWeapon = false;
	bool boss = false;
};

/* F.A.K.K.2's engine's UI update chooses among them every frame the HUD
 * shows (fakk2.exe 1.02 0x0040b2d7..0x0040b326, which looks the four menus up
 * by these names at 0x0040f0fe..0x0040f312): the health and water bars
 * always; the two-handed weapon's menu while activeItems[4] holds an item,
 * which the game fills only for a two-handed weapon, and otherwise the
 * one-handed menu, whether or not a hand holds anything, so its two empty
 * rings show from the start; the boss's bar while stat 13 is above zero, as
 * the game keeps it while the boss lives. */
inline Selection Select( const int *stats, const int *activeItems ) {
	Selection s;
	s.health = true;
	s.dualWeapon = ReadSlot( activeItems, ItemWeaponDual ) >= 0;
	s.weapon = !s.dualWeapon;
	s.boss = ReadSlot( stats, StatBossHealth ) > 0;
	return s;
}

} // namespace hud
} // namespace fakk2
} // namespace fntech3

#endif
